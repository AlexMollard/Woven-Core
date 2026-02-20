#include "pch.hpp"

// Volk must be included before VkBootstrap and Vulkan headers
#include <volk.h>

// Vulkan stuff must be included after Volk
#include <SDL3/SDL_vulkan.h>
#include <VkBootstrap.h>

#include "Application.hpp"

Application::Application()
{
}

Application::~Application()
{
}

bool Application::Init()
{
	ZoneScoped;

	if (!InitMemoryAllocator())
		return false;

	if (!InitSDL())
		return false;

	if (!InitVulkan())
		return false;

	if (!InitPhysics())
		return false;

	SDL_Log("Application initialized successfully!");
	return true;
}

void Application::Update()
{
	ZoneScoped;
	FrameMark;

	// TODO: Update physics
	// TODO: Record command buffers
	// TODO: Present
}

void Application::Shutdown()
{
	ZoneScoped;

	CleanupVulkan();

	if (m_Window)
	{
		SDL_DestroyWindow(m_Window);
		m_Window = nullptr;
	}

	SDL_Quit();

	// Print memory statistics
	mi_stats_print(nullptr);
	SDL_Log("Application shutdown complete.");
}

// --- Initialization Helpers ---

bool Application::InitMemoryAllocator()
{
	mi_version();
	SDL_Log("mimalloc initialized (tracking enabled in debug builds)");
	SDL_Log("Tracy profiler enabled (connect with Tracy Profiler GUI)");
	return true;
}

bool Application::InitSDL()
{
	if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
	{
		SDL_Log("Failed to initialize SDL: %s", SDL_GetError());
		return false;
	}

	m_Window = SDL_CreateWindow("Woven Core", 1920, 1080, SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);
	if (!m_Window)
	{
		SDL_Log("Failed to create window: %s", SDL_GetError());
		return false;
	}

	return true;
}

bool Application::InitVulkan()
{
	ZoneScopedN("InitVulkan");

	// Initialize Volk
	if (volkInitialize() != VK_SUCCESS)
	{
		SDL_Log("Failed to initialize Volk. Is Vulkan installed?");
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

	return true;
}

bool Application::InitPhysics()
{
	JPH::RegisterDefaultAllocator();
	SDL_Log("Jolt Physics initialized");
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
		SDL_Log("Failed to get Vulkan extensions from SDL");
		return false;
	}

	// Build instance
	vkb::InstanceBuilder instanceBuilder;
	auto instanceRet = instanceBuilder.set_app_name("Woven Core")
	                           .set_engine_name("Woven Engine")
	                           .require_api_version(1, 4, 325)
	                           .enable_extensions(extCount, extensions)
	                           .enable_validation_layers(true)
	                           .use_default_debug_messenger()
	                           .set_debug_callback(
	                                   [](VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT* data, void* userData) -> VkBool32
	                                   {
		                                   SDL_Log("[Vulkan] %s", data->pMessage);
		                                   return VK_FALSE;
	                                   })
	                           .build();

	if (!instanceRet)
	{
		SDL_Log("Failed to create Vulkan Instance: %s", instanceRet.error().message().c_str());
		return false;
	}

	m_VkbInstance = instanceRet.value();
	volkLoadInstance(m_VkbInstance.instance);

	// Log API version
	uint32_t apiVersion = m_VkbInstance.instance_version;
	uint32_t major = VK_VERSION_MAJOR(apiVersion);
	uint32_t minor = VK_VERSION_MINOR(apiVersion);
	uint32_t patch = VK_VERSION_PATCH(apiVersion);
	SDL_Log("Vulkan Instance created (API %u.%u.%u)", major, minor, patch);

	return true;
}

bool Application::CreateSurface()
{
	ZoneScopedN("CreateSurface");

	VkSurfaceKHR tempSurface = VK_NULL_HANDLE;
	if (!SDL_Vulkan_CreateSurface(m_Window, m_VkbInstance.instance, nullptr, &tempSurface))
	{
		SDL_Log("Failed to create Vulkan Surface: %s", SDL_GetError());
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
	auto physicalDeviceRet = selector.set_surface(m_Surface)
	                               .set_minimum_version(1, 3)
	                               .set_required_features_11(required11)
	                               .set_required_features_12(required12)
	                               .set_required_features_13(required13)
	                               .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
	                               .select();

	if (!physicalDeviceRet)
	{
		SDL_Log("Failed to select Physical Device: %s", physicalDeviceRet.error().message().c_str());
		return false;
	}

	m_VkbPhysicalDevice = physicalDeviceRet.value();
	SDL_Log("Selected GPU: %s", m_VkbPhysicalDevice.properties.deviceName);

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
			SDL_Log("Enabled VK_KHR_fragment_shading_rate");
		}
		else
		{
			SDL_Log("VK_KHR_fragment_shading_rate present but required features are unavailable");
		}
	}
	else
	{
		SDL_Log("VK_KHR_fragment_shading_rate not available (continuing without it)");
	}
#else
	SDL_Log("VK_KHR_fragment_shading_rate headers not available (continuing without it)");
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
		SDL_Log("Failed to create Vulkan Device: %s", deviceRet.error().message().c_str());
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
		SDL_Log("Failed to get device queues");
		return false;
	}

	m_GraphicsQueue = graphicsQueueRet.value();
	m_PresentQueue = presentQueueRet.value();
	SDL_Log("Vulkan Device and Queues created successfully!");

	return true;
}

// --- Cleanup Helpers ---

void Application::CleanupVulkan()
{
	ZoneScopedN("CleanupVulkan");

	if (m_VkbDevice.device != VK_NULL_HANDLE)
	{
		vkDeviceWaitIdle(m_VkbDevice.device);
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
