// Fill out your copyright notice in the Description page of Project Settings.

// Parent Header
#include "GripScripts/GS_GunTools.h"

// Unreal
#include "DrawDebugHelpers.h"
#include "IXRTrackingSystem.h"

// VREP
#include "VRGripInterface.h"
#include "GripMotionControllerComponent.h"
#include "VRExpansionFunctionLibrary.h"
#include "VRGlobalSettings.h"
#include "VRBaseCharacter.h"

// UGS_GunTools

// Public

// Constructor & Destructor

//=============================================================================
UGS_GunTools::UGS_GunTools(const FObjectInitializer& ObjectInitializer) :
	Super(ObjectInitializer)
{
	bIsActive = true;
	WorldTransformOverrideType = EGSTransformOverrideType::OverridesWorldTransform;
	PivotOffset = FVector::ZeroVector;
	VirtualStockComponent = nullptr;
	MountWorldTransform = FTransform::Identity;
	//StockSnapOffset = FVector(0.f, 0.f, 0.f);
	bIsMounted = false;


	bHasRecoil = false;
	MaxRecoilTranslation = FVector::ZeroVector;
	MaxRecoilRotation = FVector::ZeroVector;
	MaxRecoilScale = FVector(1.f);
	bHasActiveRecoil = false;
	DecayRate = 20.f;
	LerpRate = 30.f;

	BackEndRecoilStorage = FTransform::Identity;

	//StockSnapDistance = 35.f;
	//bUseDistanceBasedStockSnapping = true;
	//SmoothingValueForStock = 0.0f;
	//bSmoothStockHand = false;

	// Speed up the lerp on fast movements for this
	//StockHandSmoothing.DeltaCutoff = 20.0f;
	//StockHandSmoothing.MinCutoff = 5.0f;
	//bDebugDrawVirtualStock = false;

	bUseGlobalVirtualStockSettings = true;

	bUseHighQualityRemoteSimulation = false;
}

// Functions

//=============================================================================
/*void UGS_GunTools::GetLifetimeReplicatedProps(TArray< class FLifetimeProperty > & OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UGS_GunTools, bUseAdvancedSecondarySettings);

	DOREPLIFETIME_CONDITION(UGS_GunTools, SecondaryGripScaler, COND_Custom);
	DOREPLIFETIME_CONDITION(UGS_GunTools, bUseConstantGripScaler, COND_Custom);
	DOREPLIFETIME_CONDITION(UGS_GunTools, bUseSecondaryGripDistanceInfluence, COND_Custom);
	DOREPLIFETIME_CONDITION(UGS_GunTools, GripInfluenceDeadZone, COND_Custom);
	DOREPLIFETIME_CONDITION(UGS_GunTools, GripInfluenceDistanceToZero, COND_Custom);

	DOREPLIFETIME(UGS_GunTools, PivotOffset);

	DOREPLIFETIME(UGS_GunTools, bUseVirtualStock);
	DOREPLIFETIME_CONDITION(UGS_GunTools, VirtualStockComponent, COND_Custom);
	DOREPLIFETIME_CONDITION(UGS_GunTools, bUseDistanceBasedStockSnapping, COND_Custom);
	DOREPLIFETIME_CONDITION(UGS_GunTools, StockSnapDistance, COND_Custom);
	DOREPLIFETIME_CONDITION(UGS_GunTools, StockSnapOffset, COND_Custom);

	DOREPLIFETIME(UGS_GunTools, bHasRecoil);
	DOREPLIFETIME_CONDITION(UGS_GunTools, MaxRecoilTranslation, COND_Custom);
	DOREPLIFETIME_CONDITION(UGS_GunTools, MaxRecoilRotation, COND_Custom);
	DOREPLIFETIME_CONDITION(UGS_GunTools, MaxRecoilScale, COND_Custom);
	DOREPLIFETIME_CONDITION(UGS_GunTools, DecayRate, COND_Custom);
	DOREPLIFETIME_CONDITION(UGS_GunTools, LerpRate, COND_Custom);
}*/

