#pragma once

#include "pch.hpp"

#include <vk_mem_alloc.h>
#include <VkBootstrap.h>

// Forward declare Tracy context
namespace tracy
{
	class VkCtx;
}

class GraphicsSystem
{
public:
	GraphicsSystem();
	~GraphicsSystem();

	bool Initialize(SDL_Window* window);
	void Shutdown();

	// Accessors
	VkInstance GetInstance() const
	{
		return m_VkbInstance.instance;
	}

	VkPhysicalDevice GetPhysicalDevice() const
	{
		return m_VkbPhysicalDevice.physical_device;
	}

	VkDevice GetDevice() const
	{
		return m_VkbDevice.device;
	}

	VkSurfaceKHR GetSurface() const
	{
		return m_Surface;
	}

	VkQueue GetGraphicsQueue() const
	{
		return m_GraphicsQueue;
	}

	VkQueue GetPresentQueue() const
	{
		return m_PresentQueue;
	}

	VmaAllocator GetAllocator() const
	{
		return m_VmaAllocator;
	}

	tracy::VkCtx* GetTracyContext() const
	{
		return m_TracyContext;
	}

	VkCommandBuffer GetTracyCommandBuffer() const
	{
		return m_TracyCommandBuffer;
	}

	// Profiling
	void UpdateProfiler();

private:
	// Initialization helpers
	bool CreateVulkanInstance(SDL_Window* window);
	bool CreateSurface(SDL_Window* window);
	bool SelectPhysicalDevice();
	bool CreateLogicalDevice();
	bool GetQueues();
	bool InitializeVulkanMemoryAllocator();
	bool CreateTracyContext();

	// Cleanup helpers
	void CleanupVulkan();

private:
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
};
