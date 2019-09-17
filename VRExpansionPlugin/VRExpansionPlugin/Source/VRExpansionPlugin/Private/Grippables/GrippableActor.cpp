// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

// Parent Header
#include "Grippables/GrippableActor.h"

// Unreal
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

// VREP



// AGRippableActor

// Public

// Constructor & Destructor

//=============================================================================
AGrippableActor::AGrippableActor(const FObjectInitializer& ObjectInitializer) : 
	Super                          (),
	bAllowIgnoringAttachOnOwner(true)
{
	VRGripInterfaceSettings.bDenyGripping           = false                                                    ;
	VRGripInterfaceSettings.bIsHeld                 = false                                                    ;
	VRGripInterfaceSettings.bSimulateOnDrop         = true                                                     ;
	VRGripInterfaceSettings.ConstraintBreakDistance = 100.0f                                                   ;
	VRGripInterfaceSettings.ConstraintDamping       = 200.0f                                                   ;
	VRGripInterfaceSettings.ConstraintStiffness     = 1500.0f                                                  ;
	VRGripInterfaceSettings.FreeDefaultGripType     = EGripCollisionType::InteractiveCollisionWithPhysics      ;
	VRGripInterfaceSettings.LateUpdateSetting       = EGripLateUpdateSettings::NotWhenCollidingOrDoubleGripping;
	VRGripInterfaceSettings.MovementReplicationType = EGripMovementReplicationSettings::ForceClientSideMovement;
	VRGripInterfaceSettings.OnTeleportBehavior      = EGripInterfaceTeleportBehavior::TeleportAllComponents    ;
	VRGripInterfaceSettings.PrimarySlotRange        = 20.0f                                                    ;   
	VRGripInterfaceSettings.SecondaryGripType       = ESecondaryGripType::SG_None                              ;
	VRGripInterfaceSettings.SecondarySlotRange      = 20.0f                                                    ;
	VRGripInterfaceSettings.SlotDefaultGripType     = EGripCollisionType::InteractiveCollisionWithPhysics      ;

	// Default replication on for multiplayer.
  //this->bNetLoadOnClient   = false;
	this->bReplicateMovement = true;
	this->bReplicates        = true;
	
  //bAllowIgnoringAttachOnOwner     = true;   Moved to direct initialization.
	bRepGripSettingsAndGameplayTags = true;

	// Setting a minimum of every 3rd frame (VR 90fps) for replication consideration.
	// Otherwise we will get some massive slow downs if the replication is allowed to hit the 2 per second minimum default.
	MinNetUpdateFrequency = 30.0f;
}

AGrippableActor::~AGrippableActor()
{}

// Functions



// ------------------------------------------------
// Client Auth Throwing Data and functions 
// ------------------------------------------------

void AGrippableActor::CeaseReplicationBlocking()
{
	ClientAuthReplicationData.bIsCurrentlyClientAuth = false;

	if (UWorld* OurWorld = GetWorld())
	{
		OurWorld->GetTimerManager().ClearTimer(ClientAuthReplicationData.ResetReplicationHandle);
	}
}

