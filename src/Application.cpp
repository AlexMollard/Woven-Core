#include "pch.hpp"

// Volk must be included before VkBootstrap and Vulkan headers
#include <volk.h>

// Vulkan stuff must be included after Volk
#include <SDL3/SDL_vulkan.h>
#include <tracy/TracyVulkan.hpp>
#include <VkBootstrap.h>

#include "Application.hpp"
#include "Logger.hpp"

Application::Application()
{
}

Application::~Application()
{
}

bool Application::Init()
{
	ZoneScoped;

	Logger::Init();

	if (!InitSDL())
		return false;

	if (!InitVulkan())
		return false;

	if (!InitPhysics())
		return false;

	if (!InitTaskScheduler())
		return false;

	Logger::Info("Application initialized successfully!");
	return true;
}

void Application::Update()
{
	ZoneScoped;
	FrameMark;

	// Schedule physics tasks
	if (m_TaskScheduler.GetNumTaskThreads() > 0)
	{
		ZoneScopedN("Physics Tasks");
		// TODO: Create physics update tasks and add to scheduler
		// m_TaskScheduler.WaitforAll();
	}

	// Collect GPU profiling data
	if (m_TracyContext)
	{
		TracyVkCollect(m_TracyContext, m_TracyCommandBuffer);
	}

	// TODO: Update physics
	// TODO: Record command buffers
	// TODO: Present
}

void Application::Shutdown()
{
	ZoneScoped;

	// Wait for any pending tasks
	m_TaskScheduler.WaitforAll();

	CleanupVulkan();

	if (m_Window)
	{
		SDL_DestroyWindow(m_Window);
		m_Window = nullptr;
	}

	SDL_Quit();

	Logger::Shutdown();
}

// --- Initialization Helpers ---

bool Application::InitSDL()
{
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
	{
		Logger::Error("Failed to initialize SDL: %s", SDL_GetError());
		return false;
	}

	m_Window = SDL_CreateWindow("Woven Core", 1920, 1080, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	if (!m_Window)
	{
		Logger::Error("Failed to create window: %s", SDL_GetError());
		return false;
	}

	Logger::Info("SDL initialized (1920x1080, Vulkan)");
	return true;
}

bool Application::InitVulkan()
{
	ZoneScopedN("InitVulkan");

	// Initialize Volk
	if (volkInitialize() != VK_SUCCESS)
	{
		Logger::Error("Failed to initialize Volk. Is Vulkan installed?");
		return false;
	}

	if (!CreateVulkanInstance())
		return false;

	if (!CreateSurface())
		return false;

	if (!SelectPhysicalDevice())
		return false;

	if (!CreateLogicalDevice())
		return false;

	if (!GetQueues())
		return false;

	if (!CreateTracyContext())
		return false;

	return true;
}

bool Application::InitPhysics()
{
	ZoneScopedN("InitPhysics");
	JPH::RegisterDefaultAllocator();
	Logger::Debug("Jolt Physics initialized");
	return true;
}

bool Application::InitTaskScheduler()
{
	ZoneScopedN("InitTaskScheduler");

	enki::TaskSchedulerConfig config;
	m_TaskScheduler.Initialize(config);

	uint32_t numThreads = m_TaskScheduler.GetNumTaskThreads();
	Logger::Info("Task Scheduler initialized with %u worker threads", numThreads);
	return true;
}

// --- Vulkan Initialization Steps ---

bool Application::CreateVulkanInstance()
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
	vkb::InstanceBuilder instanceBuilder;
	instanceBuilder.set_app_name("Woven Core").set_engine_name("Woven Engine").require_api_version(1, 3, 0).enable_extensions(extCount, extensions);

#ifndef NDEBUG
	// Debug builds: enhanced validation with best practices and synchronization checks
	instanceBuilder.request_validation_layers(true)
	        .use_default_debug_messenger()
	        .set_debug_callback(
	                [](VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT* data, void* userData) -> VkBool32
	                {
		                if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
			                Logger::VulkanError(data->pMessage);
		                else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
			                Logger::VulkanWarning(data->pMessage);
		                else if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
			                Logger::VulkanInfo(data->pMessage);

		                return VK_FALSE;
	                })
	        .set_debug_messenger_severity(VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
	        .add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT)
	        .add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT)
	        .add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT)
	        .add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT)
	        .add_validation_feature_disable(VK_VALIDATION_FEATURE_DISABLE_CORE_CHECKS_EXT);

	Logger::Info("Validation: GPU-Assisted + Sync + Best Practices + Debug Printf");
