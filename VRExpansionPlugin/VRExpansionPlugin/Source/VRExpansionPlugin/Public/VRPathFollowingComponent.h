// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

// Unreal

#include "CoreMinimal.h"
#include "AbstractNavData.h"
#include "Navigation/PathFollowingComponent.h"
#include "Runtime/Launch/Resources/Version.h"

// VREP
#include "VRBPDatatypes.h"
#include "VRRootComponent.h"
#include "VRBaseCharacterMovementComponent.h"
#include "VRPathFollowingComponent.generated.h"



DECLARE_LOG_CATEGORY_EXTERN(LogPathFollowingVR, Warning, All);



UCLASS()
class VREXPANSIONPLUGIN_API UVRPathFollowingComponent : public UPathFollowingComponent
{
	GENERATED_BODY()

public:

	// Functions

	void DebugReachTest
	(
		float& CurrentDot     , 
		float& CurrentDistance, 
		float& CurrentHeight  , 
		uint8& bDotFailed     , 
		uint8& bDistanceFailed, 
		uint8& bHeightFailed
	) 
	const;

	/** Simple test for stationary agent (used as early finish condition), check if reached given point.
	*  @param TestPoint - point to test
	*  @param AcceptanceRadius - allowed 2D distance
	*  @param ReachMode - modifiers for AcceptanceRadius
	*/
	bool HasReached(const FVector& TestPoint, EPathFollowingReachMode ReachMode, float AcceptanceRadius = UPathFollowingComponent::DefaultAcceptanceRadius) const;

	/** Simple test for stationary agent (used as early finish condition), check if reached given goal.
	*  @param TestGoal - actor to test
	*  @param AcceptanceRadius - allowed 2D distance
	*  @param ReachMode - modifiers for AcceptanceRadius
	*  @param bUseNavAgentGoalLocation - true: if the goal is a nav agent, we will use their nav agent location rather than their actual location
	*/
	bool HasReached(const AActor& TestGoal, EPathFollowingReachMode ReachMode, float AcceptanceRadius = UPathFollowingComponent::DefaultAcceptanceRadius, bool bUseNavAgentGoalLocation = true) const;

	bool HasReachedCurrentTarget(const FVector& CurrentLocation) const;

	///////UPDATE WITH 4.13////////
	// Have to override this to call the correct HasReachCurrentTarget.
	void UpdatePathSegment() override;

	// UPathFollowingComponent Overloads

	int32 DetermineStartingPathPoint(const FNavigationPath* ConsideredPath) const override;

	void FollowPathSegment(float DeltaTime) override;

	// Had to override this to get the correct DebugReachTest.
	virtual void GetDebugStringTokens(TArray<FString>& Tokens, TArray<EPathFollowingDebugTokens::Type>& Flags) const override;

	/** Pause path following.
	*  @param RequestID - request to pause, FAIRequestID::CurrentRequest means pause current request, regardless of its ID */
	void PauseMove(FAIRequestID RequestID = FAIRequestID::CurrentRequest, EPathFollowingVelocityMode VelocityMode = EPathFollowingVelocityMode::Reset) override;

	// Add link to VRMovementComp.
	void SetMovementComponent(UNavMovementComponent* MoveComp) override;

	bool ShouldCheckPathOnResume() const override;   // Fine in 4.13
	bool UpdateBlockDetection   ()       override;   // Fine in 4.13, had to change feet based for both

	// Now has an actor feet call  .......Just a debug reference
	//virtual FAIRequestID RequestMove(FNavPathSharedPtr Path, FRequestCompletedSignature OnComplete, const AActor* DestinationActor = NULL, float AcceptanceRadius = UPathFollowingComponent::DefaultAcceptanceRadius, bool bStopOnOverlap = true, FCustomMoveSharedPtr GameData = NULL) override;

	// This has a foot location when using meta paths, i'm not overriding it yet but this means that meta paths might have slightly bugged implementation.
	/** notify about finished movement */
	//virtual void OnPathFinished(const FPathFollowingResult& Result) override;


	// Declares

	UPROPERTY(transient)
	UVRBaseCharacterMovementComponent* VRMovementComp;
};