bool AGrippableActor::PollReplicationEvent()
{
	if (!ClientAuthReplicationData.bIsCurrentlyClientAuth)
	{
		return false;
	}

	UWorld* OurWorld = GetWorld();

	if (!OurWorld)
	{
		return false;
	}

	if ((OurWorld->GetTimeSeconds() - ClientAuthReplicationData.TimeAtInitialThrow) > 10.0f)
	{
		// Lets time out sending, its been 10 seconds since we threw the object and its likely that it is conflicting with some server.
		// Authed movement that is forcing it to keep momentum.
		return false;
	}

	// Store current transform for resting check.
	FTransform CurTransform = this->GetActorTransform();

	if (!CurTransform.GetRotation().Equals(ClientAuthReplicationData.LastActorTransform.GetRotation()) || !CurTransform.GetLocation().Equals(ClientAuthReplicationData.LastActorTransform.GetLocation()))
	{
		ClientAuthReplicationData.LastActorTransform = CurTransform;

		if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(RootComponent))
		{
			// Need to clamp to a max time since start, to handle cases with conflicting collisions.
			if (PrimComp->IsSimulatingPhysics() && ShouldWeSkipAttachmentReplication())
			{
				FRepMovementVR ClientAuthMovementRep;

				if (ClientAuthMovementRep.GatherActorsMovement(this))
				{
					Server_GetClientAuthReplication(ClientAuthMovementRep);

					if (PrimComp->RigidBodyIsAwake())
					{
						return true;
					}
				}
			}
		}
		else
		{
			return false;
		}
	}
	else
	{
		// Difference is too small, lets end sending location.
		ClientAuthReplicationData.LastActorTransform = FTransform::Identity;
	}

	AActor* TopOwner = GetOwner();

	if (TopOwner != nullptr)
	{
		AActor* tempOwner = TopOwner->GetOwner();

		// I have an owner so search that for the top owner
		while (tempOwner)
		{
			TopOwner  = tempOwner           ;
			tempOwner = TopOwner->GetOwner();
		}

		if (APlayerController* PlayerController = Cast<APlayerController>(TopOwner))
		{
			if (APlayerState* PlayerState = PlayerController->PlayerState)
			{
				if (ClientAuthReplicationData.ResetReplicationHandle.IsValid())
				{
					OurWorld->GetTimerManager().ClearTimer(ClientAuthReplicationData.ResetReplicationHandle);
				}

				// Lets clamp the ping to a min / max value just in case.
				float clampedPing = FMath::Clamp(PlayerState->ExactPing, 0.0f, 1000.0f);

				OurWorld->GetTimerManager().SetTimer(ClientAuthReplicationData.ResetReplicationHandle, this, &AGrippableActor::CeaseReplicationBlocking, clampedPing, false);
			}
		}
	}

	return false;
}

void AGrippableActor::Server_GetClientAuthReplication_Implementation(const FRepMovementVR& newMovement)
{
	newMovement.CopyTo(ReplicatedMovement);

	OnRep_ReplicatedMovement();
}

bool AGrippableActor::Server_GetClientAuthReplication_Validate(const FRepMovementVR& newMovement)
{
	return true;
}

// End client auth throwing data and functions. //

// Actor Overloads

// On Destroy clean up our objects.
void AGrippableActor::BeginDestroy()
{
	Super::BeginDestroy();

	for (int32 scriptIndex = 0; scriptIndex < GripLogicScripts.Num(); scriptIndex++)
	{
		if (UObject *SubObject = GripLogicScripts[scriptIndex])
		{
			SubObject->MarkPendingKill();
		}
	}

	GripLogicScripts.Empty();
}

void AGrippableActor::BeginPlay()
{
	// Call the base class.
	Super::BeginPlay();

	// Call all grip scripts begin play events so they can perform any needed logic.
	for (UVRGripScriptBase* Script : GripLogicScripts)
	{
		if (Script)
		{
			Script->BeginPlay(this);
		}
	}
}

void AGrippableActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (ClientAuthReplicationData.bIsCurrentlyClientAuth)
	{
		GEngine->GetEngineSubsystem<UBucketUpdateSubsystem>()->RemoveObjectFromBucketByFunctionName(this, FName(TEXT("PollReplicationEvent")));
	}

	CeaseReplicationBlocking();

	// Call all grip scripts begin play events so they can perform any needed logic.
	for (UVRGripScriptBase* Script : GripLogicScripts)
	{
		if (Script)
		{
			Script->EndPlay(EndPlayReason);
		}
	}

	Super::EndPlay(EndPlayReason);
}

// This isn't called very many places but it does come up.
void AGrippableActor::MarkComponentsAsPendingKill()
{
	Super::MarkComponentsAsPendingKill();

	for (int32 scriptIndex = 0; scriptIndex < GripLogicScripts.Num(); ++scriptIndex)
	{
		if (UObject* SubObject = GripLogicScripts[scriptIndex])
		{
			SubObject->MarkPendingKill();
		}
	}

	GripLogicScripts.Empty();
}