#else
	// Release builds: no validation overhead
	Logger::Info("Validation layers disabled (Release build)");
#endif

	auto instanceRet = instanceBuilder.build();

	if (!instanceRet)
	{
		Logger::Error("Failed to create Vulkan Instance: %s", instanceRet.error().message().c_str());
		return false;
	}

	m_VkbInstance = instanceRet.value();
	volkLoadInstance(m_VkbInstance.instance);

	// Log API version
	uint32_t apiVersion = m_VkbInstance.instance_version;
	uint32_t major = VK_VERSION_MAJOR(apiVersion);
	uint32_t minor = VK_VERSION_MINOR(apiVersion);
	uint32_t patch = VK_VERSION_PATCH(apiVersion);
	Logger::Info("Vulkan Instance (API %u.%u.%u)", major, minor, patch);

	return true;
}

bool Application::CreateSurface()
{
	ZoneScopedN("CreateSurface");

	VkSurfaceKHR tempSurface = VK_NULL_HANDLE;
	if (!SDL_Vulkan_CreateSurface(m_Window, m_VkbInstance.instance, nullptr, &tempSurface))
	{
		Logger::Error("Failed to create Vulkan Surface: %s", SDL_GetError());
		return false;
	}

	m_Surface = tempSurface;
	return true;
}

bool Application::SelectPhysicalDevice()
{
	ZoneScopedN("SelectPhysicalDevice");

	VkPhysicalDeviceVulkan11Features required11{};
	required11.shaderDrawParameters = VK_TRUE;

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

	vkb::PhysicalDeviceSelector selector(m_VkbInstance);
	auto physicalDeviceRet = selector.set_surface(m_Surface).set_minimum_version(1, 3).set_required_features_11(required11).set_required_features_12(required12).set_required_features_13(required13).prefer_gpu_device_type(vkb::PreferredDeviceType::discrete).select();

	if (!physicalDeviceRet)
	{
		Logger::Error("Failed to select Physical Device: %s", physicalDeviceRet.error().message().c_str());
		return false;
	}

	m_VkbPhysicalDevice = physicalDeviceRet.value();
	Logger::Info("Selected GPU: %s", m_VkbPhysicalDevice.properties.deviceName);

#ifdef VK_KHR_fragment_shading_rate
	VkPhysicalDeviceFragmentShadingRateFeaturesKHR fragmentShadingRateFeatures{};
	fragmentShadingRateFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADING_RATE_FEATURES_KHR;
	fragmentShadingRateFeatures.pipelineFragmentShadingRate = VK_TRUE;
	fragmentShadingRateFeatures.primitiveFragmentShadingRate = VK_TRUE;
	fragmentShadingRateFeatures.attachmentFragmentShadingRate = VK_TRUE;

	if (m_VkbPhysicalDevice.enable_extension_if_present(VK_KHR_FRAGMENT_SHADING_RATE_EXTENSION_NAME))
	{
		if (m_VkbPhysicalDevice.enable_extension_features_if_present(fragmentShadingRateFeatures))
		{
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

bool Application::CreateLogicalDevice()
{
	ZoneScopedN("CreateLogicalDevice");

	vkb::DeviceBuilder deviceBuilder(m_VkbPhysicalDevice);
	auto deviceRet = deviceBuilder.build();

	if (!deviceRet)
	{
		Logger::Error("Failed to create Vulkan Device: %s", deviceRet.error().message().c_str());
		return false;
	}

	m_VkbDevice = deviceRet.value();
	volkLoadDevice(m_VkbDevice.device);

	return true;
}

bool Application::GetQueues()
{
	ZoneScopedN("GetQueues");

	auto graphicsQueueRet = m_VkbDevice.get_queue(vkb::QueueType::graphics);
	auto presentQueueRet = m_VkbDevice.get_queue(vkb::QueueType::present);

	if (!graphicsQueueRet || !presentQueueRet)
	{
		Logger::Error("Failed to get device queues");
		return false;
	}

	m_GraphicsQueue = graphicsQueueRet.value();
	m_PresentQueue = presentQueueRet.value();
	Logger::Info("Vulkan Device and Queues ready");

	return true;
}

bool Application::CreateTracyContext()
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

// --- Cleanup Helpers ---

void Application::CleanupVulkan()
{
	ZoneScopedN("CleanupVulkan");

	if (m_VkbDevice.device != VK_NULL_HANDLE)
	{
		vkDeviceWaitIdle(m_VkbDevice.device);

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
