#include "pch.hpp"

#include "core/Logger.hpp"
#include "PhysicsSystem.hpp"

PhysicsSystem::PhysicsSystem()
{
}

PhysicsSystem::~PhysicsSystem()
{
}

bool PhysicsSystem::Initialize()
{
	ZoneScopedN("PhysicsSystem::Initialize");

	JPH::RegisterDefaultAllocator();
	Logger::Debug("Jolt Physics initialized");
	return true;
}

void PhysicsSystem::Shutdown()
{
	ZoneScopedN("PhysicsSystem::Shutdown");
	// Cleanup physics resources if needed
}

void PhysicsSystem::Update()
{
	ZoneScopedN("PhysicsSystem::Update");
	// TODO: Update physics simulation
}