void AGrippableActor::OnRep_AttachmentReplication()
{
	if (bAllowIgnoringAttachOnOwner && ShouldWeSkipAttachmentReplication())
	{
		return;
	}

	// None of our overrides are required, lets just pass it on now.
	Super::OnRep_AttachmentReplication();
}

void AGrippableActor::OnRep_ReplicateMovement()
{
	if (bAllowIgnoringAttachOnOwner && ShouldWeSkipAttachmentReplication())
	{
		return;
	}

	if (RootComponent)
	{
		const FRepAttachment ReplicationAttachment = GetAttachmentReplication();

		if (!ReplicationAttachment.AttachParent)
		{
			/*
			This "fix" corrects the simulation state not replicating over correctly.
			If you turn off movement replication, simulate an object, turn movement replication back on and un-simulate, it never knows the difference.
			This change ensures that it is checking against the current state.
			*/
			if (RootComponent->IsSimulatingPhysics() != ReplicatedMovement.bRepPhysics)//SavedbRepPhysics != ReplicatedMovement.bRepPhysics)
			{
				// Turn on/off physics sim to match server.
				SyncReplicatedPhysicsSimulation();

				// It doesn't really hurt to run it here, the super can call it again but it will fail out as they already match.
			}
		}
	}

	Super::OnRep_ReplicateMovement();
}

void AGrippableActor::OnRep_ReplicatedMovement()
{
	if (ClientAuthReplicationData.bIsCurrentlyClientAuth && ShouldWeSkipAttachmentReplication())
	{
		return;
	}

	Super::OnRep_ReplicatedMovement();
}

/** Called right before being marked for destruction due to network replication */
// Clean up our objects so that they aren't sitting around for GC.
void AGrippableActor::PreDestroyFromReplication()
{
	Super::PreDestroyFromReplication();

	// Destroy any sub-objects we created
	for (int32 scriptIndex = 0; scriptIndex < GripLogicScripts.Num(); ++scriptIndex)
	{
		if (UObject* SubObject = GripLogicScripts[scriptIndex])
		{
			OnSubobjectDestroyFromReplication(SubObject); //-V595

			SubObject->PreDestroyFromReplication();
			SubObject->MarkPendingKill          ();
		}
	}

	for (UActorComponent* ActorComp : GetComponents())
	{
		// Pending kill components should have already had this called as they were network spawned and are being killed.
		if (ActorComp && !ActorComp->IsPendingKill() && ActorComp->GetClass()->ImplementsInterface(UVRGripInterface::StaticClass()))
		{
			ActorComp->PreDestroyFromReplication();
		}
	}

	GripLogicScripts.Empty();
}

void AGrippableActor::PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker)
{
	Super::PreReplication(ChangedPropertyTracker);

	// Don't replicate if set to not do it.
	DOREPLIFETIME_ACTIVE_OVERRIDE(AGrippableActor, VRGripInterfaceSettings, bRepGripSettingsAndGameplayTags);
	DOREPLIFETIME_ACTIVE_OVERRIDE(AGrippableActor, GameplayTags           , bRepGripSettingsAndGameplayTags);
}

void AGrippableActor::PostNetReceivePhysicState()
{
	if (VRGripInterfaceSettings.bIsHeld && bAllowIgnoringAttachOnOwner && ShouldWeSkipAttachmentReplication())
	{
		return;
	}

	Super::PostNetReceivePhysicState();
}

bool AGrippableActor::ReplicateSubobjects(UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags)
{
	bool WroteSomething = Super::ReplicateSubobjects(Channel, Bunch, RepFlags);

	for (UVRGripScriptBase* Script : GripLogicScripts)
	{
		if (Script && !Script->IsPendingKill())
		{
			WroteSomething |= Channel->ReplicateSubobject(Script, *Bunch, *RepFlags);
		}
	}

	return WroteSomething;
}

// IVRGripInterface Implementation.

