// Fill out your copyright notice in the Description page of Project Settings.

// Parent Header
#include "Grippables/GrippablePhysicsReplication.h"

// Unreal
#include "UObject/ObjectMacros.h"
#include "UObject/Interface.h"

// VREP



// Cpp Only

// I cannot dynamic cast without RTTI so I am using a static var as a declarative in case the user removed our custom replicator.
// We don't want our casts to cause issues..
namespace VRPhysicsReplicationStatics
{
	static bool bHasVRPhysicsReplication = false;
}


// FPhysicsReplicationVR

// Public

// Constructor

FPhysicsReplicationVR::FPhysicsReplicationVR(FPhysScene* PhysScene) : FPhysicsReplication(PhysScene)
{
	VRPhysicsReplicationStatics::bHasVRPhysicsReplication = true;
}

// Functions

bool FPhysicsReplicationVR::IsInitialized()
{
	return VRPhysicsReplicationStatics::bHasVRPhysicsReplication;
}