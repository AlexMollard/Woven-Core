#include "pch.hpp"

#include <cmath>

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

	// Render a time-varying clear color to validate per-frame rendering
	uint32_t imageIndex = 0;
	if (!m_Graphics->BeginFrame(imageIndex))
	{
		return;
	}

	FrameData& frame = m_Graphics->GetCurrentFrame();
	VkCommandBuffer cmd = frame.commandBuffer;
	VkImage swapchainImage = m_Graphics->GetSwapchainImage(imageIndex);
	if (swapchainImage == VK_NULL_HANDLE)
	{
		Logger::Error("Invalid swapchain image index %u", imageIndex);
		m_Graphics->EndFrame(imageIndex);
		return;
	}
	if (cmd == VK_NULL_HANDLE)
	{
		Logger::Error("Invalid command buffer for frame %u", m_Graphics->GetCurrentFrameIndex());
		m_Graphics->EndFrame(imageIndex);
		return;
	}
	if (!vkCmdPipelineBarrier || !vkCmdClearColorImage)
	{
		Logger::Error("Vulkan command pointers not loaded (vkCmdPipelineBarrier/vkCmdClearColorImage)");
		m_Graphics->EndFrame(imageIndex);
		return;
	}

	const float timeSeconds = SDL_GetTicks() * 0.001f;
	const float r = 0.5f + 0.5f * std::sin(timeSeconds);
	const float g = 0.5f + 0.5f * std::sin(timeSeconds * 1.3f);
	const float b = 0.5f + 0.5f * std::cos(timeSeconds * 0.7f);

	VkClearColorValue clearColor{};
	clearColor.float32[0] = r;
	clearColor.float32[1] = g;
	clearColor.float32[2] = b;
	clearColor.float32[3] = 1.0f;

	if (!m_Graphics->RecordSwapchainClear(cmd, imageIndex, clearColor))
	{
		m_Graphics->EndFrame(imageIndex);
		return;
	}

	m_Graphics->EndFrame(imageIndex);
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