bool AGrippableActor::AllowsMultipleGrips_Implementation()
{
	return VRGripInterfaceSettings.bAllowMultipleGrips;
}

FBPAdvGripSettings AGrippableActor::AdvancedGripSettings_Implementation()
{
	return VRGripInterfaceSettings.AdvancedGripSettings;
}

void AGrippableActor::ClosestGripSlotInRange_Implementation
(
	FVector                         WorldLocation     , 
	bool                            bSecondarySlot    , 
	bool&                           bHadSlotInRange   , 
	FTransform&                     SlotWorldTransform, 
	UGripMotionControllerComponent* CallingController , 
	FName                           OverridePrefix
)
{
	if (OverridePrefix.IsNone())
	{
		bSecondarySlot ? OverridePrefix = "VRGripS" : OverridePrefix = "VRGripP";
	}

	UVRExpansionFunctionLibrary::GetGripSlotInRangeByTypeName
	(
		OverridePrefix                                                                                        , 
		this                                                                                                  , 
		WorldLocation                                                                                         , 
		bSecondarySlot ? VRGripInterfaceSettings.SecondarySlotRange : VRGripInterfaceSettings.PrimarySlotRange, 
		bHadSlotInRange                                                                                       , 
		SlotWorldTransform
	);
}

bool AGrippableActor::DenyGripping_Implementation()
{
	return VRGripInterfaceSettings.bDenyGripping;
}

bool AGrippableActor::GetGripScripts_Implementation(TArray<UVRGripScriptBase*>& ArrayReference)
{
	ArrayReference = GripLogicScripts;

	return GripLogicScripts.Num() > 0;
}

void AGrippableActor::GetGripStiffnessAndDamping_Implementation(float& GripStiffnessOut, float& GripDampingOut)
{
	GripStiffnessOut = VRGripInterfaceSettings.ConstraintStiffness;
	GripDampingOut   = VRGripInterfaceSettings.ConstraintDamping  ;
}

float AGrippableActor::GripBreakDistance_Implementation()
{
	return VRGripInterfaceSettings.ConstraintBreakDistance;
}

EGripLateUpdateSettings AGrippableActor::GripLateUpdateSetting_Implementation()
{
	return VRGripInterfaceSettings.LateUpdateSetting;
}

EGripMovementReplicationSettings AGrippableActor::GripMovementReplicationType_Implementation()
{
	return VRGripInterfaceSettings.MovementReplicationType;
}

EGripCollisionType AGrippableActor::GetPrimaryGripType_Implementation(bool bIsSlot)
{
	return bIsSlot ? VRGripInterfaceSettings.SlotDefaultGripType : VRGripInterfaceSettings.FreeDefaultGripType;
}

void AGrippableActor::IsHeld_Implementation(TArray<FBPGripPair>& HoldingControllers, bool& bIsHeld)
{
	HoldingControllers = VRGripInterfaceSettings.HoldingControllers;
	bIsHeld            = VRGripInterfaceSettings.bIsHeld           ;
}

bool AGrippableActor::RequestsSocketing_Implementation(USceneComponent*& ParentToSocketTo, FName& OptionalSocketName, FTransform_NetQuantize& RelativeTransform)
{ 
	return false; 
}

ESecondaryGripType AGrippableActor::SecondaryGripType_Implementation()
{
	return VRGripInterfaceSettings.SecondaryGripType;
}

void AGrippableActor::SetDenyGripping(bool bDenyGripping)
{
	VRGripInterfaceSettings.bDenyGripping = bDenyGripping;
}