void UGS_GunTools::AddRecoilInstance(const FTransform& RecoilAddition)
{
	if (!bHasRecoil)
		return;

	BackEndRecoilTarget += RecoilAddition;

	FVector CurVec = BackEndRecoilTarget.GetTranslation();

	// Identity on min value is technically wrong, what if they want to recoil in the opposing direction?
	CurVec.X = FMath::Clamp(CurVec.X, FMath::Min(0.f, MaxRecoilTranslation.X), FMath::Max(MaxRecoilTranslation.X, 0.f));
	CurVec.Y = FMath::Clamp(CurVec.Y, FMath::Min(0.f, MaxRecoilTranslation.Y), FMath::Max(MaxRecoilTranslation.Y, 0.f));
	CurVec.Z = FMath::Clamp(CurVec.Z, FMath::Min(0.f, MaxRecoilTranslation.Z), FMath::Max(MaxRecoilTranslation.Z, 0.f));
	BackEndRecoilTarget.SetTranslation(CurVec);

	FVector CurScale = BackEndRecoilTarget.GetScale3D();

	// Identity on min value is technically wrong, what if they want to recoil in the opposing direction?
	CurScale.X = FMath::Clamp(CurScale.X, FMath::Min(0.f, MaxRecoilScale.X), FMath::Max(MaxRecoilScale.X, 0.f));
	CurScale.Y = FMath::Clamp(CurScale.Y, FMath::Min(0.f, MaxRecoilScale.Y), FMath::Max(MaxRecoilScale.Y, 0.f));
	CurScale.Z = FMath::Clamp(CurScale.Z, FMath::Min(0.f, MaxRecoilScale.Z), FMath::Max(MaxRecoilScale.Z, 0.f));
	BackEndRecoilTarget.SetScale3D(CurScale);

	FRotator curRot = BackEndRecoilTarget.Rotator();
	curRot.Pitch = FMath::Clamp(curRot.Pitch, FMath::Min(0.f, MaxRecoilRotation.Y), FMath::Max(MaxRecoilRotation.Y, 0.f));
	curRot.Yaw = FMath::Clamp(curRot.Yaw, FMath::Min(0.f, MaxRecoilRotation.Z), FMath::Max(MaxRecoilRotation.Z, 0.f));
	curRot.Roll = FMath::Clamp(curRot.Roll, FMath::Min(0.f, MaxRecoilRotation.X), FMath::Max(MaxRecoilRotation.X, 0.f));

	BackEndRecoilTarget.SetRotation(curRot.Quaternion());

	bHasActiveRecoil = !BackEndRecoilTarget.Equals(FTransform::Identity);
}

void UGS_GunTools::GetVirtualStockTarget(UGripMotionControllerComponent* GrippingController)
{
	if (GrippingController && (GrippingController->bHasAuthority || bUseHighQualityRemoteSimulation))
	{
		if (AVRBaseCharacter* vrOwner = Cast<AVRBaseCharacter>(GrippingController->GetOwner()))
		{
			CameraComponent = vrOwner->VRReplicatedCamera;
			return;
		}
		else
		{
			TArray<USceneComponent*> children = GrippingController->GetOwner()->GetRootComponent()->GetAttachChildren();

			for (int i = 0; i < children.Num(); i++)
			{
				if (children[i]->IsA(UCameraComponent::StaticClass()))
				{
					CameraComponent = children[i];
					return;
				}
			}
		}

		CameraComponent = nullptr;
	}
}

