#include "pch.hpp"

// Volk must be included before VkBootstrap and Vulkan headers
#include <volk.h>

// Vulkan stuff must be included after Volk
#include <SDL3/SDL_vulkan.h>
#include <tracy/TracyVulkan.hpp>
#include <VkBootstrap.h>

#include "core/Logger.hpp"
#include "GraphicsSystem.hpp"

GraphicsSystem::GraphicsSystem()
{
}

GraphicsSystem::~GraphicsSystem()
{
}

bool GraphicsSystem::Initialize(SDL_Window* window)
{
	ZoneScopedN("GraphicsSystem::Initialize");

	m_Window = window;

	// Initialize Volk
	if (volkInitialize() != VK_SUCCESS)
	{
		Logger::Error("Failed to initialize Volk. Is Vulkan installed?");
		return false;
	}

	if (!CreateVulkanInstance(window))
		return false;

	if (!CreateSurface(window))
		return false;

	if (!SelectPhysicalDevice())
		return false;

	if (!CreateLogicalDevice())
		return false;

	if (!GetQueues())
		return false;

	if (!InitializeVulkanMemoryAllocator())
		return false;

	if (!CreateTracyContext())
		return false;

	if (!CreateSwapchain(window))
		return false;

	if (!CreateDepthResources())
		return false;

	if (!CreateHDRRenderTarget())
		return false;

	if (!CreateCommandPools())
		return false;

	if (!CreateSyncPrimitives())
		return false;

	if (!CreateBindlessDescriptors())
		return false;

	if (!CreatePipelineInfrastructure())
		return false;

	return true;
}

void GraphicsSystem::Shutdown()
{
	ZoneScopedN("GraphicsSystem::Shutdown");
	CleanupVulkan();
}

void GraphicsSystem::UpdateProfiler()
{
	if (m_TracyContext)
	{
		TracyVkCollect(m_TracyContext, m_TracyCommandBuffer);
	}
}

// --- Vulkan Initialization Steps ---

bool GraphicsSystem::CreateVulkanInstance(SDL_Window* window)
{
	ZoneScopedN("CreateVulkanInstance");

	// Get SDL required extensions
	Uint32 extCount = 0;
	const char* const* extensions = SDL_Vulkan_GetInstanceExtensions(&extCount);
	if (!extensions)
	{
		Logger::Error("Failed to get Vulkan extensions from SDL");
		return false;
	}

	// Build instance
	vkb::InstanceBuilder builder;
	builder.set_app_name("Woven Core");
	builder.set_engine_name("Woven Engine");
	builder.require_api_version(1, 3, 0);
	builder.enable_extensions(extCount, extensions);

#ifndef NDEBUG
	builder.request_validation_layers(true);
	builder.use_default_debug_messenger();
	builder.set_debug_callback(
	        [](VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT /*type*/, const VkDebugUtilsMessengerCallbackDataEXT* data, void* /*userData*/) -> VkBool32
	        {
		        if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
			        Logger::VulkanError(data->pMessage);
		        else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
			        Logger::VulkanWarning(data->pMessage);
		        else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
			        Logger::VulkanInfo(data->pMessage);
		        return VK_FALSE;
	        });
	builder.set_debug_messenger_severity(VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT);
	builder.add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT);
	builder.add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT);
	builder.add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT);
	builder.add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT);
	builder.add_validation_feature_disable(VK_VALIDATION_FEATURE_DISABLE_CORE_CHECKS_EXT);

	Logger::Info("Validation: GPU-Assisted + Sync + Best Practices + Debug Printf");
#else
	Logger::Info("Validation layers disabled (Release build)");
#endif

	if (auto instanceRet = builder.build())
	{
		m_VkbInstance = std::move(instanceRet).value();
		volkLoadInstance(m_VkbInstance.instance);
	}
	else
	{
		Logger::Error("Failed to create Vulkan Instance: %s", instanceRet.error().message().c_str());
		return false;
	}

	// Log API version
	uint32_t apiVersion = m_VkbInstance.instance_version;
	uint32_t major = VK_VERSION_MAJOR(apiVersion);
	uint32_t minor = VK_VERSION_MINOR(apiVersion);
	uint32_t patch = VK_VERSION_PATCH(apiVersion);
	Logger::Info("Vulkan Instance (API %u.%u.%u)", major, minor, patch);

	return true;
}

bool GraphicsSystem::CreateSurface(SDL_Window* window)
{
	ZoneScopedN("CreateSurface");

	VkSurfaceKHR tempSurface = VK_NULL_HANDLE;
	if (!SDL_Vulkan_CreateSurface(window, m_VkbInstance.instance, nullptr, &tempSurface))
	{
		Logger::Error("Failed to create Vulkan Surface: %s", SDL_GetError());
		return false;
	}

	m_Surface = tempSurface;
	return true;
}

