// Fill out your copyright notice in the Description page of Project Settings.

// Parent Header
#include "GripScripts/GS_LerpToHand.h"

// Unreal
#include "Math/DualQuat.h"

// VREP
#include "GripMotionControllerComponent.h"

// UGS_LerpToHand

// Public

// Constructor & Destructor

//=============================================================================
UGS_LerpToHand::UGS_LerpToHand(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	bIsActive = false;
	bDenyAutoDrop = true; // Always deny auto dropping while this script is active
	WorldTransformOverrideType = EGSTransformOverrideType::ModifiesWorldTransform;

	LerpInterpolationMode = EVRLerpInterpolationMode::QuatInterp;
	InterpSpeed = 1.0f;
	CurrentLerpTime = 0.0f;
	OnGripTransform = FTransform::Identity;
	bUseCurve = false;
	MinDistanceForLerp = 0.0f;
}


// Functions

//void UGS_InteractibleSettings::BeginPlay_Implementation() {}
void UGS_LerpToHand::OnGrip_Implementation(UGripMotionControllerComponent* GrippingController, const FBPActorGripInformation& GripInformation) 
{
	OnGripTransform = GetParentTransform();

	if (MinDistanceForLerp > 0.0f)
	{
		FTransform TargetTransform = GripInformation.RelativeTransform * GrippingController->GetPivotTransform();
		if (FVector::DistSquared(OnGripTransform.GetLocation(), TargetTransform.GetLocation()) < FMath::Square(MinDistanceForLerp))
		{
			// Don't init
			return;
		}
	}

	bIsActive = true;
	CurrentLerpTime = 0.0f;
}

void UGS_LerpToHand::OnGripRelease_Implementation(UGripMotionControllerComponent* ReleasingController, const FBPActorGripInformation& GripInformation, bool bWasSocketed) 
{
	bIsActive = false;
}

bool UGS_LerpToHand::GetWorldTransform_Implementation
(
	UGripMotionControllerComponent* GrippingController, 
	float DeltaTime, FTransform& WorldTransform, 
	const FTransform& ParentTransform, 
	FBPActorGripInformation& Grip, 
	AActor* actor, 
	UPrimitiveComponent* root, 
	bool bRootHasInterface, 
	bool bActorHasInterface, 
	bool bIsForTeleport
) 
{
	if (!root)
		return false;

	if (InterpSpeed <= 0.f)
	{
		bIsActive = false;
	}

	FTransform NA = OnGripTransform;//root->GetComponentTransform();

	float Alpha = 0.0f; 

	if (bUseCurve)
	{
		if (FRichCurve * richCurve = OptionalCurveToFollow.GetRichCurve())
		{
			if (CurrentLerpTime > richCurve->GetLastKey().Time)
			{
				// Stop lerping
				OnLerpToHandFinished.Broadcast();
				CurrentLerpTime = 0.0f;
				bIsActive = false;
				return true;
			}
			else
			{
				float curCurveAlpha = richCurve->Eval(CurrentLerpTime);
				curCurveAlpha = FMath::Clamp(curCurveAlpha, 0.0f, 1.0f);

				Alpha = curCurveAlpha;

				CurrentLerpTime += DeltaTime;
			}
		}
	}
	else
	{
		Alpha = FMath::Clamp(CurrentLerpTime, 0.f, 1.0f);
		CurrentLerpTime += DeltaTime * InterpSpeed;
	}

	FTransform NB = WorldTransform;
	NA.NormalizeRotation();
	NB.NormalizeRotation();

	// Quaternion interpolation
	if (LerpInterpolationMode == EVRLerpInterpolationMode::QuatInterp)
	{
		WorldTransform.Blend(NA, NB, Alpha);
	}

	// Euler Angle interpolation
	else if (LerpInterpolationMode == EVRLerpInterpolationMode::EulerInterp)
	{
		WorldTransform.SetTranslation(FMath::Lerp(NA.GetTranslation(), NB.GetTranslation(), Alpha));
		WorldTransform.SetScale3D(FMath::Lerp(NA.GetScale3D(), NB.GetScale3D(), Alpha));

		FRotator A = NA.Rotator();
		FRotator B = NB.Rotator();
		WorldTransform.SetRotation(FQuat(A + (Alpha * (B - A))));
	}
	// Dual quaternion interpolation
	else
	{
		if ((NB.GetRotation() | NA.GetRotation()) < 0.0f)
		{
			NB.SetRotation(NB.GetRotation()*-1.0f);
		}
		WorldTransform = (FDualQuat(NA)*(1 - Alpha) + FDualQuat(NB)*Alpha).Normalized().AsFTransform(FMath::Lerp(NA.GetScale3D(), NB.GetScale3D(), Alpha));
	}

	// Turn it off if we need to
	if (Alpha == 1.0f)
	{
		OnLerpToHandFinished.Broadcast();
		CurrentLerpTime = 0.0f;
		bIsActive = false;
	}

	return true;
}