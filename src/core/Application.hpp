#pragma once

#include "pch.hpp"

// Forward declarations
class WindowSystem;
class GraphicsSystem;
class PhysicsSystem;
class TaskSchedulingSystem;

class Application
{
public:
	Application();
	~Application();

	// Lifecycle methods
	bool Init();
	void Update();
	void Shutdown();
	void HandleEvent(const SDL_Event& event);

	// Accessors
	SDL_Window* GetWindow() const;

	WindowSystem* GetWindowSystem() const
	{
		return m_Window.get();
	}

	GraphicsSystem* GetGraphicsSystem() const
	{
		return m_Graphics.get();
	}

	PhysicsSystem* GetPhysicsSystem() const
	{
		return m_Physics.get();
	}

	TaskSchedulingSystem* GetTaskSchedulingSystem() const
	{
		return m_TaskScheduling.get();
	}

	bool ShouldClose() const
	{
		return m_ShouldClose;
	}

	void RequestClose()
	{
		m_ShouldClose = true;
	}

private:
	std::unique_ptr<WindowSystem> m_Window;
	std::unique_ptr<GraphicsSystem> m_Graphics;
	std::unique_ptr<PhysicsSystem> m_Physics;
	std::unique_ptr<TaskSchedulingSystem> m_TaskScheduling;

	bool m_ShouldClose = false;
};
