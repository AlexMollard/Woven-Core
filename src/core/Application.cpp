#include "pch.hpp"

#include "Application.hpp"
#include "core/Logger.hpp"
#include "graphics/GraphicsSystem.hpp"
#include "physics/PhysicsSystem.hpp"
#include "scheduling/TaskSchedulingSystem.hpp"
#include "window/WindowSystem.hpp"

Application::Application()
      : m_Window(std::make_unique<WindowSystem>()), m_Graphics(std::make_unique<GraphicsSystem>()), m_Physics(std::make_unique<PhysicsSystem>()), m_TaskScheduling(std::make_unique<TaskSchedulingSystem>())
{
}

Application::~Application()
{
}

bool Application::Init()
{
	ZoneScoped;

	Logger::Init();

	if (!m_Window->Initialize())
		return false;

	if (!m_Graphics->Initialize(m_Window->GetWindow()))
		return false;

	if (!m_Physics->Initialize())
		return false;

	if (!m_TaskScheduling->Initialize())
		return false;

	Logger::Info("Application initialized successfully!");
	return true;
}

void Application::Update()
{
	ZoneScoped;
	FrameMark;

	// Update physics
	m_Physics->Update();

	// Schedule physics tasks
	if (m_TaskScheduling->GetWorkerThreadCount() > 0)
	{
		ZoneScopedN("Physics Tasks");
		// TODO: Create physics update tasks and add to scheduler
		// m_TaskScheduling->GetScheduler()->WaitforAll();
	}

	// Update graphics profiler
	m_Graphics->UpdateProfiler();

	const float timeSeconds = SDL_GetTicks() * 0.001f;
	m_Graphics->RenderFrame(timeSeconds);
}

void Application::Shutdown()
{
	ZoneScoped;

	// Wait for any pending tasks
	m_TaskScheduling->WaitAll();

	// Shutdown systems in reverse order
	m_Physics->Shutdown();
	m_Graphics->Shutdown();
	m_Window->Shutdown();

	Logger::Shutdown();
}

SDL_Window* Application::GetWindow() const
{
	return m_Window->GetWindow();
}