bool GraphicsSystem::SelectPhysicalDevice()
{
	ZoneScopedN("SelectPhysicalDevice");

	VkPhysicalDeviceVulkan11Features required11{};
	required11.shaderDrawParameters = VK_TRUE;
	required11.multiview = VK_TRUE;

	VkPhysicalDeviceVulkan12Features required12{};
	required12.bufferDeviceAddress = VK_TRUE;
	required12.descriptorIndexing = VK_TRUE;
	required12.runtimeDescriptorArray = VK_TRUE;
	required12.descriptorBindingPartiallyBound = VK_TRUE;
	required12.descriptorBindingVariableDescriptorCount = VK_TRUE;
	required12.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
	required12.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE;
	required12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
	required12.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
	required12.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
	required12.descriptorBindingUniformTexelBufferUpdateAfterBind = VK_TRUE;
	required12.descriptorBindingStorageTexelBufferUpdateAfterBind = VK_TRUE;
	required12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
	required12.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
	required12.shaderStorageImageArrayNonUniformIndexing = VK_TRUE;
	required12.shaderUniformBufferArrayNonUniformIndexing = VK_TRUE;
	required12.shaderUniformTexelBufferArrayNonUniformIndexing = VK_TRUE;
	required12.shaderStorageTexelBufferArrayNonUniformIndexing = VK_TRUE;
	required12.timelineSemaphore = VK_TRUE;
	required12.scalarBlockLayout = VK_TRUE;
	required12.uniformBufferStandardLayout = VK_TRUE;
	required12.shaderSubgroupExtendedTypes = VK_TRUE;
	required12.vulkanMemoryModel = VK_TRUE;
	required12.vulkanMemoryModelDeviceScope = VK_TRUE;
	required12.vulkanMemoryModelAvailabilityVisibilityChains = VK_TRUE;

	VkPhysicalDeviceVulkan13Features required13{};
	required13.dynamicRendering = VK_TRUE;
	required13.synchronization2 = VK_TRUE;
	required13.maintenance4 = VK_TRUE;
	required13.shaderDemoteToHelperInvocation = VK_TRUE;
	required13.inlineUniformBlock = VK_TRUE;

	vkb::PhysicalDeviceSelector selector(m_VkbInstance);
	selector.set_surface(m_Surface);
	selector.set_minimum_version(1, 3);
	selector.set_required_features_11(required11);
	selector.set_required_features_12(required12);
	selector.set_required_features_13(required13);
	selector.prefer_gpu_device_type(vkb::PreferredDeviceType::discrete);

	if (auto physicalDeviceRet = selector.select())
	{
		m_VkbPhysicalDevice = std::move(physicalDeviceRet).value();
		Logger::Info("Selected GPU: %s", m_VkbPhysicalDevice.properties.deviceName);
	}
	else
	{
		Logger::Error("Failed to select Physical Device: %s", physicalDeviceRet.error().message().c_str());
		return false;
	}

	// Enable Mesh Shaders (EXT)
	VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT,
		.taskShader = VK_TRUE,
		.meshShader = VK_TRUE,
	};

	if (m_VkbPhysicalDevice.enable_extension_if_present(VK_EXT_MESH_SHADER_EXTENSION_NAME))
	{
		if (m_VkbPhysicalDevice.enable_extension_features_if_present(meshShaderFeatures))
		{
			m_SupportsMeshShaders = true;
			Logger::Info("Enabled VK_EXT_mesh_shader");
		}
		else
		{
			Logger::Warning("VK_EXT_mesh_shader present but features unavailable");
		}
	}
	else
	{
		Logger::Debug("VK_EXT_mesh_shader not available");
	}

	// Enable Descriptor Buffer (optional, next-gen bindless)
	if (m_VkbPhysicalDevice.enable_extension_if_present(VK_EXT_DESCRIPTOR_BUFFER_EXTENSION_NAME))
	{
		m_SupportsDescriptorBuffer = true;
		Logger::Info("Enabled VK_EXT_descriptor_buffer");
	}
	else
	{
		Logger::Debug("VK_EXT_descriptor_buffer not available");
	}

	// Enable Push Descriptor (fast descriptor updates)
	if (m_VkbPhysicalDevice.enable_extension_if_present(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME))
	{
		m_SupportsPushDescriptor = true;
		Logger::Info("Enabled VK_KHR_push_descriptor");
	}
	else
	{
		Logger::Debug("VK_KHR_push_descriptor not available");
	}

	// Enable Graphics Pipeline Library (fast pipeline creation)
	// VK_EXT_graphics_pipeline_library requires VK_KHR_pipeline_library
	if (m_VkbPhysicalDevice.enable_extension_if_present(VK_KHR_PIPELINE_LIBRARY_EXTENSION_NAME))
	{
		m_VkbPhysicalDevice.enable_extension_if_present(VK_EXT_GRAPHICS_PIPELINE_LIBRARY_EXTENSION_NAME);
	}

	// Enable Extended Dynamic State 3
	m_VkbPhysicalDevice.enable_extension_if_present(VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME);

#ifdef VK_KHR_fragment_shading_rate
	VkPhysicalDeviceFragmentShadingRateFeaturesKHR fragmentShadingRateFeatures{
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR,
		.pipelineFragmentShadingRate = VK_TRUE,
		.primitiveFragmentShadingRate = VK_TRUE,
		.attachmentFragmentShadingRate = VK_TRUE,
	};

	if (m_VkbPhysicalDevice.enable_extension_if_present(VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME))
	{
		if (m_VkbPhysicalDevice.enable_extension_features_if_present(fragmentShadingRateFeatures))
		{
			m_SupportsFragmentShadingRate = true;
			Logger::Info("Enabled VK_KHR_fragment_shading_rate");
		}
		else
		{
			Logger::Warning("VK_KHR_fragment_shading_rate present but features unavailable");
		}
	}
	else
	{
		Logger::Debug("VK_KHR_fragment_shading_rate not available");
	}
#else
	Logger::Debug("VK_KHR_fragment_shading_rate headers not available");
#endif

	return true;
}

bool GraphicsSystem::CreateLogicalDevice()
{
	ZoneScopedN("CreateLogicalDevice");

	vkb::DeviceBuilder deviceBuilder(m_VkbPhysicalDevice);

	if (auto deviceRet = deviceBuilder.build())
	{
		m_VkbDevice = std::move(deviceRet).value();
		volkLoadDevice(m_VkbDevice.device);
		return true;
	}
	else
	{
		Logger::Error("Failed to create Vulkan Device: %s", deviceRet.error().message().c_str());
		return false;
	}
}

bool GraphicsSystem::GetQueues()
{
	ZoneScopedN("GetQueues");

	if (auto graphicsQueue = m_VkbDevice.get_queue(vkb::QueueType::graphics))
	{
		if (auto presentQueue = m_VkbDevice.get_queue(vkb::QueueType::present))
		{
			m_GraphicsQueue = std::move(graphicsQueue).value();
			m_PresentQueue = std::move(presentQueue).value();
			Logger::Info("Vulkan Device and Queues ready");
			return true;
		}
		else
		{
			Logger::Error("Failed to get presentation queue");
			return false;
		}
	}
	else
	{
		Logger::Error("Failed to get graphics queue");
		return false;
	}
}