bool UGS_GunTools::GetWorldTransform_Implementation
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
	if (!GrippingController)
		return false;

	bool bSkipHighQualityOperations = !bUseHighQualityRemoteSimulation && !GrippingController->bHasAuthority;

	/*if (!GunViewExtension.IsValid() && GEngine)
	{
		GunViewExtension = FSceneViewExtensions::NewExtension<FGunViewExtension>(GrippingController);
	}*/

	// Just simple transform setting
	if (bHasRecoil && bHasActiveRecoil)
	{
		BackEndRecoilStorage.Blend(BackEndRecoilStorage, BackEndRecoilTarget, FMath::Clamp(LerpRate * DeltaTime, 0.f, 1.f));
		BackEndRecoilTarget.Blend(BackEndRecoilTarget, FTransform::Identity, FMath::Clamp(DecayRate * DeltaTime, 0.f, 1.f));
		bHasActiveRecoil = !BackEndRecoilTarget.Equals(FTransform::Identity);

		if (!bHasActiveRecoil)
		{
			BackEndRecoilStorage.SetIdentity();
			BackEndRecoilTarget.SetIdentity();
		}		
	}

	if (bHasActiveRecoil)
	{
		// Eventually may want to adjust the pivot of the recoil rotation by the PivotOffset vector...
		WorldTransform = BackEndRecoilStorage * Grip.RelativeTransform * Grip.AdditionTransform * ParentTransform;
	}
	else
		WorldTransform = Grip.RelativeTransform * Grip.AdditionTransform * ParentTransform;

	// Check the grip lerp state, this it ouside of the secondary attach check below because it can change the result of it
	if (Grip.SecondaryGripInfo.bHasSecondaryAttachment && Grip.SecondaryGripInfo.SecondaryAttachment)
	{
		if (!bSkipHighQualityOperations && bUseVirtualStock)
		{
			if (VirtualStockComponent.IsValid())
			{
				FRotator PureYaw = UVRExpansionFunctionLibrary::GetHMDPureYaw_I(VirtualStockComponent->GetComponentRotation());
				MountWorldTransform = FTransform(PureYaw.Quaternion(), VirtualStockComponent->GetComponentLocation() + PureYaw.RotateVector(VirtualStockSettings.StockSnapOffset));
			}
			else if (GrippingController->bHasAuthority && GEngine->XRSystem.IsValid() && GEngine->XRSystem->IsHeadTrackingAllowed())
			{
				FQuat curRot = FQuat::Identity;
				FVector curLoc = FVector::ZeroVector;

				if (GEngine->XRSystem->GetCurrentPose(IXRTrackingSystem::HMDDeviceId, curRot, curLoc))
				{
					// Translate hmd offset by the gripping controllers parent component, this should be in the same space
					FRotator PureYaw = UVRExpansionFunctionLibrary::GetHMDPureYaw_I(curRot.Rotator());
					MountWorldTransform = FTransform(PureYaw.Quaternion(), curLoc + PureYaw.RotateVector(VirtualStockSettings.StockSnapOffset)) * GrippingController->GetAttachParent()->GetComponentTransform();
				}
			}
			else if(CameraComponent.IsValid())
			{		
				FRotator PureYaw = UVRExpansionFunctionLibrary::GetHMDPureYaw_I(CameraComponent->GetComponentRotation());
				MountWorldTransform = FTransform(PureYaw.Quaternion(), CameraComponent->GetComponentLocation() + PureYaw.RotateVector(VirtualStockSettings.StockSnapOffset));
			}

			float StockSnapDistance = FMath::Square(VirtualStockSettings.StockSnapDistance);
			float DistSquared = FVector::DistSquared(ParentTransform.GetTranslation(), MountWorldTransform.GetTranslation());

			if (DistSquared <= StockSnapDistance)
			{

				float StockSnapLerpThresh = FMath::Square(VirtualStockSettings.StockSnapLerpThreshold);

				if (StockSnapLerpThresh > 0.0f)
					VirtualStockSettings.StockLerpValue = 1.0f - FMath::Clamp((DistSquared - (StockSnapDistance - StockSnapLerpThresh)) / StockSnapLerpThresh, 0.0f, 1.0f);
				else
					VirtualStockSettings.StockLerpValue = 1.0f; // Just skip lerping logic

				if (!bIsMounted)
				{
					VirtualStockSettings.StockHandSmoothing.ResetSmoothingFilter();
					
					// Mount up
					bIsMounted = true;

					OnVirtualStockModeChanged.Broadcast(bIsMounted);
				}

				// Adjust the mount location to follow the Z of the primary hand
				FVector WorldTransVec = MountWorldTransform.GetTranslation();
				WorldTransVec.Z = ParentTransform.GetTranslation().Z;
				MountWorldTransform.SetLocation(WorldTransVec);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				if (VirtualStockSettings.bDebugDrawVirtualStock)
				{
					DrawDebugLine(GetWorld(), ParentTransform.GetTranslation(), MountWorldTransform.GetTranslation(), FColor::Red);
					DrawDebugLine(GetWorld(), Grip.SecondaryGripInfo.SecondaryAttachment->GetComponentLocation(), MountWorldTransform.GetTranslation(), FColor::Green);
					DrawDebugSphere(GetWorld(), MountWorldTransform.GetTranslation(), 10.f, 32, FColor::White);
				}
#endif
			}
			else
			{
				if (bIsMounted)
				{
					bIsMounted = false;
					VirtualStockSettings.StockLerpValue = 0.0f;
					OnVirtualStockModeChanged.Broadcast(bIsMounted);
				}
			}

			if (bIsMounted && VirtualStockSettings.bSmoothStockHand)
			{
				FVector smoothedTrans = FMath::Lerp(WorldTransform.GetTranslation(), VirtualStockSettings.StockHandSmoothing.RunFilterSmoothing(WorldTransform.GetTranslation(), DeltaTime), VirtualStockSettings.SmoothingValueForStock);
				WorldTransform.SetTranslation(smoothedTrans);

			}
		}
	}
	else
	{
		if (bIsMounted)
		{
			bIsMounted = false;
			VirtualStockSettings.StockLerpValue = 0.0f;
			OnVirtualStockModeChanged.Broadcast(bIsMounted);
		}
	}

	// Check the grip lerp state, this it ouside of the secondary attach check below because it can change the result of it
	if ((Grip.SecondaryGripInfo.bHasSecondaryAttachment && Grip.SecondaryGripInfo.SecondaryAttachment) || Grip.SecondaryGripInfo.GripLerpState == EGripLerpState::EndLerp)
	{
		switch (Grip.SecondaryGripInfo.GripLerpState)
		{
		case EGripLerpState::StartLerp:
		case EGripLerpState::EndLerp:
		{
			if (Grip.SecondaryGripInfo.curLerp > 0.01f)
				Grip.SecondaryGripInfo.curLerp -= DeltaTime;
			else
			{
				Grip.SecondaryGripInfo.GripLerpState = EGripLerpState::NotLerping;
			}

		}break;
		//case EGripLerpState::ConstantLerp_DEPRECATED:
		case EGripLerpState::NotLerping:
		default:break;
		}
	}

	// Handle the interp and multi grip situations, re-checking the grip situation here as it may have changed in the switch above.
	if ((Grip.SecondaryGripInfo.bHasSecondaryAttachment && Grip.SecondaryGripInfo.SecondaryAttachment) || Grip.SecondaryGripInfo.GripLerpState == EGripLerpState::EndLerp)
	{
		FTransform SecondaryTransform = Grip.RelativeTransform * ParentTransform;

		// Checking secondary grip type for the scaling setting
		ESecondaryGripType SecondaryType = ESecondaryGripType::SG_None;

		if (bRootHasInterface)
			SecondaryType = IVRGripInterface::Execute_SecondaryGripType(root);
		else if (bActorHasInterface)
			SecondaryType = IVRGripInterface::Execute_SecondaryGripType(actor);

		// If the grip is a custom one, skip all of this logic we won't be changing anything
		if (SecondaryType != ESecondaryGripType::SG_Custom)
		{
			// Variables needed for multi grip transform
			FVector BasePoint = ParentTransform.GetLocation();
			FVector Pivot = (FTransform(PivotOffset) * ParentTransform).GetLocation();
				
			const FTransform PivotToWorld = FTransform(FQuat::Identity, Pivot);//BasePoint);
			const FTransform WorldToPivot = FTransform(FQuat::Identity, -Pivot);//-BasePoint);

			FVector frontLocOrig;
			FVector frontLoc;

			// Ending lerp out of a multi grip
			if (Grip.SecondaryGripInfo.GripLerpState == EGripLerpState::EndLerp)
			{
				frontLocOrig = (/*WorldTransform*/SecondaryTransform.TransformPosition(Grip.SecondaryGripInfo.SecondaryRelativeTransform.GetLocation())) - BasePoint;
				frontLoc = Grip.SecondaryGripInfo.LastRelativeLocation;

				frontLocOrig = FMath::Lerp(frontLoc, frontLocOrig, FMath::Clamp(Grip.SecondaryGripInfo.curLerp / Grip.SecondaryGripInfo.LerpToRate, 0.0f, 1.0f));
			}
			else // Is in a multi grip, might be lerping into it as well.
			{
				//FVector curLocation; // Current location of the secondary grip

				bool bPulledControllerLoc = false;
				if (GrippingController->bHasAuthority && Grip.SecondaryGripInfo.SecondaryAttachment->GetOwner() == GrippingController->GetOwner())
				{
					if (UGripMotionControllerComponent * OtherController = Cast<UGripMotionControllerComponent>(Grip.SecondaryGripInfo.SecondaryAttachment))
					{
						if (!OtherController->bUseWithoutTracking)
						{
							FVector Position = FVector::ZeroVector;
							FRotator Orientation = FRotator::ZeroRotator;
							float WorldToMeters = GetWorld() ? GetWorld()->GetWorldSettings()->WorldToMeters : 100.0f;
							if (OtherController->GripPollControllerState(Position, Orientation, WorldToMeters))
							{
								frontLoc = OtherController->CalcControllerComponentToWorld(Orientation, Position).GetLocation() - BasePoint;
								///*curLocation*/ frontLoc = OtherController->CalcNewComponentToWorld(FTransform(Orientation, Position)).GetLocation() - BasePoint;
								bPulledControllerLoc = true;
							}
						}
					}
				}

				if (!bPulledControllerLoc)
					/*curLocation*/ frontLoc = Grip.SecondaryGripInfo.SecondaryAttachment->GetComponentLocation() - BasePoint;

				frontLocOrig = (/*WorldTransform*/SecondaryTransform.TransformPosition(Grip.SecondaryGripInfo.SecondaryRelativeTransform.GetLocation())) - BasePoint;

				// Apply any smoothing settings and lerping in / constant lerping
				GunTools_ApplySmoothingAndLerp(Grip, frontLoc, frontLocOrig, DeltaTime, bSkipHighQualityOperations);

				Grip.SecondaryGripInfo.LastRelativeLocation = frontLoc;
			}

			// Get any scaling addition from a scaling secondary grip type
			FVector Scaler = FVector(1.0f);
			Default_GetAnyScaling(Scaler, Grip, frontLoc, frontLocOrig, SecondaryType, SecondaryTransform);

			Grip.SecondaryGripInfo.SecondaryGripDistance = FVector::Dist(frontLocOrig, frontLoc);

			if (!bSkipHighQualityOperations && AdvSecondarySettings.bUseAdvancedSecondarySettings && AdvSecondarySettings.bUseSecondaryGripDistanceInfluence)
			{
				float rotScaler = 1.0f - FMath::Clamp((Grip.SecondaryGripInfo.SecondaryGripDistance - AdvSecondarySettings.GripInfluenceDeadZone) / FMath::Max(AdvSecondarySettings.GripInfluenceDistanceToZero, 1.0f), 0.0f, 1.0f);
				frontLoc = FMath::Lerp(frontLocOrig, frontLoc, rotScaler);
			}

			// Skip rot val for scaling only
			if (SecondaryType != ESecondaryGripType::SG_ScalingOnly)
			{
				// Get shoulder mount addition rotation
				if (!bSkipHighQualityOperations && bUseVirtualStock && bIsMounted)
				{
					// Get the rotation difference from the initial second grip
					FQuat rotVal = FQuat::FindBetweenVectors(GrippingController->GetPivotLocation() - MountWorldTransform.GetTranslation(), (frontLoc + BasePoint) - MountWorldTransform.GetTranslation());
					FQuat MountAdditionRotation = FQuat::FindBetweenVectors(frontLocOrig, GrippingController->GetPivotLocation() - MountWorldTransform.GetTranslation());

					if (VirtualStockSettings.StockLerpValue < 1.0f)
					{
						// Rebase the world transform to the pivot point, add the rotation, remove the pivot point rebase
						FTransform NA = FTransform(rotVal * MountAdditionRotation, FVector::ZeroVector, Scaler);
						FTransform NB = FTransform(FQuat::FindBetweenVectors(frontLocOrig, frontLoc), FVector::ZeroVector, Scaler);
						NA.NormalizeRotation();
						NB.NormalizeRotation();

						// Quaternion interpolation
						NA.Blend(NB, NA, VirtualStockSettings.StockLerpValue);

						WorldTransform = WorldTransform * WorldToPivot * NA * PivotToWorld;
					}
					else
					{
						// Rebase the world transform to the pivot point, add the rotation, remove the pivot point rebase
						WorldTransform = WorldTransform * WorldToPivot * MountAdditionRotation * FTransform(rotVal, FVector::ZeroVector, Scaler) * PivotToWorld;
					}
				}
				else
				{
					// Get the rotation difference from the initial second grip
					FQuat rotVal = FQuat::FindBetweenVectors(frontLocOrig, frontLoc);

					// Rebase the world transform to the pivot point, add the rotation, remove the pivot point rebase
					WorldTransform = WorldTransform * WorldToPivot * FTransform(rotVal, FVector::ZeroVector, Scaler) * PivotToWorld;
				}
			}
			else
			{

				// Get shoulder mount addition rotation
				if (!bSkipHighQualityOperations && bUseVirtualStock && bIsMounted)
				{
					FQuat MountAdditionRotation = FQuat::FindBetweenVectors(frontLocOrig, GrippingController->GetPivotLocation() - MountWorldTransform.GetTranslation());

					// If it is exactly 1.0f then lets skip all of the extra logic and just set it
					if (VirtualStockSettings.StockLerpValue < 1.0f)
					{
						FTransform NA = FTransform(MountAdditionRotation, FVector::ZeroVector, Scaler);
						FTransform NB = FTransform(FQuat::Identity, FVector::ZeroVector, Scaler);
						NA.NormalizeRotation();
						NB.NormalizeRotation();

						// Quaternion interpolation
						NA.Blend(NB, NA, VirtualStockSettings.StockLerpValue);
						WorldTransform = WorldTransform * WorldToPivot * NA * PivotToWorld;
					}
					else
					{
						// Rebase the world transform to the pivot point, add the scaler, remove the pivot point rebase
						WorldTransform = WorldTransform * WorldToPivot * MountAdditionRotation * FTransform(FQuat::Identity, FVector::ZeroVector, Scaler) * PivotToWorld;
					}
				}
				else
				{
					// Rebase the world transform to the pivot point, add the scaler, remove the pivot point rebase
					WorldTransform = WorldTransform * WorldToPivot * FTransform(FQuat::Identity, FVector::ZeroVector, Scaler) * PivotToWorld;
				}
			}
		}
	}
	return true;
}

