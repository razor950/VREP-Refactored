// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

// Includes

// Unreal
#include "CoreMinimal.h"
#include "Components/CapsuleComponent.h"
#include "Engine/ActorChannel.h"
#include "GameplayTagAssetInterface.h"
#include "GameplayTagContainer.h"

// VREP
#include "GripScripts/VRGripScriptBase.h"
#include "VRBPDatatypes.h"
#include "VRGripInterface.h"
#include "VRExpansionFunctionLibrary.h"

// UHeader Tool
#include "GrippableCapsuleComponent.generated.h"



/**
*
*/
UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent), ClassGroup = (VRExpansionPlugin))
class VREXPANSIONPLUGIN_API UGrippableCapsuleComponent : 
	// Parent
	public UCapsuleComponent, 
	
	// Interfaces
	public IGameplayTagAssetInterface, public IVRGripInterface
{
	GENERATED_BODY()

public:

	// Constructor & Destructor

	UGrippableCapsuleComponent(const FObjectInitializer& ObjectInitializer);
	
	~UGrippableCapsuleComponent();


	// Functions

	// UCapsuleComponent Overloads

	virtual void BeginPlay                                        () override;
	virtual void EndPlay  (const EEndPlayReason::Type EndPlayReason) override;

	virtual void GetSubobjectsWithStableNamesForNetworking(TArray<UObject*> &ObjList) override;

	// This one is for components to clean up.
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

	/** Called right before being marked for destruction due to network replication. */
	// Clean up our objects so that they aren't sitting around for GC.
	virtual void PreDestroyFromReplication() override;

	virtual void PreReplication(IRepChangedPropertyTracker& ChangedPropertyTracker) override;

	bool ReplicateSubobjects(UActorChannel* Channel, class FOutBunch* Bunch, FReplicationFlags* RepFlags) override;

	// Gameplay tag interface
	// ------------------------------------------------

	/** Overridden to return requirements tags. */
	virtual void GetOwnedGameplayTags(FGameplayTagContainer& TagContainer) const override
	{
		TagContainer = GameplayTags;
	}

	// End Gameplay Tag Interface

	// VRGripInterface

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
			FVector                         WorldLocation                 , 
			bool                            bSecondarySlot                , 
			bool&                           bHadSlotInRange               , 
			FTransform&                     SlotWorldTransform            , 
			UGripMotionControllerComponent* CallingController  = nullptr  ,  
			FName                           OverridePrefix     = NAME_None
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

	// Grip type to use.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VRGripInterface")
		EGripCollisionType GetPrimaryGripType(bool bIsSlot);

	// What distance to break a grip at (only relevant with physics enabled grips.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VRGripInterface")
		float GripBreakDistance();

	// Define the late update setting.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VRGripInterface")
		EGripLateUpdateSettings GripLateUpdateSetting();

	// Define which movement replication setting to use.
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "VRGripInterface")
		EGripMovementReplicationSettings GripMovementReplicationType();

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
		void SetHeld(UGripMotionControllerComponent* HoldingController, uint8 GripID, bool bIsHeld);

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

	bool bOriginalReplicatesMovement;

	// IGameplayTagAssetInterface

	/** Tags that are set on this object. */
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category = "GameplayTags")
		FGameplayTagContainer GameplayTags;

	// IVRGripInterface

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadOnly, Instanced, Category = "VRGripInterface")
		TArray<class UVRGripScriptBase*> GripLogicScripts;

	UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category = "VRGripInterface")
		FBPInterfaceProperties VRGripInterfaceSettings;

	// VRGripInterface Replication

	// Requires bReplicates to be true for the component.
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category = "VRGripInterface|Replication")
		bool bRepGripSettingsAndGameplayTags;

	// Overrides the default of : true and allows for controlling it like in an actor, should be default of off normally with grippable components.
	UPROPERTY(EditAnywhere, Replicated, BlueprintReadWrite, Category = "VRGripInterface|Replication")
		bool bReplicateMovement;
};