bool GraphicsSystem::InitializeVulkanMemoryAllocator()
{
	ZoneScopedN("InitializeVulkanMemoryAllocator");

	const VmaVulkanFunctions vmaVulkanFunc{
		.vkGetInstanceProcAddr = vkGetInstanceProcAddr,
		.vkGetDeviceProcAddr = vkGetDeviceProcAddr,
	};

	const VmaAllocatorCreateInfo allocatorInfo{
		.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
		.physicalDevice = m_VkbPhysicalDevice.physical_device,
		.device = m_VkbDevice.device,
		.preferredLargeHeapBlockSize = 0,
		.pAllocationCallbacks = nullptr,
		.pDeviceMemoryCallbacks = nullptr,
		.pHeapSizeLimit = nullptr,
		.pVulkanFunctions = &vmaVulkanFunc,
		.instance = m_VkbInstance.instance,
		.vulkanApiVersion = m_VkbInstance.instance_version,
		.pTypeExternalMemoryHandleTypes = nullptr,
	};

	if (vmaCreateAllocator(&allocatorInfo, &m_VmaAllocator) != VK_SUCCESS)
	{
		Logger::Error("Failed to create VMA allocator");
		return false;
	}

	Logger::Info("Vulkan Memory Allocator initialized");
	return true;
}

bool GraphicsSystem::CreateTracyContext()
{
	ZoneScopedN("CreateTracyContext");

	// Create command pool for Tracy
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = m_VkbDevice.get_queue_index(vkb::QueueType::graphics).value();

	if (vkCreateCommandPool(m_VkbDevice.device, &poolInfo, nullptr, &m_TracyCommandPool) != VK_SUCCESS)
	{
		Logger::Error("Failed to create Tracy command pool");
		return false;
	}

	// Allocate command buffer for Tracy
	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.commandPool = m_TracyCommandPool;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	if (vkAllocateCommandBuffers(m_VkbDevice.device, &allocInfo, &m_TracyCommandBuffer) != VK_SUCCESS)
	{
		Logger::Error("Failed to allocate Tracy command buffer");
		return false;
	}

	// Create Tracy Vulkan context
	m_TracyContext = TracyVkContextCalibrated(m_VkbInstance.instance, m_VkbPhysicalDevice.physical_device, m_VkbDevice.device, m_GraphicsQueue, m_TracyCommandBuffer, vkGetInstanceProcAddr, vkGetDeviceProcAddr);

	if (!m_TracyContext)
	{
		Logger::Error("Failed to create Tracy GPU context");
		return false;
	}

	// Name the context for clarity in Tracy UI
	TracyVkContextName(m_TracyContext, "Vulkan Main Context", 21);

	Logger::Info("Tracy GPU profiling initialized");
	return true;
}

// --- Modern Renderer Setup ---

bool GraphicsSystem::CreateSwapchain(SDL_Window* window)
{
	ZoneScopedN("CreateSwapchain");

	// Query surface capabilities
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_VkbPhysicalDevice.physical_device, m_Surface, &surfaceCapabilities) != VK_SUCCESS)
	{
		Logger::Error("Failed to query surface capabilities");
		return false;
	}

	// Choose surface format (prefer SRGB for proper color space)
	uint32_t formatCount;
	vkGetPhysicalDeviceSurfaceFormatsKHR(m_VkbPhysicalDevice.physical_device, m_Surface, &formatCount, nullptr);
	std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
	vkGetPhysicalDeviceSurfaceFormatsKHR(m_VkbPhysicalDevice.physical_device, m_Surface, &formatCount, surfaceFormats.data());

	VkSurfaceFormatKHR selectedFormat = surfaceFormats[0];
	for (const auto& format: surfaceFormats)
	{
		// Prefer SRGB for gamma-correct rendering
		if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			selectedFormat = format;
			break;
		}
		// Fallback to UNORM
		if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
		{
			selectedFormat = format;
		}
	}

	m_SwapchainImageFormat = selectedFormat.format;

	// Choose present mode (prefer MAILBOX for low latency, fallback to FIFO which is always available)
	uint32_t presentModeCount;
	vkGetPhysicalDeviceSurfacePresentModesKHR(m_VkbPhysicalDevice.physical_device, m_Surface, &presentModeCount, nullptr);
	std::vector<VkPresentModeKHR> presentModes(presentModeCount);
	vkGetPhysicalDeviceSurfacePresentModesKHR(m_VkbPhysicalDevice.physical_device, m_Surface, &presentModeCount, presentModes.data());

	VkPresentModeKHR selectedPresentMode = VK_PRESENT_MODE_FIFO_KHR; // Always available per spec
	for (const auto& mode: presentModes)
	{
		// MAILBOX: Low latency triple buffering
		if (mode == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			selectedPresentMode = mode;
			break;
		}
		// IMMEDIATE: Lowest latency but may tear (good for uncapped FPS)
		if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR)
		{
			selectedPresentMode = mode;
		}
	}

	// Determine swapchain extent
	if (surfaceCapabilities.currentExtent.width != UINT32_MAX)
	{
		m_SwapchainExtent = surfaceCapabilities.currentExtent;
	}
	else
	{
		// Window manager doesn't specify extent, query from SDL
		int width, height;
		SDL_GetWindowSize(window, &width, &height);

		VkExtent2D actualExtent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height) };

		// Clamp to supported range
		actualExtent.width = std::clamp(actualExtent.width, surfaceCapabilities.minImageExtent.width, surfaceCapabilities.maxImageExtent.width);
		actualExtent.height = std::clamp(actualExtent.height, surfaceCapabilities.minImageExtent.height, surfaceCapabilities.maxImageExtent.height);

		m_SwapchainExtent = actualExtent;
	}

	// Determine image count (prefer triple buffering if available)
	uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
	if (surfaceCapabilities.maxImageCount > 0 && imageCount > surfaceCapabilities.maxImageCount)
	{
		imageCount = surfaceCapabilities.maxImageCount;
	}

	// Create swapchain
	VkSwapchainCreateInfoKHR createInfo{};
	createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	createInfo.surface = m_Surface;
	createInfo.minImageCount = imageCount;
	createInfo.imageFormat = selectedFormat.format;
	createInfo.imageColorSpace = selectedFormat.colorSpace;
	createInfo.imageExtent = m_SwapchainExtent;
	createInfo.imageArrayLayers = 1;
	createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT; // Transfer for blits/clears
	createInfo.preTransform = surfaceCapabilities.currentTransform;
	createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	createInfo.presentMode = selectedPresentMode;
	createInfo.clipped = VK_TRUE;
	createInfo.oldSwapchain = VK_NULL_HANDLE; // For recreation, could pass old swapchain

	// Handle queue family ownership
	uint32_t queueFamilyIndices[] = { m_VkbDevice.get_queue_index(vkb::QueueType::graphics).value(), m_VkbDevice.get_queue_index(vkb::QueueType::present).value() };

	if (queueFamilyIndices[0] != queueFamilyIndices[1])
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		createInfo.queueFamilyIndexCount = 2;
		createInfo.pQueueFamilyIndices = queueFamilyIndices;
	}
	else
	{
		createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	}

	if (vkCreateSwapchainKHR(m_VkbDevice.device, &createInfo, nullptr, &m_Swapchain) != VK_SUCCESS)
	{
		Logger::Error("Failed to create swapchain");
		return false;
	}

	// Retrieve swapchain images
	uint32_t swapchainImageCount;
	vkGetSwapchainImagesKHR(m_VkbDevice.device, m_Swapchain, &swapchainImageCount, nullptr);
	m_SwapchainImages.resize(swapchainImageCount);
	vkGetSwapchainImagesKHR(m_VkbDevice.device, m_Swapchain, &swapchainImageCount, m_SwapchainImages.data());
	m_SwapchainImageLayouts.assign(swapchainImageCount, VK_IMAGE_LAYOUT_UNDEFINED);

	// Create image views for each swapchain image
	m_SwapchainImageViews.resize(swapchainImageCount);
	for (size_t i = 0; i < swapchainImageCount; i++)
	{
		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = m_SwapchainImages[i];
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = m_SwapchainImageFormat;
		viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		if (vkCreateImageView(m_VkbDevice.device, &viewInfo, nullptr, &m_SwapchainImageViews[i]) != VK_SUCCESS)
		{
			Logger::Error("Failed to create image view for swapchain image %zu", i);
			return false;
		}
	}

	const char* presentModeStr = selectedPresentMode == VK_PRESENT_MODE_MAILBOX_KHR ? "MAILBOX" : selectedPresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR ? "IMMEDIATE" : selectedPresentMode == VK_PRESENT_MODE_FIFO_KHR ? "FIFO" : "OTHER";

	Logger::Info("Swapchain created: %ux%u, %u images, %s mode", m_SwapchainExtent.width, m_SwapchainExtent.height, swapchainImageCount, presentModeStr);

	m_SwapchainOutOfDate = false;
	return true;
}

