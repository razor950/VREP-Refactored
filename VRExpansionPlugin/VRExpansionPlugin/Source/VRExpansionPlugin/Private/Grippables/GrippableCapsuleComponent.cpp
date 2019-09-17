// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

// Parent Header
#include "Grippables/GrippableCapsuleComponent.h"

// Unreal
#include "Net/UnrealNetwork.h"

// VREP



// UGrippableCapsuleComponent

// Public

// Constructor & Destructor

  //=============================================================================
UGrippableCapsuleComponent::UGrippableCapsuleComponent(const FObjectInitializer& ObjectInitializer) : 
	Super                          (ObjectInitializer),
	bRepGripSettingsAndGameplayTags(true             ),
	bReplicateMovement             (false            )
{
	VRGripInterfaceSettings.bDenyGripping           = false                                                    ;
	VRGripInterfaceSettings.bIsHeld                 = false                                                    ;
	VRGripInterfaceSettings.bSimulateOnDrop         = true                                                     ;
	VRGripInterfaceSettings.ConstraintBreakDistance = 100.0f                                                   ;
	VRGripInterfaceSettings.ConstraintDamping       = 200.0f                                                   ;
	VRGripInterfaceSettings.ConstraintStiffness     = 1500.0f                                                  ;
	VRGripInterfaceSettings.FreeDefaultGripType     = EGripCollisionType::ManipulationGrip                     ;
	VRGripInterfaceSettings.LateUpdateSetting       = EGripLateUpdateSettings::LateUpdatesAlwaysOff            ;
	VRGripInterfaceSettings.OnTeleportBehavior      = EGripInterfaceTeleportBehavior::DropOnTeleport           ;
	VRGripInterfaceSettings.MovementReplicationType = EGripMovementReplicationSettings::ForceClientSideMovement;
	VRGripInterfaceSettings.PrimarySlotRange        = 20.0f                                                    ;
	VRGripInterfaceSettings.SecondaryGripType       = ESecondaryGripType::SG_None                              ;
	VRGripInterfaceSettings.SecondarySlotRange      = 20.0f                                                    ;
	VRGripInterfaceSettings.SlotDefaultGripType     = EGripCollisionType::ManipulationGrip                     ;


	// Moved to direct initialization -Ed.
	bReplicateMovement = false;
	//this->bReplicates = true;

	bRepGripSettingsAndGameplayTags = true;
}

UGrippableCapsuleComponent::~UGrippableCapsuleComponent()
{}


// Functions

// UCapsuleComponent Overloads

void UGrippableCapsuleComponent::BeginPlay()
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

	bOriginalReplicatesMovement = bReplicateMovement;
}

void UGrippableCapsuleComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Call the base class 
	Super::EndPlay(EndPlayReason);

	// Call all grip scripts begin play events so they can perform any needed logic
	for (UVRGripScriptBase* Script : GripLogicScripts)
	{
		if (Script)
		{
			Script->EndPlay(EndPlayReason);
		}
	}
}

void UGrippableCapsuleComponent::GetSubobjectsWithStableNamesForNetworking(TArray<UObject*>& ObjList)
{
	for (int32 scriptIndex = 0; scriptIndex < GripLogicScripts.Num(); ++scriptIndex)
	{
		if (UObject* SubObject = GripLogicScripts[scriptIndex])
		{
			ObjList.Add(SubObject);
		}
	}
}

void UGrippableCapsuleComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	// Call the super at the end, after we've done what we needed to do.
	Super::OnComponentDestroyed(bDestroyingHierarchy);

	// Don't set these in editor preview window and the like, it causes saving issues.
	if (UWorld* World = GetWorld())
	{
		EWorldType::Type WorldType = World->WorldType;

		if (WorldType == EWorldType::Editor || WorldType == EWorldType::EditorPreview)
		{
			return;
		}
	}

	for (int32 scriptIndex = 0; scriptIndex < GripLogicScripts.Num(); scriptIndex++)
	{
		if (UObject* SubObject = GripLogicScripts[scriptIndex])
		{
			SubObject->MarkPendingKill();
		}
	}

	GripLogicScripts.Empty();
}

void UGrippableCapsuleComponent::PreDestroyFromReplication()
{
	Super::PreDestroyFromReplication();

	// Destroy any sub-objects we created.
	for (int32 scriptIndex = 0; scriptIndex < GripLogicScripts.Num(); ++scriptIndex)
	{
		if (UObject *SubObject = GripLogicScripts[scriptIndex])
		{
			SubObject->PreDestroyFromReplication();
			SubObject->MarkPendingKill          ();
		}
	}

	GripLogicScripts.Empty();
}