void AGrippableActor::SetHeld_Implementation(UGripMotionControllerComponent* HoldingController, uint8 GripID, bool bIsHeld)
{
	if (bIsHeld)
	{
		VRGripInterfaceSettings.HoldingControllers.AddUnique(FBPGripPair(HoldingController, GripID));

		if (ClientAuthReplicationData.bIsCurrentlyClientAuth)
		{
			GEngine->GetEngineSubsystem<UBucketUpdateSubsystem>()->RemoveObjectFromBucketByFunctionName(this, FName(TEXT("PollReplicationEvent")));

			CeaseReplicationBlocking();
		}
	}
	else
	{
		VRGripInterfaceSettings.HoldingControllers.Remove(FBPGripPair(HoldingController, GripID));

		if (ClientAuthReplicationData.bUseClientAuthThrowing && ShouldWeSkipAttachmentReplication())
		{
			if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(GetRootComponent()))
			{
				if (PrimComp->IsSimulatingPhysics())
				{
					// The subsystem automatically removes entries with the same function signature so its safe to just always add here
					GEngine->GetEngineSubsystem<UBucketUpdateSubsystem>()->AddObjectToBucket(ClientAuthReplicationData.UpdateRate, this, FName(TEXT("PollReplicationEvent")));

					ClientAuthReplicationData.bIsCurrentlyClientAuth = true;

					if (UWorld * World = GetWorld())
					{
						ClientAuthReplicationData.TimeAtInitialThrow = World->GetTimeSeconds();
					}
				}
			}
		}
	}

	VRGripInterfaceSettings.bIsHeld = VRGripInterfaceSettings.HoldingControllers.Num() > 0;
}

bool AGrippableActor::SimulateOnDrop_Implementation()
{
	return VRGripInterfaceSettings.bSimulateOnDrop;
}

EGripInterfaceTeleportBehavior AGrippableActor::TeleportBehavior_Implementation()
{
	return VRGripInterfaceSettings.OnTeleportBehavior;
}

void AGrippableActor::OnChildGrip_Implementation       (UGripMotionControllerComponent* GrippingController , const FBPActorGripInformation& GripInformation                   ) {}
void AGrippableActor::OnChildGripRelease_Implementation(UGripMotionControllerComponent* ReleasingController, const FBPActorGripInformation& GripInformation, bool bWasSocketed) {}

void AGrippableActor::OnGrip_Implementation       (UGripMotionControllerComponent* GrippingController , const FBPActorGripInformation& GripInformation                   ) {}
void AGrippableActor::OnGripRelease_Implementation(UGripMotionControllerComponent* ReleasingController, const FBPActorGripInformation& GripInformation, bool bWasSocketed) {}

void AGrippableActor::OnSecondaryGrip_Implementation       (USceneComponent* SecondaryGripComponent         , const FBPActorGripInformation& GripInformation) {}
void AGrippableActor::OnSecondaryGripRelease_Implementation(USceneComponent* ReleasingSecondaryGripComponent, const FBPActorGripInformation& GripInformation) {}

void AGrippableActor::TickGrip_Implementation(UGripMotionControllerComponent* GrippingController, const FBPActorGripInformation& GripInformation, float DeltaTime) {}

// Interaction Functions

void AGrippableActor::OnEndSecondaryUsed_Implementation() {}
void AGrippableActor::OnEndUsed_Implementation         () {}

void AGrippableActor::OnInput_Implementation(FKey Key, EInputEvent KeyEvent) {}

void AGrippableActor::OnSecondaryUsed_Implementation() {}
void AGrippableActor::OnUsed_Implementation         () {}


// CPP only... (IDK why). -Ed.

void AGrippableActor::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	/*_CONDITION*/
	DOREPLIFETIME(AGrippableActor, GripLogicScripts               );   // , COND_Custom);
	DOREPLIFETIME(AGrippableActor, bRepGripSettingsAndGameplayTags);
	DOREPLIFETIME(AGrippableActor, bAllowIgnoringAttachOnOwner    );
	DOREPLIFETIME(AGrippableActor, ClientAuthReplicationData      );

	DOREPLIFETIME_CONDITION(AGrippableActor, VRGripInterfaceSettings, COND_Custom);
	DOREPLIFETIME_CONDITION(AGrippableActor, GameplayTags           , COND_Custom);
}


/*FBPInteractionSettings AGrippableActor::GetInteractionSettings_Implementation()
{
	return VRGripInterfaceSettings.InteractionSettings;
}*/