bool GraphicsSystem::RecordSwapchainClear(VkCommandBuffer cmd, uint32_t imageIndex, const VkClearColorValue& clearColor)
{
	ZoneScopedN("RecordSwapchainClear");

	if (cmd == VK_NULL_HANDLE)
	{
		Logger::Error("Invalid command buffer for swapchain clear");
		return false;
	}

	if (imageIndex >= m_SwapchainImages.size())
	{
		Logger::Error("Swapchain image index out of range: %u", imageIndex);
		return false;
	}

	VkImage swapchainImage = m_SwapchainImages[imageIndex];
	if (swapchainImage == VK_NULL_HANDLE)
	{
		Logger::Error("Swapchain image is null at index %u", imageIndex);
		return false;
	}

	VkImageSubresourceRange range{};
	range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	range.baseMipLevel = 0;
	range.levelCount = 1;
	range.baseArrayLayer = 0;
	range.layerCount = 1;

	// Modern Vulkan 1.3 barrier with synchronization2
	VkImageMemoryBarrier2 toTransfer{};
	toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	toTransfer.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	toTransfer.srcAccessMask = 0;
	toTransfer.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	toTransfer.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	toTransfer.oldLayout = m_SwapchainImageLayouts[imageIndex];
	toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	toTransfer.image = swapchainImage;
	toTransfer.subresourceRange = range;

	VkDependencyInfo depInfo{};
	depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	depInfo.imageMemoryBarrierCount = 1;
	depInfo.pImageMemoryBarriers = &toTransfer;

	vkCmdPipelineBarrier2(cmd, &depInfo);

	vkCmdClearColorImage(cmd, swapchainImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);

	VkImageMemoryBarrier2 toPresent{};
	toPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	toPresent.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	toPresent.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	toPresent.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	toPresent.dstAccessMask = 0;
	toPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	toPresent.image = swapchainImage;
	toPresent.subresourceRange = range;

	depInfo.pImageMemoryBarriers = &toPresent;
	vkCmdPipelineBarrier2(cmd, &depInfo);

	m_SwapchainImageLayouts[imageIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	return true;
}

VkFormat GraphicsSystem::FindDepthFormat()
{
	// Check for optimal depth format support
	const std::vector<VkFormat> candidates = {
		VK_FORMAT_D32_SFLOAT,         // Best: 32-bit float depth
		VK_FORMAT_D32_SFLOAT_S8_UINT, // 32-bit float depth + 8-bit stencil
		VK_FORMAT_D24_UNORM_S8_UINT   // 24-bit depth + 8-bit stencil (widely supported)
	};

	for (VkFormat format: candidates)
	{
		VkFormatProperties props;
		vkGetPhysicalDeviceFormatProperties(m_VkbPhysicalDevice.physical_device, format, &props);

		if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
		{
			return format;
		}
	}

	Logger::Warning("No optimal depth format found, using D32_SFLOAT");
	return VK_FORMAT_D32_SFLOAT;
}

bool GraphicsSystem::CreateDepthResources()
{
	ZoneScopedN("CreateDepthResources");

	// Find supported depth format
	m_DepthFormat = FindDepthFormat();

	// Create depth image
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = m_SwapchainExtent.width;
	imageInfo.extent.height = m_SwapchainExtent.height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = m_DepthFormat;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo allocInfo{};
	allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

	if (vmaCreateImage(m_VmaAllocator, &imageInfo, &allocInfo, &m_DepthImage, &m_DepthImageAllocation, nullptr) != VK_SUCCESS)
	{
		Logger::Error("Failed to create depth image");
		return false;
	}

	// Create depth image view
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = m_DepthImage;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = m_DepthFormat;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	if (vkCreateImageView(m_VkbDevice.device, &viewInfo, nullptr, &m_DepthImageView) != VK_SUCCESS)
	{
		Logger::Error("Failed to create depth image view");
		return false;
	}

	Logger::Info("Depth buffer created: %ux%u, format %d", m_SwapchainExtent.width, m_SwapchainExtent.height, m_DepthFormat);
	return true;
}

void GraphicsSystem::CleanupDepthResources()
{
	ZoneScopedN("CleanupDepthResources");

	if (m_VkbDevice.device == VK_NULL_HANDLE)
		return;

	if (m_DepthImageView != VK_NULL_HANDLE)
	{
		vkDestroyImageView(m_VkbDevice.device, m_DepthImageView, nullptr);
		m_DepthImageView = VK_NULL_HANDLE;
	}

	if (m_DepthImage != VK_NULL_HANDLE)
	{
		vmaDestroyImage(m_VmaAllocator, m_DepthImage, m_DepthImageAllocation);
		m_DepthImage = VK_NULL_HANDLE;
		m_DepthImageAllocation = VK_NULL_HANDLE;
	}
}

bool GraphicsSystem::CreateHDRRenderTarget()
{
	ZoneScopedN("CreateHDRRenderTarget");

	// Create HDR render target image for forward+ rendering
	VkImageCreateInfo imageInfo{};
	imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageInfo.imageType = VK_IMAGE_TYPE_2D;
	imageInfo.extent.width = m_SwapchainExtent.width;
	imageInfo.extent.height = m_SwapchainExtent.height;
	imageInfo.extent.depth = 1;
	imageInfo.mipLevels = 1;
	imageInfo.arrayLayers = 1;
	imageInfo.format = m_HDRFormat;
	imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
	imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VmaAllocationCreateInfo allocInfo{};
	allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

	if (vmaCreateImage(m_VmaAllocator, &imageInfo, &allocInfo, &m_HDRRenderTarget, &m_HDRRenderTargetAllocation, nullptr) != VK_SUCCESS)
	{
		Logger::Error("Failed to create HDR render target");
		return false;
	}

	// Create HDR image view
	VkImageViewCreateInfo viewInfo{};
	viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	viewInfo.image = m_HDRRenderTarget;
	viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	viewInfo.format = m_HDRFormat;
	viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	viewInfo.subresourceRange.baseMipLevel = 0;
	viewInfo.subresourceRange.levelCount = 1;
	viewInfo.subresourceRange.baseArrayLayer = 0;
	viewInfo.subresourceRange.layerCount = 1;

	if (vkCreateImageView(m_VkbDevice.device, &viewInfo, nullptr, &m_HDRRenderTargetView) != VK_SUCCESS)
	{
		Logger::Error("Failed to create HDR render target view");
		return false;
	}

	Logger::Info("HDR render target created: %ux%u, format R16G16B16A16_SFLOAT", m_SwapchainExtent.width, m_SwapchainExtent.height);
	return true;
}

void GraphicsSystem::CleanupHDRRenderTarget()
{
	ZoneScopedN("CleanupHDRRenderTarget");

	if (m_VkbDevice.device == VK_NULL_HANDLE)
		return;

	if (m_HDRRenderTargetView != VK_NULL_HANDLE)
	{
		vkDestroyImageView(m_VkbDevice.device, m_HDRRenderTargetView, nullptr);
		m_HDRRenderTargetView = VK_NULL_HANDLE;
	}

	if (m_HDRRenderTarget != VK_NULL_HANDLE)
	{
		vmaDestroyImage(m_VmaAllocator, m_HDRRenderTarget, m_HDRRenderTargetAllocation);
		m_HDRRenderTarget = VK_NULL_HANDLE;
		m_HDRRenderTargetAllocation = VK_NULL_HANDLE;
	}
}

bool GraphicsSystem::RecreateSwapchain(SDL_Window* window)
{
	ZoneScopedN("RecreateSwapchain");

	// Wait for device to be idle before recreating swapchain
	vkDeviceWaitIdle(m_VkbDevice.device);

	// Clean up old swapchain resources
	CleanupSwapchain();
	CleanupDepthResources();
	CleanupHDRRenderTarget();

	// Create new swapchain and dependent resources
	if (!CreateSwapchain(window))
	{
		Logger::Error("Failed to recreate swapchain");
		return false;
	}

	if (!CreateDepthResources())
	{
		Logger::Error("Failed to recreate depth resources");
		return false;
	}

	if (!CreateHDRRenderTarget())
	{
		Logger::Error("Failed to recreate HDR render target");
		return false;
	}

	m_SwapchainOutOfDate = false;
	m_FramebufferResized = false;
	Logger::Info("Swapchain recreated");
	return true;
}

void GraphicsSystem::CleanupSwapchain()
{
	ZoneScopedN("CleanupSwapchain");

	if (m_VkbDevice.device == VK_NULL_HANDLE)
		return;

	// Destroy image views
	for (auto imageView: m_SwapchainImageViews)
	{
		if (imageView != VK_NULL_HANDLE)
		{
			vkDestroyImageView(m_VkbDevice.device, imageView, nullptr);
		}
	}
	m_SwapchainImageViews.clear();
	m_SwapchainImages.clear();
	m_SwapchainImageLayouts.clear();

	// Destroy swapchain
	if (m_Swapchain != VK_NULL_HANDLE)
	{
		vkDestroySwapchainKHR(m_VkbDevice.device, m_Swapchain, nullptr);
		m_Swapchain = VK_NULL_HANDLE;
	}
}

bool GraphicsSystem::CreateCommandPools()
{
	ZoneScopedN("CreateCommandPools");

	// Get graphics queue family index
	uint32_t graphicsQueueFamily = m_VkbDevice.get_queue_index(vkb::QueueType::graphics).value();

	// Create command pool and buffer for each frame in flight
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // Allow individual command buffer reset
	poolInfo.queueFamilyIndex = graphicsQueueFamily;

	VkCommandBufferAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocInfo.commandBufferCount = 1;

	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		// Create dedicated command pool per frame for lock-free recording
		if (vkCreateCommandPool(m_VkbDevice.device, &poolInfo, nullptr, &m_Frames[i].commandPool) != VK_SUCCESS)
		{
			Logger::Error("Failed to create command pool for frame %u", i);
			return false;
		}

		// Allocate primary command buffer from this frame's pool
		allocInfo.commandPool = m_Frames[i].commandPool;
		if (vkAllocateCommandBuffers(m_VkbDevice.device, &allocInfo, &m_Frames[i].commandBuffer) != VK_SUCCESS)
		{
			Logger::Error("Failed to allocate command buffer for frame %u", i);
			return false;
		}
	}

	Logger::Info("Command pools created: %u frame command buffers (bindless + push constants)", MAX_FRAMES_IN_FLIGHT);
	return true;
}

