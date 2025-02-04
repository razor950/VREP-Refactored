// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

// Unreal
#include "CoreMinimal.h"
#include "Components/ShapeComponent.h"

// VREP
#include "VRTrackedParentInterface.h"
#include "VRBaseCharacter.h"
#include "VRExpansionFunctionLibrary.h"

// UHeader Tool
#include "VRRootComponent.generated.h"



//For UE4 Profiler ~ Stat Group
//DECLARE_STATS_GROUP(TEXT("VRPhysicsUpdate"), STATGROUP_VRPhysics, STATCAT_Advanced);

// Forward Declarations
//class AVRBaseCharacter;




DECLARE_LOG_CATEGORY_EXTERN(LogVRRootComponent, Log, All);

DECLARE_STATS_GROUP(TEXT("VRRootComponent"         ), STATGROUP_VRRootComponent, STATCAT_Advanced         );
DECLARE_CYCLE_STAT (TEXT("VR Root Set Half Height" ), STAT_VRRootSetHalfHeight , STATGROUP_VRRootComponent);
DECLARE_CYCLE_STAT (TEXT("VR Root Set Capsule Size"), STAT_VRRootSetCapsuleSize, STATGROUP_VRRootComponent);



/**
* A capsule component that repositions its physics scene and rendering location to the camera/HMD's relative position.
* Generally not to be used by itself unless on a base Pawn and not a character, the VRCharacter has been highly customized to correctly support it.
*/
UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent), ClassGroup = VRExpansionLibrary)
class VREXPANSIONPLUGIN_API UVRRootComponent : public UCapsuleComponent, public IVRTrackedParentInterface
{
	GENERATED_BODY()

public:
	// Friends

	friend class FDrawCylinderSceneProxy;


	// Contructors

	UVRRootComponent(const FObjectInitializer& ObjectInitializer);


	// Functions

	FORCEINLINE void GenerateOffsetToWorld(bool bUpdateBounds = true, bool bGetPureYaw = true);

	inline void OnUpdateTransform_Public(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport = ETeleportType::None)
	{
		OnUpdateTransform(UpdateTransformFlags, Teleport);

		if (bNavigationRelevant && bRegistered)
		{
			UpdateNavigationData    ();
			PostUpdateNavigationData();
		}
	}

	// Used to update the capsule half height and calculate the new offset value for VR.
	UFUNCTION(BlueprintCallable, Category = "Components|Capsule")
	void SetCapsuleHalfHeightVR(float HalfHeight, bool bUpdateOverlaps = true);

	/**
	* This is overidden for the VR Character to re-set physics location
	* Change the capsule size. This is the unscaled size, before component scale is applied.
	* @param	InRadius : radius of end-cap hemispheres and center cylinder.
	* @param	InHalfHeight : half-height, from capsule center to end of top or bottom hemisphere.
	* @param	bUpdateOverlaps: if true and this shape is registered and collides, updates touching array for owner actor.
	*/
	UFUNCTION(BlueprintCallable, Category = "Components|Capsule")
	virtual void SetCapsuleSizeVR(float NewRadius, float NewHalfHeight, bool bUpdateOverlaps = true);

	// UCapsuleComponent Overloads

	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;

	virtual void GetNavigationData(FNavigationRelevantData& Data) const override;   // Overriding this and applying the offset to world position for the elements.

	// UComponent Overloads?

	void                  BeginPlay          () override;
	FPrimitiveSceneProxy* CreateSceneProxy   () override;
	bool                  IsLocallyControlled() const   ;

	void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	// IVRTrackedParentInterface Overloads

	virtual void SetTrackedParent(UPrimitiveComponent * NewParentComponent, float WaistRadius, EBPVRWaistTrackingMode WaistTrackingMode) override
	{
		IVRTrackedParentInterface::Default_SetTrackedParent_Impl(NewParentComponent, WaistRadius, WaistTrackingMode, OptionalWaistTrackingParent, this);
	}

	// Something about UObjects...

	// Begin UObject interface
	#if WITH_EDITOR

		virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent  ) override;
		        void PreEditChange         (UProperty*             PropertyThatWillChange)         ;

	#endif // WITH_EDITOR
	// End UObject interface


	// Declares

	bool BCalledUpdateTransform;   // Original Name: bCalledUpdateTransform

	AVRBaseCharacter* OwningVRChar           ;   // Original Name: owningVRChar
	FVector           DifferenceFromLastFrame;
	FTransform        OffsetComponentToWorld ;
	FVector           CurrentCameraLocation  ;   // Original Name: curCameraLoc
	FRotator          CurrentCameraRotation  ;   // Original Name: curCameraRot
	FVector           LastCameraLocation     ;   // Original Name: lastCameraLoc
	FRotator          LastCameraRotation     ;   // Original Name: lastCameraRot
	FRotator          StoredCameraRotOffset  ;

	// While misnamed, is true if we collided with a wall/obstacle due to the HMDs movement in this frame (not movement components).
	UPROPERTY(BlueprintReadOnly, Category = "VRExpansionLibrary") 
		bool BHadRelativeMovement;   // Origial Name: bHadRelativeMovement

	UPROPERTY(BlueprintReadWrite, Transient, Category = "VRExpansionLibrary") 
		USceneComponent* TargetPrimitiveComponent;

	/*
	Used to offset the collision (IE backwards from the player slightly.
	The default 2.15 Z offset is to account for floor hover from the character movement component.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExpansionLibrary") FVector VRCapsuleOffset;

	// #TODO: See if making this multiplayer compatible is viable.
	// Offsets capsule to be centered on HMD - currently NOT multiplayer compatible.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExpansionLibrary") bool                           BCenterCapsuleOnHMD         ;   // Original Name: bCenterCapsuleOnHMD
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExpansionLibrary") bool                           BAllowSimulatingCollision   ;   // Original Name: bAllowSimulatingCollision   //Allows the root component to be blocked by simulating objects (default off due to sickness inducing stuttering).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExpansionLibrary") bool                           BUseWalkingCollisionOverride;   // Original Name: bUseWalkingCollisionOverride
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRExpansionLibrary") TEnumAsByte<ECollisionChannel> WalkingCollisionOverride    ;

	// If valid will use this as the tracked parent instead of the HMD / Parent.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "VRTrackedParentInterface")
		FBPVRWaistTracking_Info OptionalWaistTrackingParent;


protected:

	const TArray<FOverlapInfo>* ConvertRotationOverlapsToCurrentOverlaps(TArray<FOverlapInfo>& OverlapsAtEndLocation, const TArray<FOverlapInfo>& CurrentOverlaps);

	const TArray<FOverlapInfo>* ConvertSweptOverlapsToCurrentOverlaps
	(
		      TArray<FOverlapInfo>& OverlapsAtEndLocation, 
		const TArray<FOverlapInfo>& SweptOverlaps        , 
		      int32                 SweptOverlapsIndex   ,
		const FVector&              EndLocation          , 
		const FQuat&                EndRotationQuat
	);

	virtual bool MoveComponentImpl
	(
		const FVector&            Delta                          , 
		const FQuat&              NewRotation                    , 
		      bool                bSweep                         , 
		      FHitResult*         OutHit    = NULL               , 
		      EMoveComponentFlags MoveFlags = MOVECOMP_NoFlags   , 
		      ETeleportType       Teleport  = ETeleportType::None
	) 
	override;

	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport = ETeleportType::None) override;

	void SendPhysicsTransform(ETeleportType Teleport);

	virtual bool UpdateOverlapsImpl
	(
		const TArray<FOverlapInfo>* NewPendingOverlaps    = nullptr,
		      bool                  bDoNotifies           = true   ,
		const TArray<FOverlapInfo>* OverlapsAtEndLocation = nullptr
	) 
	override;


private:

		friend class FVRCharacterScopedMovementUpdate;

public:

	//UPROPERTY(BlueprintReadOnly, Transient, Category = "VRExpansionLibrary") FTransform OffsetComponentToWorld;

	//UPROPERTY(BlueprintReadWrite, Transient, Category = "VRExpansionLibrary") AVRBaseCharacter * owningVRChar;
	//UPROPERTY(BlueprintReadWrite, Transient, Category = "VRExpansionLibrary") UCapsuleComponent * VRCameraCollider;

	// #TODO: Test with 100.f rounding to make sure it isn't noticable, currently that is what it is
	// If true will subtract the HMD's location from the position, useful for if the actors base is set to the HMD location always (simple character).
	/*UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ReplicatedCamera") bool bOffsetByHMD; */

	/*ECollisionChannel GetVRCollisionObjectType()
	{
		if (bUseWalkingCollisionOverride)
			return WalkingCollisionOverride;
		else
			return GetCollisionObjectType();
	}*/
};