void UGrippableCapsuleComponent::PreReplication(IRepChangedPropertyTracker & ChangedPropertyTracker)
{
	Super::PreReplication(ChangedPropertyTracker);

	// Don't replicate if set to not do it
	DOREPLIFETIME_ACTIVE_OVERRIDE(UGrippableCapsuleComponent, VRGripInterfaceSettings, bRepGripSettingsAndGameplayTags);
	DOREPLIFETIME_ACTIVE_OVERRIDE(UGrippableCapsuleComponent, GameplayTags           , bRepGripSettingsAndGameplayTags);

	DOREPLIFETIME_ACTIVE_OVERRIDE(USceneComponent, RelativeLocation, bReplicateMovement);
	DOREPLIFETIME_ACTIVE_OVERRIDE(USceneComponent, RelativeRotation, bReplicateMovement);
	DOREPLIFETIME_ACTIVE_OVERRIDE(USceneComponent, RelativeScale3D, bReplicateMovement);
}

bool UGrippableCapsuleComponent::ReplicateSubobjects(UActorChannel* Channel, class FOutBunch*Bunch, FReplicationFlags* RepFlags)
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

// IVRGripInterface Implementation

FBPAdvGripSettings UGrippableCapsuleComponent::AdvancedGripSettings_Implementation()
{
	return VRGripInterfaceSettings.AdvancedGripSettings;
}

bool UGrippableCapsuleComponent::AllowsMultipleGrips_Implementation()
{
	return VRGripInterfaceSettings.bAllowMultipleGrips;
}

void UGrippableCapsuleComponent::ClosestGripSlotInRange_Implementation
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

	UVRExpansionFunctionLibrary::GetGripSlotInRangeByTypeName_Component
	(
		OverridePrefix                                                                                        , 
		this                                                                                                  , 
		WorldLocation                                                                                         , 
		bSecondarySlot ? VRGripInterfaceSettings.SecondarySlotRange : VRGripInterfaceSettings.PrimarySlotRange, 
		bHadSlotInRange                                                                                       ,
		SlotWorldTransform
	);
}

bool UGrippableCapsuleComponent::DenyGripping_Implementation()
{
	return VRGripInterfaceSettings.bDenyGripping;
}

bool UGrippableCapsuleComponent::GetGripScripts_Implementation(TArray<UVRGripScriptBase*>& ArrayReference)
{
	ArrayReference = GripLogicScripts;
	
	return GripLogicScripts.Num() > 0;
}

void UGrippableCapsuleComponent::GetGripStiffnessAndDamping_Implementation(float& GripStiffnessOut, float& GripDampingOut)
{
	GripStiffnessOut = VRGripInterfaceSettings.ConstraintStiffness;
	GripDampingOut   = VRGripInterfaceSettings.ConstraintDamping  ;
}

EGripCollisionType UGrippableCapsuleComponent::GetPrimaryGripType_Implementation(bool bIsSlot)
{
	return bIsSlot ? VRGripInterfaceSettings.SlotDefaultGripType : VRGripInterfaceSettings.FreeDefaultGripType;
}

float UGrippableCapsuleComponent::GripBreakDistance_Implementation()
{
	return VRGripInterfaceSettings.ConstraintBreakDistance;
}

EGripLateUpdateSettings UGrippableCapsuleComponent::GripLateUpdateSetting_Implementation()
{
	return VRGripInterfaceSettings.LateUpdateSetting;
}

EGripMovementReplicationSettings UGrippableCapsuleComponent::GripMovementReplicationType_Implementation()
{
	return VRGripInterfaceSettings.MovementReplicationType;
}

void UGrippableCapsuleComponent::IsHeld_Implementation(TArray<FBPGripPair>& HoldingControllers, bool& bIsHeld)
{
	HoldingControllers = VRGripInterfaceSettings.HoldingControllers;
	bIsHeld            = VRGripInterfaceSettings.bIsHeld           ;
}

ESecondaryGripType UGrippableCapsuleComponent::SecondaryGripType_Implementation()
{
	return VRGripInterfaceSettings.SecondaryGripType;
}

void UGrippableCapsuleComponent::SetDenyGripping(bool bDenyGripping)
{
	VRGripInterfaceSettings.bDenyGripping = bDenyGripping;
}