bool GraphicsSystem::CreateSyncPrimitives()
{
	ZoneScopedN("CreateSyncPrimitives");

	// Create timeline semaphore for frame pacing (Vulkan 1.2+)
	VkSemaphoreTypeCreateInfo timelineInfo{};
	timelineInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
	timelineInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
	timelineInfo.initialValue = 0;

	VkSemaphoreCreateInfo semaphoreInfo{};
	semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreInfo.pNext = &timelineInfo;

	if (vkCreateSemaphore(m_VkbDevice.device, &semaphoreInfo, nullptr, &m_TimelineSemaphore) != VK_SUCCESS)
	{
		Logger::Error("Failed to create timeline semaphore");
		return false;
	}

	// Initialize timeline value
	m_TimelineValue = 0;

	// Create per-frame synchronization primitives
	VkSemaphoreCreateInfo binarySemaphoreInfo{};
	binarySemaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // Start signaled so first frame doesn't wait

	for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
	{
		// Create binary semaphore for swapchain image acquisition
		if (vkCreateSemaphore(m_VkbDevice.device, &binarySemaphoreInfo, nullptr, &m_Frames[i].swapchainAcquireSemaphore) != VK_SUCCESS)
		{
			Logger::Error("Failed to create swapchain acquire semaphore for frame %u", i);
			return false;
		}

		// Create binary semaphore for render completion
		if (vkCreateSemaphore(m_VkbDevice.device, &binarySemaphoreInfo, nullptr, &m_Frames[i].renderCompleteSemaphore) != VK_SUCCESS)
		{
			Logger::Error("Failed to create render complete semaphore for frame %u", i);
			return false;
		}

		// Create fence (optional fallback, timeline semaphores are preferred)
		if (vkCreateFence(m_VkbDevice.device, &fenceInfo, nullptr, &m_Frames[i].renderFence) != VK_SUCCESS)
		{
			Logger::Error("Failed to create render fence for frame %u", i);
			return false;
		}

		// Initialize timeline value for this frame
		m_Frames[i].timelineValue = 0;
	}

	Logger::Info("Synchronization primitives created (timeline + %u frame semaphores)", MAX_FRAMES_IN_FLIGHT);
	return true;
}

