#pragma once

#include "pch.hpp"

#include <vk_mem_alloc.h>
#include <VkBootstrap.h>

// Forward declare Tracy context
namespace tracy
{
	class VkCtx;
}

// Constants for frame-in-flight management
constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

// Per-frame resources
struct FrameData
{
	VkCommandPool commandPool = VK_NULL_HANDLE;
	VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

	// Modern sync primitives
	VkSemaphore swapchainAcquireSemaphore = VK_NULL_HANDLE;
	VkSemaphore renderCompleteSemaphore = VK_NULL_HANDLE;
	VkFence renderFence = VK_NULL_HANDLE; // Optional: used if not using timeline semaphores

	// Timeline semaphore value for this frame (Vulkan 1.2+)
	uint64_t timelineValue = 0;
};

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

	// Frame management
	uint32_t GetCurrentFrameIndex() const
	{
		return m_CurrentFrameIndex;
	}

	FrameData& GetCurrentFrame()
	{
		return m_Frames[m_CurrentFrameIndex];
	}

	// Swapchain accessors
	VkSwapchainKHR GetSwapchain() const
	{
		return m_Swapchain;
	}

	VkFormat GetSwapchainFormat() const
	{
		return m_SwapchainImageFormat;
	}

	VkExtent2D GetSwapchainExtent() const
	{
		return m_SwapchainExtent;
	}

	uint32_t GetSwapchainImageCount() const
	{
		return static_cast<uint32_t>(m_SwapchainImages.size());
	}

	VkImage GetSwapchainImage(uint32_t index) const
	{
		return index < m_SwapchainImages.size() ? m_SwapchainImages[index] : VK_NULL_HANDLE;
	}

	// Depth buffer accessors
	VkImage GetDepthImage() const
	{
		return m_DepthImage;
	}

	VkImageView GetDepthImageView() const
	{
		return m_DepthImageView;
	}

	VkFormat GetDepthFormat() const
	{
		return m_DepthFormat;
	}

	// HDR render target accessors (for forward+, HDR rendering)
	VkImage GetHDRRenderTarget() const
	{
		return m_HDRRenderTarget;
	}

	VkImageView GetHDRRenderTargetView() const
	{
		return m_HDRRenderTargetView;
	}

	VkFormat GetHDRFormat() const
	{
		return m_HDRFormat;
	}

	// Bindless descriptor set
	VkDescriptorSet GetBindlessDescriptorSet() const
	{
		return m_BindlessDescriptorSet;
	}

	// Frame presentation
	bool BeginFrame(uint32_t& outImageIndex);
	bool EndFrame(uint32_t imageIndex);
	bool RecordSwapchainClear(VkCommandBuffer cmd, uint32_t imageIndex, const VkClearColorValue& clearColor);

	// Window resize handling
	void HandleResize(SDL_Window* window);

	bool IsSwapchainOutOfDate() const
	{
		return m_SwapchainOutOfDate;
	}

	// Feature support queries
	bool SupportsMeshShaders() const
	{
		return m_SupportsMeshShaders;
	}

	bool SupportsDescriptorBuffer() const
	{
		return m_SupportsDescriptorBuffer;
	}

private:
	// Initialization helpers
	bool CreateVulkanInstance(SDL_Window* window);
	bool CreateSurface(SDL_Window* window);
	bool SelectPhysicalDevice();
	bool CreateLogicalDevice();
	bool GetQueues();
	bool InitializeVulkanMemoryAllocator();
	bool CreateTracyContext();

	// Modern renderer setup
	bool CreateSwapchain(SDL_Window* window);
	bool RecreateSwapchain(SDL_Window* window);
	void CleanupSwapchain();
	bool CreateDepthResources();
	void CleanupDepthResources();
	bool CreateHDRRenderTarget();
	void CleanupHDRRenderTarget();
	VkFormat FindDepthFormat();
	bool CreateCommandPools();
	bool CreateSyncPrimitives();
	bool CreateBindlessDescriptors();
	bool CreatePipelineInfrastructure();

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

	// Swapchain
	VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
	std::vector<VkImage> m_SwapchainImages;
	std::vector<VkImageView> m_SwapchainImageViews;
	std::vector<VkImageLayout> m_SwapchainImageLayouts;
	VkFormat m_SwapchainImageFormat = VK_FORMAT_UNDEFINED;
	VkExtent2D m_SwapchainExtent = {};

	// Depth buffer (required for forward+ and all 3D rendering)
	VkImage m_DepthImage = VK_NULL_HANDLE;
	VkImageView m_DepthImageView = VK_NULL_HANDLE;
	VmaAllocation m_DepthImageAllocation = VK_NULL_HANDLE;
	VkFormat m_DepthFormat = VK_FORMAT_UNDEFINED;

	// HDR render target (for forward+, tone mapping, post-processing)
	VkImage m_HDRRenderTarget = VK_NULL_HANDLE;
	VkImageView m_HDRRenderTargetView = VK_NULL_HANDLE;
	VmaAllocation m_HDRRenderTargetAllocation = VK_NULL_HANDLE;
	VkFormat m_HDRFormat = VK_FORMAT_R16G16B16A16_SFLOAT; // HDR format

	// Frame-in-flight management
	FrameData m_Frames[MAX_FRAMES_IN_FLIGHT];
	uint32_t m_CurrentFrameIndex = 0;

	// Timeline semaphore for modern sync
	VkSemaphore m_TimelineSemaphore = VK_NULL_HANDLE;
	uint64_t m_TimelineValue = 0;

	// Bindless descriptors
	VkDescriptorPool m_BindlessDescriptorPool = VK_NULL_HANDLE;
	VkDescriptorSetLayout m_BindlessDescriptorSetLayout = VK_NULL_HANDLE;
	VkDescriptorSet m_BindlessDescriptorSet = VK_NULL_HANDLE;

	// Pipeline infrastructure
	VkPipelineLayout m_GlobalPipelineLayout = VK_NULL_HANDLE;
	VkPipelineCache m_PipelineCache = VK_NULL_HANDLE;

	// Feature support flags
	bool m_SupportsMeshShaders = false;
	bool m_SupportsDescriptorBuffer = false;
	bool m_SupportsFragmentShadingRate = false;
	bool m_SupportsPushDescriptor = false;

	// Window state
	bool m_SwapchainOutOfDate = false;
	bool m_FramebufferResized = false;
	SDL_Window* m_Window = nullptr;
};
