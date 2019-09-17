// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

// Includes

// Unreal
#include "CoreMinimal.h"
#include "Animation/SkeletalMeshActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/ActorChannel.h"
#include "GameplayTagContainer.h"
#include "GameplayTagAssetInterface.h"

// VREP
#include "Grippables/GrippablePhysicsReplication.h"
#include "GripScripts/VRGripScriptBase.h"
#include "Misc/BucketUpdateSubsystem.h"
#include "VRGripInterface.h"
#include "VRBPDatatypes.h"
#include "VRExpansionFunctionLibrary.h"

// UHeader Tool
#include "GrippableSkeletalMeshActor.generated.h"



// Classes

/**
* A component specifically for being able to turn off movement replication in the component at will.
* Has the upside of also being a blueprintable base since UE4 doesn't allow that with std ones.
*/
UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent), ClassGroup = (VRExpansionPlugin))
class VREXPANSIONPLUGIN_API UOptionalRepSkeletalMeshComponent : public USkeletalMeshComponent
{
	GENERATED_BODY()

public:

	// Constructor

	UOptionalRepSkeletalMeshComponent(const FObjectInitializer& ObjectInitializer);


	// Functions

	virtual void PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker) override;


	// Declares

	// Overrides the default of : true and allows for controlling it like in an actor, should be default of off normally with grippable components.
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category = "Component Replication")
		bool bReplicateMovement;
};