bool GraphicsSystem::CreateBindlessDescriptors()
{
	ZoneScopedN("CreateBindlessDescriptors");

	// Define descriptor pool sizes for bindless rendering
	constexpr uint32_t MAX_BINDLESS_TEXTURES = 16384; // 16K textures
	constexpr uint32_t MAX_BINDLESS_SAMPLERS = 128;   // Samplers
	constexpr uint32_t MAX_STORAGE_BUFFERS = 1024;    // Storage buffers for GPU-driven
	constexpr uint32_t MAX_UNIFORM_BUFFERS = 256;     // Uniform buffers

	VkDescriptorPoolSize poolSizes[] = {
		{  VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, MAX_BINDLESS_TEXTURES },
        {        VK_DESCRIPTOR_TYPE_SAMPLER, MAX_BINDLESS_SAMPLERS },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,   MAX_STORAGE_BUFFERS },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,   MAX_UNIFORM_BUFFERS },
        {  VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,                   512 }  // For compute/HDR render targets
	};

	// Create descriptor pool with UPDATE_AFTER_BIND flag
	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
	poolInfo.maxSets = 1; // Single global bindless set
	poolInfo.poolSizeCount = static_cast<uint32_t>(std::size(poolSizes));
	poolInfo.pPoolSizes = poolSizes;

	if (vkCreateDescriptorPool(m_VkbDevice.device, &poolInfo, nullptr, &m_BindlessDescriptorPool) != VK_SUCCESS)
	{
		Logger::Error("Failed to create bindless descriptor pool");
		return false;
	}

	// Create bindless descriptor set layout with UPDATE_AFTER_BIND flags
	VkDescriptorSetLayoutBinding bindings[5] = {};

	// Binding 0: Large array of sampled images (textures)
	bindings[0].binding = 0;
	bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
	bindings[0].descriptorCount = MAX_BINDLESS_TEXTURES;
	bindings[0].stageFlags = VK_SHADER_STAGE_ALL; // Available to all shader stages
	bindings[0].pImmutableSamplers = nullptr;

	// Binding 1: Array of samplers
	bindings[1].binding = 1;
	bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
	bindings[1].descriptorCount = MAX_BINDLESS_SAMPLERS;
	bindings[1].stageFlags = VK_SHADER_STAGE_ALL;
	bindings[1].pImmutableSamplers = nullptr;

	// Binding 2: Storage buffers (for GPU-driven rendering, indirect draw data, etc.)
	bindings[2].binding = 2;
	bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	bindings[2].descriptorCount = MAX_STORAGE_BUFFERS;
	bindings[2].stageFlags = VK_SHADER_STAGE_ALL;
	bindings[2].pImmutableSamplers = nullptr;

	// Binding 3: Uniform buffers
	bindings[3].binding = 3;
	bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	bindings[3].descriptorCount = MAX_UNIFORM_BUFFERS;
	bindings[3].stageFlags = VK_SHADER_STAGE_ALL;
	bindings[3].pImmutableSamplers = nullptr;

	// Binding 4: Storage images (for compute shaders in forward+ light culling)
	bindings[4].binding = 4;
	bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	bindings[4].descriptorCount = 512;
	bindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	bindings[4].pImmutableSamplers = nullptr;

	// Set binding flags for UPDATE_AFTER_BIND and PARTIALLY_BOUND
	VkDescriptorBindingFlags bindingFlags[5] = {
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT,
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
		VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT,
	};

	VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsInfo{};
	bindingFlagsInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	bindingFlagsInfo.bindingCount = static_cast<uint32_t>(std::size(bindings));
	bindingFlagsInfo.pBindingFlags = bindingFlags;

	VkDescriptorSetLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	layoutInfo.pNext = &bindingFlagsInfo;
	layoutInfo.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
	layoutInfo.bindingCount = static_cast<uint32_t>(std::size(bindings));
	layoutInfo.pBindings = bindings;

	if (vkCreateDescriptorSetLayout(m_VkbDevice.device, &layoutInfo, nullptr, &m_BindlessDescriptorSetLayout) != VK_SUCCESS)
	{
		Logger::Error("Failed to create bindless descriptor set layout");
		return false;
	}

	// Allocate descriptor set with variable descriptor count
	uint32_t variableDescriptorCount = MAX_BINDLESS_TEXTURES;
	VkDescriptorSetVariableDescriptorCountAllocateInfo variableDescriptorCountInfo{};
	variableDescriptorCountInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO;
	variableDescriptorCountInfo.descriptorSetCount = 1;
	variableDescriptorCountInfo.pDescriptorCounts = &variableDescriptorCount;

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.pNext = &variableDescriptorCountInfo;
	allocInfo.descriptorPool = m_BindlessDescriptorPool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &m_BindlessDescriptorSetLayout;

	if (vkAllocateDescriptorSets(m_VkbDevice.device, &allocInfo, &m_BindlessDescriptorSet) != VK_SUCCESS)
	{
		Logger::Error("Failed to allocate bindless descriptor set");
		return false;
	}

	Logger::Info("Bindless descriptors created: %u textures, %u samplers, %u storage buffers, %u uniform buffers", MAX_BINDLESS_TEXTURES, MAX_BINDLESS_SAMPLERS, MAX_STORAGE_BUFFERS, MAX_UNIFORM_BUFFERS);

	return true;
}