void UGS_GunTools::OnGrip_Implementation(UGripMotionControllerComponent* GrippingController, const FBPActorGripInformation& GripInformation)
{

	if (bUseGlobalVirtualStockSettings)
	{
		if (GrippingController->IsLocallyControlled())
		{
			FBPVirtualStockSettings VirtualSettings;
			UVRGlobalSettings::GetVirtualStockGlobalSettings(VirtualSettings);
			VirtualStockSettings.CopyFrom(VirtualSettings);
		}
	}

	// Super doesn't do anything on grip

	// Reset smoothing filters
	if (AdvSecondarySettings.bUseConstantGripScaler)
	{
		if (AdvSecondarySettings.bUseGlobalSmoothingSettings)
		{
			const UVRGlobalSettings& VRSettings = *GetDefault<UVRGlobalSettings>();
			AdvSecondarySettings.SecondarySmoothing.CutoffSlope = VRSettings.OneEuroCutoffSlope;
			AdvSecondarySettings.SecondarySmoothing.DeltaCutoff = VRSettings.OneEuroDeltaCutoff;
			AdvSecondarySettings.SecondarySmoothing.MinCutoff = VRSettings.OneEuroMinCutoff;
		}

		AdvSecondarySettings.SecondarySmoothing.ResetSmoothingFilter();
	}

	if (bUseVirtualStock)
	{
		ResetStockVariables();
	}

	GetVirtualStockTarget(GrippingController);
}

void UGS_GunTools::OnSecondaryGrip_Implementation(UGripMotionControllerComponent* Controller, USceneComponent* SecondaryGripComponent, const FBPActorGripInformation& GripInformation)
{
	// Super doesn't do anything on Secondary grip

	// Reset smoothing filters
	if (AdvSecondarySettings.bUseConstantGripScaler)
	{
		if (AdvSecondarySettings.bUseGlobalSmoothingSettings)
		{
			const UVRGlobalSettings& VRSettings = *GetDefault<UVRGlobalSettings>();
			AdvSecondarySettings.SecondarySmoothing.CutoffSlope = VRSettings.OneEuroCutoffSlope;
			AdvSecondarySettings.SecondarySmoothing.DeltaCutoff = VRSettings.OneEuroDeltaCutoff;
			AdvSecondarySettings.SecondarySmoothing.MinCutoff = VRSettings.OneEuroMinCutoff;
		}

		AdvSecondarySettings.SecondarySmoothing.ResetSmoothingFilter();
	}

	if (bUseVirtualStock)
		ResetStockVariables();
}

void UGS_GunTools::ResetRecoil()
{
	BackEndRecoilStorage = FTransform::Identity;
	BackEndRecoilTarget = FTransform::Identity;
}