/**
*
*/
UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent), ClassGroup = (VRExpansionPlugin))
class VREXPANSIONPLUGIN_API AGrippableSkeletalMeshActor : 
	// Parent
	public ASkeletalMeshActor, 

	// Interfaces
	public IGameplayTagAssetInterface, public IVRGripInterface
{
	GENERATED_BODY()

public:

	// Constructor & Destructor

	AGrippableSkeletalMeshActor(const FObjectInitializer& ObjectInitializer);

	~AGrippableSkeletalMeshActor();


	// Functions

	// Should we skip attachment replication (vr settings say we are a client auth grip and our owner is locally controlled).
	inline bool ShouldWeSkipAttachmentReplication() const
	{
		if (VRGripInterfaceSettings.MovementReplicationType == EGripMovementReplicationSettings::ClientSide_Authoritive ||
			VRGripInterfaceSettings.MovementReplicationType == EGripMovementReplicationSettings::ClientSide_Authoritive_NoRep)
		{
			return HasLocalNetOwner();
			//const APawn* MyPawn = Cast<APawn>(GetOwner());
			//return (MyPawn ? MyPawn->IsLocallyControlled() : false);
		}
		else
			return false;
	}

	// ASkeletalMeshActor Overloads

	// On Destroy clean up our objects
	virtual void BeginDestroy                                        () override;
	virtual void BeginPlay                                           () override;
	virtual void EndPlay     (const EEndPlayReason::Type EndPlayReason) override;

	virtual void MarkComponentsAsPendingKill() override;   // This isn't called very many places but it does come up.

	virtual void OnRep_AttachmentReplication() override;   // Fix bugs with replication and bReplicateMovement
	virtual void OnRep_ReplicateMovement    () override;
	virtual void OnRep_ReplicatedMovement   () override;

	/** Called right before being marked for destruction due to network replication. */
	// Clean up our objects so that they aren't sitting around for GC.
	virtual void PreDestroyFromReplication() override;

	virtual void PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker) override;

	void PostNetReceivePhysicState() override;

	bool ReplicateSubobjects(UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags) override;

	// Debug printing of when the object is replication destroyed
	/*virtual void OnSubobjectDestroyFromReplication(UObject *Subobject) override
	{
	Super::OnSubobjectDestroyFromReplication(Subobject);

	GEngine->AddOnScreenDebugMessage(-1, 15.f, FColor::Red, FString::Printf(TEXT("Killed Object On Actor: x: %s"), *Subobject->GetName()));
	}*/

	// ------------------------------------------------
	// Client Auth Throwing Data and functions 
	// ------------------------------------------------

	UFUNCTION(Category = "Networking")
		void CeaseReplicationBlocking();

	UFUNCTION()	bool PollReplicationEvent();
	
	// Notify the server that we locally gripped something.
	UFUNCTION(UnReliable, Server, WithValidation, Category = "Networking")
		void Server_GetClientAuthReplication(const FRepMovementVR& newMovement);

	// End client auth throwing data and functions. //

	// ------------------------------------------------
	// Gameplay tag interface
	// ------------------------------------------------

	/** Overridden to return requirements tags. */
	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override
	{
		TagContainer = GameplayTags;
	}

	// End Gameplay Tag Interface

	// IVRGripInterface Implementation

	// Get the advanced physics settings for this grip.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VRGripInterface")
		FBPAdvGripSettings AdvancedGripSettings();

	// Check if an object allows multiple grips at one time.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VRGripInterface")
		bool AllowsMultipleGrips();

	// Get closest primary slot in range.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VRGripInterface")
		void ClosestGripSlotInRange
		(
			FVector WorldLocation, 
			bool bSecondarySlot, 
			bool& bHadSlotInRange, 
			FTransform& SlotWorldTransform, 
			UGripMotionControllerComponent* CallingController = nullptr, 
			FName OverridePrefix = NAME_None
		);

	// Set up as deny instead of allow so that default allows for gripping.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VRGripInterface", meta = (DisplayName = "IsDenyingGrips"))
		bool DenyGripping();

	// Get grip scripts.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VRGripInterface")
		bool GetGripScripts(TArray<UVRGripScriptBase*>& ArrayReference);

	// What grip stiffness and damping to use if using a physics constraint.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VRGripInterface")
		void GetGripStiffnessAndDamping(float& GripStiffnessOut, float& GripDampingOut);

	// What distance to break a grip at (only relevant with physics enabled grips.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VRGripInterface")
		float GripBreakDistance();

	// Define the late update setting.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VRGripInterface")
		EGripLateUpdateSettings GripLateUpdateSetting();

	// Define which movement replication setting to use.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VRGripInterface")
		EGripMovementReplicationSettings GripMovementReplicationType();

	// Grip type to use.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VRGripInterface")
		EGripCollisionType GetPrimaryGripType(bool bIsSlot);

	// Returns if the object is held and if so, which controllers are holding it.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VRGripInterface")
		void IsHeld(TArray<FBPGripPair>& HoldingControllers, bool& bIsHeld);

	// Returns if the object wants to be socketed.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VRGripInterface")
		bool RequestsSocketing(USceneComponent*& ParentToSocketTo, FName& OptionalSocketName, FTransform_NetQuantize& RelativeTransform);

	// Secondary grip type.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VRGripInterface")
		ESecondaryGripType SecondaryGripType();

	// Sets the Deny Gripping variable on the FBPInterfaceSettings struct.
	UFUNCTION(BlueprintCallable, Category = "VRGripInterface")
		void SetDenyGripping(bool bDenyGripping);

	// Sets is held, used by the plugin.
	UFUNCTION(BlueprintNativeEvent, /*BlueprintCallable,*/ Category = "VRGripInterface")
		void SetHeld(UGripMotionControllerComponent * HoldingController, uint8 GripID, bool bIsHeld);

	// Should this object simulate on drop.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VRGripInterface")
		bool SimulateOnDrop();

	// How an interfaced object behaves when teleporting.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VRGripInterface")
		EGripInterfaceTeleportBehavior TeleportBehavior();

	// Events //

	// Event triggered on the interfaced object when child component is gripped.
	UFUNCTION(BlueprintNativeEvent, Category = "VRGripInterface")
		void OnChildGrip(UGripMotionControllerComponent* GrippingController, const FBPActorGripInformation& GripInformation);

	// Event triggered on the interfaced object when child component is released.
	UFUNCTION(BlueprintNativeEvent, Category = "VRGripInterface")
		void OnChildGripRelease(UGripMotionControllerComponent* ReleasingController, const FBPActorGripInformation& GripInformation, bool bWasSocketed = false);

	// Event triggered on the interfaced object when gripped.
	UFUNCTION(BlueprintNativeEvent, Category = "VRGripInterface")
		void OnGrip(UGripMotionControllerComponent* GrippingController, const FBPActorGripInformation& GripInformation);

	// Event triggered on the interfaced object when grip is released.
	UFUNCTION(BlueprintNativeEvent, Category = "VRGripInterface")
		void OnGripRelease(UGripMotionControllerComponent* ReleasingController, const FBPActorGripInformation& GripInformation, bool bWasSocketed = false);

	// Event triggered on the interfaced object when secondary gripped.
	UFUNCTION(BlueprintNativeEvent, Category = "VRGripInterface")
		void OnSecondaryGrip(USceneComponent* SecondaryGripComponent, const FBPActorGripInformation& GripInformation);

	// Event triggered on the interfaced object when secondary grip is released.
	UFUNCTION(BlueprintNativeEvent, Category = "VRGripInterface")
		void OnSecondaryGripRelease(USceneComponent* ReleasingSecondaryGripComponent, const FBPActorGripInformation& GripInformation);

	// Event triggered each tick on the interfaced object when gripped, can be used for custom movement or grip based logic.
	UFUNCTION(BlueprintNativeEvent, Category = "VRGripInterface")
		void TickGrip(UGripMotionControllerComponent* GrippingController, const FBPActorGripInformation& GripInformation, float DeltaTime);

	// Interaction Functions

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VRGripInterface") void OnEndSecondaryUsed                              ();   // Call to stop using an object.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VRGripInterface") void OnEndUsed                                       ();   // Call to stop using an object.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VRGripInterface") void OnInput           (FKey Key, EInputEvent KeyEvent);   // Call to send an action event to the object.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VRGripInterface") void OnSecondaryUsed                                 ();   // Call to use an object.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VRGripInterface") void OnUsed                                          ();   // Call to use an object.


	// Declares

	// Gameplay Tags

	/** Tags that are set on this object */
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category = "GameplayTags")
		FGameplayTagContainer GameplayTags;

	// Replication

	// Skips the attachment replication if we are locally owned and our grip settings say that we are a client authed grip.
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category = "Replication")
		bool bAllowIgnoringAttachOnOwner;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category = "Replication")
		FVRClientAuthReplicationData ClientAuthReplicationData;

	// VR Grip Interface

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category = "VRGripInterface")
		bool bRepGripSettingsAndGameplayTags;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category = "VRGripInterface")
		FBPInterfaceProperties VRGripInterfaceSettings;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Instanced, Category = "VRGripInterface")
		TArray<class UVRGripScriptBase*> GripLogicScripts;
};