bool GraphicsSystem::CreatePipelineInfrastructure()
{
	ZoneScopedN("CreatePipelineInfrastructure");

	// Create a global pipeline layout using bindless descriptors and push constants
	VkPushConstantRange pushConstantRange{};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_ALL;
	pushConstantRange.offset = 0;
	pushConstantRange.size = 128; // Reserve 128 bytes for per-draw data

	VkPipelineLayoutCreateInfo layoutInfo{};
	layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	layoutInfo.setLayoutCount = 1;
	layoutInfo.pSetLayouts = &m_BindlessDescriptorSetLayout;
	layoutInfo.pushConstantRangeCount = 1;
	layoutInfo.pPushConstantRanges = &pushConstantRange;

	if (vkCreatePipelineLayout(m_VkbDevice.device, &layoutInfo, nullptr, &m_GlobalPipelineLayout) != VK_SUCCESS)
	{
		Logger::Error("Failed to create global pipeline layout");
		return false;
	}

	// Create pipeline cache (empty for now, can be populated from disk later)
	VkPipelineCacheCreateInfo cacheInfo{};
	cacheInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	cacheInfo.initialDataSize = 0;
	cacheInfo.pInitialData = nullptr;

	if (vkCreatePipelineCache(m_VkbDevice.device, &cacheInfo, nullptr, &m_PipelineCache) != VK_SUCCESS)
	{
		Logger::Error("Failed to create pipeline cache");
		return false;
	}

	Logger::Info("Pipeline infrastructure created (bindless layout + push constants)");
	return true;
}

// --- Frame Presentation ---

