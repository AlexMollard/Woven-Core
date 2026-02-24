#pragma once

#include "pch.hpp"

class PhysicsSystem
{
public:
	PhysicsSystem();
	~PhysicsSystem();

	bool Initialize();
	void Shutdown();

	void Update();

private:
};
