#pragma once

#include <vk_mem_alloc.h>
#include <VkBootstrap.h>

#include "pch.hpp"

// Forward declare Tracy context
namespace tracy
{
	class VkCtx;
}

class Application
{
public:
	Application();
	~Application();

	// Lifecycle methods
	bool Init();
	void Update();
	void Shutdown();

	// Accessors
	SDL_Window* GetWindow() const
	{
		return m_Window;
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
	// Initialization helpers
	bool InitSDL();
	bool InitVulkan();
	bool InitPhysics();
	bool InitTaskScheduler();

	// Vulkan initialization steps
	bool CreateVulkanInstance();
	bool CreateSurface();
	bool SelectPhysicalDevice();
	bool CreateLogicalDevice();
	bool GetQueues();
	bool InitializeVulkanMemoryAllocator();
	bool CreateTracyContext();

	// Cleanup helpers
	void CleanupVulkan();

private:
	// SDL
	SDL_Window* m_Window = nullptr;
	bool m_ShouldClose = false;

	// Vulkan Core
	vkb::Instance m_VkbInstance;
	vkb::PhysicalDevice m_VkbPhysicalDevice;
	vkb::Device m_VkbDevice;

	// Vulkan Memory Allocator
	VmaAllocator m_VmaAllocator = VK_NULL_HANDLE;

	VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
	VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
	VkQueue m_PresentQueue = VK_NULL_HANDLE;

	// Tracy GPU Profiling
	tracy::VkCtx* m_TracyContext = nullptr;
	VkCommandPool m_TracyCommandPool = VK_NULL_HANDLE;
	VkCommandBuffer m_TracyCommandBuffer = VK_NULL_HANDLE;

	// Task Scheduling (enkiTS)
	enki::TaskScheduler m_TaskScheduler;
};