bool GraphicsSystem::BeginFrame(uint32_t& outImageIndex)
{
	ZoneScopedN("BeginFrame");

	// Handle swapchain recreation if needed
	if (m_SwapchainOutOfDate || m_FramebufferResized)
	{
		if (!RecreateSwapchain(m_Window))
		{
			return false;
		}
	}

	// Get current frame resources
	FrameData& frame = m_Frames[m_CurrentFrameIndex];

	// Wait for GPU to finish with this frame using fence
	// This ensures the GPU isn't still executing commands from the last time we used this frame
	if (frame.renderFence != VK_NULL_HANDLE)
	{
		if (vkWaitForFences(m_VkbDevice.device, 1, &frame.renderFence, VK_TRUE, UINT64_MAX) != VK_SUCCESS)
		{
			Logger::Error("Failed to wait for render fence");
			return false;
		}
		// Reset fence for next use
		if (vkResetFences(m_VkbDevice.device, 1, &frame.renderFence) != VK_SUCCESS)
		{
			Logger::Error("Failed to reset render fence");
			return false;
		}
	}

	// Acquire next swapchain image
	VkResult result = vkAcquireNextImageKHR(m_VkbDevice.device, m_Swapchain, UINT64_MAX, frame.swapchainAcquireSemaphore, VK_NULL_HANDLE, &outImageIndex);

	if (result == VK_ERROR_OUT_OF_DATE_KHR)
	{
		m_SwapchainOutOfDate = true;
		return false;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
	{
		Logger::Error("Failed to acquire swapchain image");
		return false;
	}

	// Reset and begin command buffer
	if (frame.commandBuffer == VK_NULL_HANDLE)
	{
		Logger::Error("Invalid command buffer for frame %u", m_CurrentFrameIndex);
		return false;
	}

	vkResetCommandBuffer(frame.commandBuffer, 0);

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	if (vkBeginCommandBuffer(frame.commandBuffer, &beginInfo) != VK_SUCCESS)
	{
		Logger::Error("Failed to begin command buffer");
		return false;
	}

	return true;
}

bool GraphicsSystem::EndFrame(uint32_t imageIndex)
{
	ZoneScopedN("EndFrame");

	FrameData& frame = m_Frames[m_CurrentFrameIndex];

	// End command buffer recording
	if (vkEndCommandBuffer(frame.commandBuffer) != VK_SUCCESS)
	{
		Logger::Error("Failed to end command buffer");
		return false;
	}

	// Classical Vulkan synchronization: semaphore for presentation, fence for CPU-GPU sync
	// Standard VkSubmitInfo (compatible with all drivers)
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	// Wait for swapchain image to be acquired
	VkPipelineStageFlags waitStages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &frame.swapchainAcquireSemaphore;
	submitInfo.pWaitDstStageMask = &waitStages;

	// Submit command buffer
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &frame.commandBuffer;

	// Signal when rendering is complete (for presentation)
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &frame.renderCompleteSemaphore;

	// Submit with fence for CPU-GPU synchronization
	if (vkQueueSubmit(m_GraphicsQueue, 1, &submitInfo, frame.renderFence) != VK_SUCCESS)
	{
		Logger::Error("Failed to submit command buffer");
		return false;
	}

	// Present to screen (wait for rendering to complete)
	VkPresentInfoKHR presentInfo{};
	presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = &frame.renderCompleteSemaphore;
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = &m_Swapchain;
	presentInfo.pImageIndices = &imageIndex;

	VkResult result = vkQueuePresentKHR(m_PresentQueue, &presentInfo);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_FramebufferResized)
	{
		m_SwapchainOutOfDate = true;
	}
	else if (result != VK_SUCCESS)
	{
		Logger::Error("Failed to present swapchain image: %d", result);
		return false;
	}

	// Advance to next frame (will be waited on in next BeginFrame)
	m_CurrentFrameIndex = (m_CurrentFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;

	return true;
}

void GraphicsSystem::HandleResize(SDL_Window* window)
{
	ZoneScopedN("HandleResize");

	// Check if window is minimized
	int width = 0, height = 0;
	SDL_GetWindowSize(window, &width, &height);

	// Wait while window is minimized
	while (width == 0 || height == 0)
	{
		SDL_GetWindowSize(window, &width, &height);
		SDL_WaitEvent(nullptr);
	}

	m_FramebufferResized = true;
	m_SwapchainOutOfDate = true;

	Logger::Info("Window resized to %dx%d", width, height);
}

// --- Cleanup Helpers ---

void GraphicsSystem::CleanupVulkan()
{
	ZoneScopedN("CleanupVulkan");

	if (m_VkbDevice.device != VK_NULL_HANDLE)
	{
		vkDeviceWaitIdle(m_VkbDevice.device);

		// Destroy pipeline infrastructure
		if (m_PipelineCache != VK_NULL_HANDLE)
		{
			vkDestroyPipelineCache(m_VkbDevice.device, m_PipelineCache, nullptr);
			m_PipelineCache = VK_NULL_HANDLE;
		}

		if (m_GlobalPipelineLayout != VK_NULL_HANDLE)
		{
			vkDestroyPipelineLayout(m_VkbDevice.device, m_GlobalPipelineLayout, nullptr);
			m_GlobalPipelineLayout = VK_NULL_HANDLE;
		}

		// Destroy bindless descriptors
		if (m_BindlessDescriptorPool != VK_NULL_HANDLE)
		{
			vkDestroyDescriptorPool(m_VkbDevice.device, m_BindlessDescriptorPool, nullptr);
			m_BindlessDescriptorPool = VK_NULL_HANDLE;
		}

		if (m_BindlessDescriptorSetLayout != VK_NULL_HANDLE)
		{
			vkDestroyDescriptorSetLayout(m_VkbDevice.device, m_BindlessDescriptorSetLayout, nullptr);
			m_BindlessDescriptorSetLayout = VK_NULL_HANDLE;
		}

		// Destroy sync primitives
		if (m_TimelineSemaphore != VK_NULL_HANDLE)
		{
			vkDestroySemaphore(m_VkbDevice.device, m_TimelineSemaphore, nullptr);
			m_TimelineSemaphore = VK_NULL_HANDLE;
		}

		for (auto& frame: m_Frames)
		{
			if (frame.renderFence != VK_NULL_HANDLE)
			{
				vkDestroyFence(m_VkbDevice.device, frame.renderFence, nullptr);
				frame.renderFence = VK_NULL_HANDLE;
			}
			if (frame.renderCompleteSemaphore != VK_NULL_HANDLE)
			{
				vkDestroySemaphore(m_VkbDevice.device, frame.renderCompleteSemaphore, nullptr);
				frame.renderCompleteSemaphore = VK_NULL_HANDLE;
			}
			if (frame.swapchainAcquireSemaphore != VK_NULL_HANDLE)
			{
				vkDestroySemaphore(m_VkbDevice.device, frame.swapchainAcquireSemaphore, nullptr);
				frame.swapchainAcquireSemaphore = VK_NULL_HANDLE;
			}
			if (frame.commandPool != VK_NULL_HANDLE)
			{
				vkDestroyCommandPool(m_VkbDevice.device, frame.commandPool, nullptr);
				frame.commandPool = VK_NULL_HANDLE;
			}
		}

		// Destroy swapchain and render targets
		CleanupSwapchain();
		CleanupDepthResources();
		CleanupHDRRenderTarget();

		// Destroy VMA allocator
		if (m_VmaAllocator != VK_NULL_HANDLE)
		{
			vmaDestroyAllocator(m_VmaAllocator);
			m_VmaAllocator = VK_NULL_HANDLE;
		}

		// Destroy Tracy context
		if (m_TracyContext)
		{
			TracyVkDestroy(m_TracyContext);
			m_TracyContext = nullptr;
		}

		// Destroy Tracy command resources
		if (m_TracyCommandPool != VK_NULL_HANDLE)
		{
			vkDestroyCommandPool(m_VkbDevice.device, m_TracyCommandPool, nullptr);
			m_TracyCommandPool = VK_NULL_HANDLE;
			m_TracyCommandBuffer = VK_NULL_HANDLE;
		}

		vkb::destroy_device(m_VkbDevice);
	}

	if (m_Surface != VK_NULL_HANDLE)
	{
		vkDestroySurfaceKHR(m_VkbInstance.instance, m_Surface, nullptr);
	}

	if (m_VkbInstance.instance != VK_NULL_HANDLE)
	{
		vkb::destroy_instance(m_VkbInstance);
	}
}