void UGrippableCapsuleComponent::SetHeld_Implementation(UGripMotionControllerComponent* HoldingController, uint8 GripID, bool bIsHeld)
{
	if (bIsHeld)
	{
		if (VRGripInterfaceSettings.MovementReplicationType != EGripMovementReplicationSettings::ForceServerSideMovement)
		{
			if (!VRGripInterfaceSettings.bIsHeld)
			{
				bOriginalReplicatesMovement = bReplicateMovement;
			}

			bReplicateMovement = false;
		}

		VRGripInterfaceSettings.HoldingControllers.AddUnique(FBPGripPair(HoldingController, GripID));
	}
	else
	{
		if (VRGripInterfaceSettings.MovementReplicationType != EGripMovementReplicationSettings::ForceServerSideMovement)
		{
			bReplicateMovement = bOriginalReplicatesMovement;
		}

		VRGripInterfaceSettings.HoldingControllers.Remove(FBPGripPair(HoldingController, GripID));
	}

	VRGripInterfaceSettings.bIsHeld = VRGripInterfaceSettings.HoldingControllers.Num() > 0;
}

bool UGrippableCapsuleComponent::SimulateOnDrop_Implementation()
{
	return VRGripInterfaceSettings.bSimulateOnDrop;
}

EGripInterfaceTeleportBehavior UGrippableCapsuleComponent::TeleportBehavior_Implementation()
{
	return VRGripInterfaceSettings.OnTeleportBehavior;
}

// Event functions

void UGrippableCapsuleComponent::OnChildGrip_Implementation       (UGripMotionControllerComponent* GrippingController , const FBPActorGripInformation& GripInformation                   ) {}
void UGrippableCapsuleComponent::OnChildGripRelease_Implementation(UGripMotionControllerComponent* ReleasingController, const FBPActorGripInformation& GripInformation, bool bWasSocketed) {}

void UGrippableCapsuleComponent::OnGrip_Implementation       (UGripMotionControllerComponent* GrippingController , const FBPActorGripInformation& GripInformation                   ) {}
void UGrippableCapsuleComponent::OnGripRelease_Implementation(UGripMotionControllerComponent* ReleasingController, const FBPActorGripInformation& GripInformation, bool bWasSocketed) {}

void UGrippableCapsuleComponent::OnSecondaryGrip_Implementation       (USceneComponent* SecondaryGripComponent         , const FBPActorGripInformation& GripInformation) {}
void UGrippableCapsuleComponent::OnSecondaryGripRelease_Implementation(USceneComponent* ReleasingSecondaryGripComponent, const FBPActorGripInformation& GripInformation) {}

bool UGrippableCapsuleComponent::RequestsSocketing_Implementation(USceneComponent*&              ParentToSocketTo   ,       FName&                   OptionalSocketName, FTransform_NetQuantize& RelativeTransform) { return false; }
void UGrippableCapsuleComponent::TickGrip_Implementation         (UGripMotionControllerComponent* GrippingController, const FBPActorGripInformation& GripInformation   , float DeltaTime                          ) {}

//  Interaction Functions

void UGrippableCapsuleComponent::OnEndSecondaryUsed_Implementation() {}
void UGrippableCapsuleComponent::OnEndUsed_Implementation         () {}

void UGrippableCapsuleComponent::OnInput_Implementation(FKey Key, EInputEvent KeyEvent) {}

void UGrippableCapsuleComponent::OnSecondaryUsed_Implementation() {}
void UGrippableCapsuleComponent::OnUsed_Implementation         () {}


// Only in cpp (Ed).

void UGrippableCapsuleComponent::GetLifetimeReplicatedProps(TArray<class FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	/*_CONDITION*/
	DOREPLIFETIME(UGrippableCapsuleComponent, GripLogicScripts               );   // , COND_Custom);
	DOREPLIFETIME(UGrippableCapsuleComponent, bRepGripSettingsAndGameplayTags);
	DOREPLIFETIME(UGrippableCapsuleComponent, bReplicateMovement             );
	
	DOREPLIFETIME_CONDITION(UGrippableCapsuleComponent, VRGripInterfaceSettings, COND_Custom);
	DOREPLIFETIME_CONDITION(UGrippableCapsuleComponent, GameplayTags           , COND_Custom);
}



/*FBPInteractionSettings UGrippableCapsuleComponent::GetInteractionSettings_Implementation()
{
	return VRGripInterfaceSettings.InteractionSettings;
}*/