// Have to declare inlines here for blueprint
void inline UVRRootComponent::GenerateOffsetToWorld(bool bUpdateBounds, bool bGetPureYaw)
{
	FRotator camRotOffset;   // Original Name: CamRotOffset

	if (bGetPureYaw)
	{
		camRotOffset = StoredCameraRotOffset;   //UVRExpansionFunctionLibrary::GetHMDPureYaw_I(curCameraRot);
	}
	else
	{
		camRotOffset = CurrentCameraRotation;
	}

	/*
	if(bOffsetByHMD)
		OffsetComponentToWorld = FTransform(CamRotOffset.Quaternion(), FVector(0, 0, bCenterCapsuleOnHMD ? curCameraLoc.Z : CapsuleHalfHeight) + CamRotOffset.RotateVector(VRCapsuleOffset), FVector(1.0f)) * GetComponentTransform();
	else
	*/
	OffsetComponentToWorld = FTransform(camRotOffset.Quaternion(), FVector(CurrentCameraLocation.X, CurrentCameraLocation.Y, BCenterCapsuleOnHMD ? CurrentCameraLocation.Z : CapsuleHalfHeight) + camRotOffset.RotateVector(VRCapsuleOffset), FVector(1.0f)) * GetComponentTransform();

	if (OwningVRChar)
	{
		OwningVRChar->OffsetComponentToWorld = OffsetComponentToWorld;
	}

	if (bUpdateBounds)
	{
		UpdateBounds();
	}
}


FORCEINLINE void UVRRootComponent::SetCapsuleHalfHeightVR(float HalfHeight, bool bUpdateOverlaps)
{
	SCOPE_CYCLE_COUNTER(STAT_VRRootSetHalfHeight);

	if (FMath::IsNearlyEqual(HalfHeight, CapsuleHalfHeight))
	{
		return;
	}

	SetCapsuleSizeVR(GetUnscaledCapsuleRadius(), HalfHeight, bUpdateOverlaps);
}

FORCEINLINE void UVRRootComponent::SetCapsuleSizeVR(float NewRadius, float NewHalfHeight, bool bUpdateOverlaps)
{
	SCOPE_CYCLE_COUNTER(STAT_VRRootSetCapsuleSize);

	if (FMath::IsNearlyEqual(NewRadius, CapsuleRadius) && FMath::IsNearlyEqual(NewHalfHeight, CapsuleHalfHeight))
	{
		return;
	}

	CapsuleHalfHeight = FMath::Max3(0.f, NewHalfHeight, NewRadius);

	// Make sure that our character parent updates its replicated var as well
	if (AVRBaseCharacter* baseChar = Cast<AVRBaseCharacter>(GetOwner()))   // Original Name: BaseChar
	{
		if (GetNetMode() < ENetMode::NM_Client && baseChar->VRReplicateCapsuleHeight)
		{
			baseChar->ReplicatedCapsuleHeight.CapsuleHeight = CapsuleHalfHeight;
		}
	}

	CapsuleRadius = FMath::Max(0.f, NewRadius);

	UpdateBounds         ();
	UpdateBodySetup      ();
	MarkRenderStateDirty ();
	GenerateOffsetToWorld();

	// Do this if already created; otherwise, it hasn't been really created yet.
	if (bPhysicsStateCreated)
	{
		// Update physics engine collision shapes.
		BodyInstance.UpdateBodyScale(GetComponentTransform().GetScale3D(), true);

		if (bUpdateOverlaps && IsCollisionEnabled() && GetOwner())
		{
			UpdateOverlaps();
		}
	}
}