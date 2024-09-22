#include "ntshengn_graphics_module.h"
#include "../Module/utils/ntshengn_dynamic_library.h"
#include "../Common/modules/ntshengn_window_module_interface.h"
#include "../external/glslang/glslang/Include/ShHandle.h"
#include "../external/glslang/SPIRV/GlslangToSpv.h"
#include "../external/glslang/StandAlone/DirStackFileIncluder.h"
#define VMA_IMPLEMENTATION
#if defined(NTSHENGN_COMPILER_MSVC)
#pragma warning(push)
#pragma warning(disable : 4100)
#pragma warning(disable : 4127)
#pragma warning(disable : 4189)
#pragma warning(disable : 4324)
#pragma warning(disable : 4505)
#elif defined(NTSHENGN_COMPILER_GCC)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wunused-function"
#elif defined(NTSHENGN_COMPILER_CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wunused-function"
#endif
#include "../external/VulkanMemoryAllocator/include/vk_mem_alloc.h"
#if defined(NTSHENGN_COMPILER_MSVC)
#pragma warning(pop)
#elif defined(NTSHENGN_COMPILER_GCC)
#pragma GCC diagnostic pop
#elif defined(NTSHENGN_COMPILER_CLANG)
#pragma clang diagnostic pop
#endif
#include <limits>
#include <array>

void NtshEngn::GraphicsModule::init() {
	if (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) {
		m_framesInFlight = 2;
	}
	else {
		m_framesInFlight = 1;
	}

	// Create instance
	VkApplicationInfo applicationInfo = {};
	applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	applicationInfo.pNext = nullptr;
	applicationInfo.pApplicationName = getName().c_str();
	applicationInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
	applicationInfo.pEngineName = "NutshellEngine";
	applicationInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);
	applicationInfo.apiVersion = VK_API_VERSION_1_1;

	VkInstanceCreateInfo instanceCreateInfo = {};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pNext = nullptr;
	instanceCreateInfo.flags = 0;
	instanceCreateInfo.pApplicationInfo = &applicationInfo;
#if defined(NTSHENGN_DEBUG)
	std::array<const char*, 1> explicitLayers = { "VK_LAYER_KHRONOS_validation" };
	bool foundValidationLayer = false;
	uint32_t instanceLayerPropertyCount;
	NTSHENGN_VK_CHECK(vkEnumerateInstanceLayerProperties(&instanceLayerPropertyCount, nullptr));
	std::vector<VkLayerProperties> instanceLayerProperties(instanceLayerPropertyCount);
	NTSHENGN_VK_CHECK(vkEnumerateInstanceLayerProperties(&instanceLayerPropertyCount, instanceLayerProperties.data()));

	for (const VkLayerProperties& availableLayer : instanceLayerProperties) {
		if (strcmp(availableLayer.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
			foundValidationLayer = true;
			break;
		}
	}

	if (foundValidationLayer) {
		instanceCreateInfo.enabledLayerCount = 1;
		instanceCreateInfo.ppEnabledLayerNames = explicitLayers.data();
	}
	else {
		NTSHENGN_MODULE_WARNING("Could not find validation layer VK_LAYER_KHRONOS_validation.");
		instanceCreateInfo.enabledLayerCount = 0;
		instanceCreateInfo.ppEnabledLayerNames = nullptr;
	}
#else
	instanceCreateInfo.enabledLayerCount = 0;
	instanceCreateInfo.ppEnabledLayerNames = nullptr;
#endif
	std::vector<const char*> instanceExtensions;
#if defined(NTSHENGN_DEBUG)
	instanceExtensions.push_back("VK_EXT_debug_utils");
#endif
	if (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) {
		instanceExtensions.push_back("VK_KHR_surface");
		instanceExtensions.push_back("VK_KHR_get_surface_capabilities2");
		instanceExtensions.push_back("VK_KHR_get_physical_device_properties2");
#if defined(NTSHENGN_OS_WINDOWS)
		instanceExtensions.push_back("VK_KHR_win32_surface");
#elif defined(NTSHENGN_OS_LINUX) || defined(NTSHENGN_OS_FREEBSD)
		instanceExtensions.push_back("VK_KHR_xlib_surface");
#endif
	}
	instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
	instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
	NTSHENGN_VK_CHECK(vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance));

#if defined(NTSHENGN_DEBUG)
	// Create debug messenger
	VkDebugUtilsMessengerCreateInfoEXT debugMessengerCreateInfo = {};
	debugMessengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debugMessengerCreateInfo.pNext = nullptr;
	debugMessengerCreateInfo.flags = 0;
	debugMessengerCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	debugMessengerCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
		VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debugMessengerCreateInfo.pfnUserCallback = debugCallback;
	debugMessengerCreateInfo.pUserData = nullptr;

	auto createDebugUtilsMessengerEXT = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT");
	NTSHENGN_VK_CHECK(createDebugUtilsMessengerEXT(m_instance, &debugMessengerCreateInfo, nullptr, &m_debugMessenger));
#endif

	// Create surface
	if (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) {
#if defined(NTSHENGN_OS_WINDOWS)
		HWND windowHandle = reinterpret_cast<HWND>(windowModule->getWindowNativeHandle(windowModule->getMainWindowID()));
		VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {};
		surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		surfaceCreateInfo.pNext = nullptr;
		surfaceCreateInfo.flags = 0;
		surfaceCreateInfo.hinstance = reinterpret_cast<HINSTANCE>(windowModule->getWindowNativeAdditionalInformation(windowModule->getMainWindowID()));
		surfaceCreateInfo.hwnd = windowHandle;
		auto createWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)vkGetInstanceProcAddr(m_instance, "vkCreateWin32SurfaceKHR");
		NTSHENGN_VK_CHECK(createWin32SurfaceKHR(m_instance, &surfaceCreateInfo, nullptr, &m_surface));
#elif defined(NTSHENGN_OS_LINUX) || defined(NTSHENGN_OS_FREEBSD)
		Window windowHandle = reinterpret_cast<Window>(windowModule->getWindowNativeHandle(windowModule->getMainWindowID()));
		VkXlibSurfaceCreateInfoKHR surfaceCreateInfo = {};
		surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
		surfaceCreateInfo.pNext = nullptr;
		surfaceCreateInfo.flags = 0;
		surfaceCreateInfo.dpy = reinterpret_cast<Display*>(windowModule->getWindowNativeAdditionalInformation(windowModule->getMainWindowID()));
		surfaceCreateInfo.window = windowHandle;
		auto createXlibSurfaceKHR = (PFN_vkCreateXlibSurfaceKHR)vkGetInstanceProcAddr(m_instance, "vkCreateXlibSurfaceKHR");
		NTSHENGN_VK_CHECK(createXlibSurfaceKHR(m_instance, &surfaceCreateInfo, nullptr, &m_surface));
#endif
	}

	// Pick a physical device
	uint32_t deviceCount;
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
	if (deviceCount == 0) {
		NTSHENGN_MODULE_ERROR("Vulkan: Found no suitable GPU.", Result::ModuleError);
	}
	std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, physicalDevices.data());
	m_physicalDevice = physicalDevices[0];

	VkPhysicalDeviceRayTracingPipelinePropertiesKHR physicalDeviceRayTracingPipelineProperties = {};
	physicalDeviceRayTracingPipelineProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR;
	physicalDeviceRayTracingPipelineProperties.pNext = nullptr;

	VkPhysicalDeviceAccelerationStructurePropertiesKHR physicalDeviceAccelerationStructureProperties = {};
	physicalDeviceAccelerationStructureProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;
	physicalDeviceAccelerationStructureProperties.pNext = &physicalDeviceRayTracingPipelineProperties;

	VkPhysicalDeviceProperties2 physicalDeviceProperties2 = {};
	physicalDeviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	physicalDeviceProperties2.pNext = &physicalDeviceAccelerationStructureProperties;
	vkGetPhysicalDeviceProperties2(m_physicalDevice, &physicalDeviceProperties2);

	std::string physicalDeviceType;
	switch (physicalDeviceProperties2.properties.deviceType) {
	case VK_PHYSICAL_DEVICE_TYPE_OTHER:
		physicalDeviceType = "Other";
		break;
	case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
		physicalDeviceType = "Integrated";
		break;
	case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
		physicalDeviceType = "Discrete";
		break;
	case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
		physicalDeviceType = "Virtual";
		break;
	case VK_PHYSICAL_DEVICE_TYPE_CPU:
		physicalDeviceType = "CPU";
		break;
	default:
		physicalDeviceType = "Unknown";
	}

	std::string driverVersion = std::to_string(VK_API_VERSION_MAJOR(physicalDeviceProperties2.properties.driverVersion)) + "."
		+ std::to_string(VK_API_VERSION_MINOR(physicalDeviceProperties2.properties.driverVersion)) + "."
		+ std::to_string(VK_API_VERSION_PATCH(physicalDeviceProperties2.properties.driverVersion));
	if (physicalDeviceProperties2.properties.vendorID == 4318) { // NVIDIA
		uint32_t major = (physicalDeviceProperties2.properties.driverVersion >> 22) & 0x3ff;
		uint32_t minor = (physicalDeviceProperties2.properties.driverVersion >> 14) & 0x0ff;
		uint32_t patch = (physicalDeviceProperties2.properties.driverVersion >> 6) & 0x0ff;
		driverVersion = std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
	}
#if defined(NTSHENGN_OS_WINDOWS)
	else if (physicalDeviceProperties2.properties.vendorID == 0x8086) { // Intel
		uint32_t major = (physicalDeviceProperties2.properties.driverVersion >> 14);
		uint32_t minor = (physicalDeviceProperties2.properties.driverVersion) & 0x3fff;
		driverVersion = std::to_string(major) + "." + std::to_string(minor);
	}
#endif

	NTSHENGN_MODULE_INFO("Physical Device Name: " + std::string(physicalDeviceProperties2.properties.deviceName));
	NTSHENGN_MODULE_INFO("Physical Device Type: " + physicalDeviceType);
	NTSHENGN_MODULE_INFO("Physical Device Driver Version: " + driverVersion);
	NTSHENGN_MODULE_INFO("Physical Device Vulkan API Version: " + std::to_string(VK_API_VERSION_MAJOR(physicalDeviceProperties2.properties.apiVersion)) + "."
		+ std::to_string(VK_API_VERSION_MINOR(physicalDeviceProperties2.properties.apiVersion)) + "."
		+ std::to_string(VK_API_VERSION_PATCH(physicalDeviceProperties2.properties.apiVersion)));
	NTSHENGN_MODULE_INFO("Physical Device Raytracing capabilities:");
	NTSHENGN_MODULE_INFO("Max Ray Recursion Depth: " + std::to_string(physicalDeviceRayTracingPipelineProperties.maxRayRecursionDepth));
	NTSHENGN_MODULE_INFO("Max Ray Dispatch Invocation Count: " + std::to_string(physicalDeviceRayTracingPipelineProperties.maxRayDispatchInvocationCount));
	NTSHENGN_MODULE_INFO("Max BLAS Count In TLAS: " + std::to_string(physicalDeviceAccelerationStructureProperties.maxInstanceCount));
	NTSHENGN_MODULE_INFO("Max Geometry Count In BLAS: " + std::to_string(physicalDeviceAccelerationStructureProperties.maxGeometryCount));
	NTSHENGN_MODULE_INFO("Max Triangle Or AABB Count In BLAS: " + std::to_string(physicalDeviceAccelerationStructureProperties.maxPrimitiveCount));

	m_rayTracingPipelineShaderGroupHandleSize = physicalDeviceRayTracingPipelineProperties.shaderGroupHandleSize;
	m_rayTracingPipelineShaderGroupHandleAlignment = physicalDeviceRayTracingPipelineProperties.shaderGroupHandleAlignment;
	m_rayTracingPipelineShaderGroupBaseAlignment = physicalDeviceRayTracingPipelineProperties.shaderGroupBaseAlignment;

	// Find a queue family supporting graphics
	uint32_t queueFamilyPropertyCount;
	vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyPropertyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyPropertyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyPropertyCount, queueFamilyProperties.data());
	m_graphicsQueueFamilyIndex = 0;
	for (const VkQueueFamilyProperties& queueFamilyProperty : queueFamilyProperties) {
		if (queueFamilyProperty.queueCount > 0 && (queueFamilyProperty.queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
			if (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) {
				VkBool32 presentSupport;
				vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, m_graphicsQueueFamilyIndex, m_surface, &presentSupport);
				if (presentSupport) {
					break;
				}
			}
			else {
				break;
			}
		}
		m_graphicsQueueFamilyIndex++;
	}

	// Create a queue supporting graphics
	float queuePriority = 1.0f;
	VkDeviceQueueCreateInfo deviceQueueCreateInfo = {};
	deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	deviceQueueCreateInfo.pNext = nullptr;
	deviceQueueCreateInfo.flags = 0;
	deviceQueueCreateInfo.queueFamilyIndex = m_graphicsQueueFamilyIndex;
	deviceQueueCreateInfo.queueCount = 1;
	deviceQueueCreateInfo.pQueuePriorities = &queuePriority;

	// Enable features
	VkPhysicalDeviceRayTracingPipelineFeaturesKHR physicalDeviceRayTracingPipelineFeatures = {};
	physicalDeviceRayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
	physicalDeviceRayTracingPipelineFeatures.pNext = nullptr;
	physicalDeviceRayTracingPipelineFeatures.rayTracingPipeline = VK_TRUE;

	VkPhysicalDeviceAccelerationStructureFeaturesKHR physicalDeviceAccelerationStructureFeatures = {};
	physicalDeviceAccelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
	physicalDeviceAccelerationStructureFeatures.pNext = &physicalDeviceRayTracingPipelineFeatures;
	physicalDeviceAccelerationStructureFeatures.accelerationStructure = VK_TRUE;

	VkPhysicalDeviceBufferDeviceAddressFeaturesKHR physicalDeviceBufferDeviceAddressFeatures = {};
	physicalDeviceBufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR;
	physicalDeviceBufferDeviceAddressFeatures.pNext = &physicalDeviceAccelerationStructureFeatures;
	physicalDeviceBufferDeviceAddressFeatures.bufferDeviceAddress = VK_TRUE;

	VkPhysicalDeviceScalarBlockLayoutFeaturesEXT physicalDeviceScalarBlockLayoutFeaturesEXT = {};
	physicalDeviceScalarBlockLayoutFeaturesEXT.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES_EXT;
	physicalDeviceScalarBlockLayoutFeaturesEXT.pNext = &physicalDeviceBufferDeviceAddressFeatures;
	physicalDeviceScalarBlockLayoutFeaturesEXT.scalarBlockLayout = VK_TRUE;

	VkPhysicalDeviceDescriptorIndexingFeatures physicalDeviceDescriptorIndexingFeatures = {};
	physicalDeviceDescriptorIndexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;
	physicalDeviceDescriptorIndexingFeatures.pNext = &physicalDeviceScalarBlockLayoutFeaturesEXT;
	physicalDeviceDescriptorIndexingFeatures.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
	physicalDeviceDescriptorIndexingFeatures.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
	physicalDeviceDescriptorIndexingFeatures.descriptorBindingPartiallyBound = VK_TRUE;
	physicalDeviceDescriptorIndexingFeatures.runtimeDescriptorArray = VK_TRUE;

	VkPhysicalDeviceDynamicRenderingFeatures physicalDeviceDynamicRenderingFeatures = {};
	physicalDeviceDynamicRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
	physicalDeviceDynamicRenderingFeatures.pNext = &physicalDeviceDescriptorIndexingFeatures;
	physicalDeviceDynamicRenderingFeatures.dynamicRendering = VK_TRUE;

	VkPhysicalDeviceSynchronization2Features physicalDeviceSynchronization2Features = {};
	physicalDeviceSynchronization2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
	physicalDeviceSynchronization2Features.pNext = &physicalDeviceDynamicRenderingFeatures;
	physicalDeviceSynchronization2Features.synchronization2 = VK_TRUE;

	VkPhysicalDeviceFeatures physicalDeviceFeatures = {};
	physicalDeviceFeatures.samplerAnisotropy = VK_TRUE;
	physicalDeviceFeatures.shaderInt64 = VK_TRUE;
	
	// Create the logical device
	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pNext = &physicalDeviceSynchronization2Features;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pQueueCreateInfos = &deviceQueueCreateInfo;
	deviceCreateInfo.enabledLayerCount = 0;
	deviceCreateInfo.ppEnabledLayerNames = nullptr;
	std::vector<const char*> deviceExtensions = { "VK_KHR_synchronization2",
		"VK_KHR_create_renderpass2",
		"VK_KHR_depth_stencil_resolve",
		"VK_KHR_dynamic_rendering",
		"VK_KHR_maintenance3",
		"VK_KHR_shader_float_controls",
		"VK_KHR_spirv_1_4",
		"VK_EXT_descriptor_indexing",
		"VK_EXT_scalar_block_layout",
		"VK_KHR_ray_tracing_pipeline",
		"VK_KHR_buffer_device_address",
		"VK_KHR_deferred_host_operations",
		"VK_KHR_acceleration_structure" };
	if (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) {
		deviceExtensions.push_back("VK_KHR_swapchain");
	}
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
	deviceCreateInfo.pEnabledFeatures = &physicalDeviceFeatures;
	NTSHENGN_VK_CHECK(vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_device));

	vkGetDeviceQueue(m_device, m_graphicsQueueFamilyIndex, 0, &m_graphicsQueue);

	// Get functions
	m_vkCmdPipelineBarrier2KHR = (PFN_vkCmdPipelineBarrier2KHR)vkGetDeviceProcAddr(m_device, "vkCmdPipelineBarrier2KHR");
	m_vkCmdBeginRenderingKHR = (PFN_vkCmdBeginRenderingKHR)vkGetDeviceProcAddr(m_device, "vkCmdBeginRenderingKHR");
	m_vkCmdEndRenderingKHR = (PFN_vkCmdEndRenderingKHR)vkGetDeviceProcAddr(m_device, "vkCmdEndRenderingKHR");
	m_vkGetBufferDeviceAddressKHR = (PFN_vkGetBufferDeviceAddressKHR)vkGetDeviceProcAddr(m_device, "vkGetBufferDeviceAddressKHR");
	m_vkGetAccelerationStructureBuildSizesKHR = (PFN_vkGetAccelerationStructureBuildSizesKHR)vkGetDeviceProcAddr(m_device, "vkGetAccelerationStructureBuildSizesKHR");
	m_vkCreateAccelerationStructureKHR = (PFN_vkCreateAccelerationStructureKHR)vkGetDeviceProcAddr(m_device, "vkCreateAccelerationStructureKHR");
	m_vkDestroyAccelerationStructureKHR = (PFN_vkDestroyAccelerationStructureKHR)vkGetDeviceProcAddr(m_device, "vkDestroyAccelerationStructureKHR");
	m_vkCmdBuildAccelerationStructuresKHR = (PFN_vkCmdBuildAccelerationStructuresKHR)vkGetDeviceProcAddr(m_device, "vkCmdBuildAccelerationStructuresKHR");
	m_vkGetAccelerationStructureDeviceAddressKHR = (PFN_vkGetAccelerationStructureDeviceAddressKHR)vkGetDeviceProcAddr(m_device, "vkGetAccelerationStructureDeviceAddressKHR");
	m_vkCreateRayTracingPipelinesKHR = (PFN_vkCreateRayTracingPipelinesKHR)vkGetDeviceProcAddr(m_device, "vkCreateRayTracingPipelinesKHR");
	m_vkGetRayTracingShaderGroupHandlesKHR = (PFN_vkGetRayTracingShaderGroupHandlesKHR)vkGetDeviceProcAddr(m_device, "vkGetRayTracingShaderGroupHandlesKHR");
	m_vkCmdTraceRaysKHR = (PFN_vkCmdTraceRaysKHR)vkGetDeviceProcAddr(m_device, "vkCmdTraceRaysKHR");

	// Initialize VMA
	VmaAllocatorCreateInfo vmaAllocatorCreateInfo = {};
	vmaAllocatorCreateInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	vmaAllocatorCreateInfo.physicalDevice = m_physicalDevice;
	vmaAllocatorCreateInfo.device = m_device;
	vmaAllocatorCreateInfo.preferredLargeHeapBlockSize = 0;
	vmaAllocatorCreateInfo.pAllocationCallbacks = nullptr;
	vmaAllocatorCreateInfo.pDeviceMemoryCallbacks = nullptr;
	vmaAllocatorCreateInfo.pHeapSizeLimit = nullptr;
	vmaAllocatorCreateInfo.pVulkanFunctions = nullptr;
	vmaAllocatorCreateInfo.instance = m_instance;
	vmaAllocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_1;
	NTSHENGN_VK_CHECK(vmaCreateAllocator(&vmaAllocatorCreateInfo, &m_allocator));

	// Create the swapchain
	if (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) {
		createSwapchain(VK_NULL_HANDLE);
	}
	// Or create an image to draw on
	else {
		m_drawImageFormat = VK_FORMAT_R8G8B8A8_SRGB;
		m_imageCount = 1;

		m_viewport.x = 0.0f;
		m_viewport.y = 0.0f;
		m_viewport.width = 1280.0f;
		m_viewport.height = 720.0f;
		m_viewport.minDepth = 0.0f;
		m_viewport.maxDepth = 1.0f;

		m_scissor.offset.x = 0;
		m_scissor.offset.y = 0;
		m_scissor.extent.width = 1280;
		m_scissor.extent.height = 720;

		VkImageCreateInfo imageCreateInfo = {};
		imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageCreateInfo.pNext = nullptr;
		imageCreateInfo.flags = 0;
		imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
		imageCreateInfo.format = m_drawImageFormat;
		imageCreateInfo.extent.width = 1280;
		imageCreateInfo.extent.height = 720;
		imageCreateInfo.extent.depth = 1;
		imageCreateInfo.mipLevels = 1;
		imageCreateInfo.arrayLayers = 1;
		imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageCreateInfo.queueFamilyIndexCount = 0;
		imageCreateInfo.pQueueFamilyIndices = nullptr;
		imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VmaAllocationCreateInfo imageAllocationCreateInfo = {};
		imageAllocationCreateInfo.flags = 0;
		imageAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

		NTSHENGN_VK_CHECK(vmaCreateImage(m_allocator, &imageCreateInfo, &imageAllocationCreateInfo, &m_drawImage, &m_drawImageAllocation, nullptr));

		// Create the image view
		VkImageViewCreateInfo imageViewCreateInfo = {};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.pNext = nullptr;
		imageViewCreateInfo.flags = 0;
		imageViewCreateInfo.image = m_drawImage;
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = imageCreateInfo.format;
		imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
		imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
		imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
		imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
		imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		imageViewCreateInfo.subresourceRange.levelCount = 1;
		imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		imageViewCreateInfo.subresourceRange.layerCount = 1;
		NTSHENGN_VK_CHECK(vkCreateImageView(m_device, &imageViewCreateInfo, nullptr, &m_drawImageView));
	}

	// Create initialization command pool
	VkCommandPoolCreateInfo initializationCommandPoolCreateInfo = {};
	initializationCommandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	initializationCommandPoolCreateInfo.pNext = nullptr;
	initializationCommandPoolCreateInfo.flags = 0;
	initializationCommandPoolCreateInfo.queueFamilyIndex = m_graphicsQueueFamilyIndex;
	NTSHENGN_VK_CHECK(vkCreateCommandPool(m_device, &initializationCommandPoolCreateInfo, nullptr, &m_initializationCommandPool));

	// Allocate initialization command pool
	VkCommandBufferAllocateInfo initializationCommandBufferAllocateInfo = {};
	initializationCommandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	initializationCommandBufferAllocateInfo.pNext = nullptr;
	initializationCommandBufferAllocateInfo.commandPool = m_initializationCommandPool;
	initializationCommandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	initializationCommandBufferAllocateInfo.commandBufferCount = 1;
	NTSHENGN_VK_CHECK(vkAllocateCommandBuffers(m_device, &initializationCommandBufferAllocateInfo, &m_initializationCommandBuffer));

	// Create initialization fence
	VkFenceCreateInfo initializationFenceCreateInfo = {};
	initializationFenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	initializationFenceCreateInfo.pNext = nullptr;
	initializationFenceCreateInfo.flags = 0;
	NTSHENGN_VK_CHECK(vkCreateFence(m_device, &initializationFenceCreateInfo, nullptr, &m_initializationFence));

	createVertexIndexAndAccelerationStructureBuffers();

	createTopLevelAccelerationStructure();

	createColorImage();

	createDescriptorSetLayout();

	createRayTracingPipeline();

	createRayTracingShaderBindingTable();

	createToneMappingResources();

	createUIResources();

	// Create camera uniform buffer
	m_cameraBuffers.resize(m_framesInFlight);
	VkBufferCreateInfo cameraBufferCreateInfo = {};
	cameraBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	cameraBufferCreateInfo.pNext = nullptr;
	cameraBufferCreateInfo.flags = 0;
	cameraBufferCreateInfo.size = sizeof(Math::mat4) * 2 + sizeof(Math::vec4);
	cameraBufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	cameraBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	cameraBufferCreateInfo.queueFamilyIndexCount = 1;
	cameraBufferCreateInfo.pQueueFamilyIndices = &m_graphicsQueueFamilyIndex;

	VmaAllocationInfo bufferAllocationInfo;

	VmaAllocationCreateInfo bufferAllocationCreateInfo = {};
	bufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
	bufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &cameraBufferCreateInfo, &bufferAllocationCreateInfo, &m_cameraBuffers[i].handle, &m_cameraBuffers[i].allocation, &bufferAllocationInfo));
		m_cameraBuffers[i].address = bufferAllocationInfo.pMappedData;
	}

	// Create object storage buffers
	m_objectBuffers.resize(m_framesInFlight);
	VkBufferCreateInfo objectBufferCreateInfo = {};
	objectBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	objectBufferCreateInfo.pNext = nullptr;
	objectBufferCreateInfo.flags = 0;
	objectBufferCreateInfo.size = 32768;
	objectBufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	objectBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	objectBufferCreateInfo.queueFamilyIndexCount = 1;
	objectBufferCreateInfo.pQueueFamilyIndices = &m_graphicsQueueFamilyIndex;

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &objectBufferCreateInfo, &bufferAllocationCreateInfo, &m_objectBuffers[i].handle, &m_objectBuffers[i].allocation, &bufferAllocationInfo));
		m_objectBuffers[i].address = bufferAllocationInfo.pMappedData;
	}

	// Create mesh storage buffers
	m_meshBuffers.resize(m_framesInFlight);
	VkBufferCreateInfo meshBufferCreateInfo = {};
	meshBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	meshBufferCreateInfo.pNext = nullptr;
	meshBufferCreateInfo.flags = 0;
	meshBufferCreateInfo.size = 32768;
	meshBufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	meshBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	meshBufferCreateInfo.queueFamilyIndexCount = 1;
	meshBufferCreateInfo.pQueueFamilyIndices = &m_graphicsQueueFamilyIndex;

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &meshBufferCreateInfo, &bufferAllocationCreateInfo, &m_meshBuffers[i].handle, &m_meshBuffers[i].allocation, &bufferAllocationInfo));
		m_meshBuffers[i].address = bufferAllocationInfo.pMappedData;
	}

	// Create material storage buffers
	m_materialBuffers.resize(m_framesInFlight);
	VkBufferCreateInfo materialBufferCreateInfo = {};
	materialBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	materialBufferCreateInfo.pNext = nullptr;
	materialBufferCreateInfo.flags = 0;
	materialBufferCreateInfo.size = 32768;
	materialBufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	materialBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	materialBufferCreateInfo.queueFamilyIndexCount = 1;
	materialBufferCreateInfo.pQueueFamilyIndices = &m_graphicsQueueFamilyIndex;

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &materialBufferCreateInfo, &bufferAllocationCreateInfo, &m_materialBuffers[i].handle, &m_materialBuffers[i].allocation, &bufferAllocationInfo));
		m_materialBuffers[i].address = bufferAllocationInfo.pMappedData;
	}

	// Create light storage buffer
	m_lightBuffers.resize(m_framesInFlight);
	VkBufferCreateInfo lightBufferCreateInfo = {};
	lightBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	lightBufferCreateInfo.pNext = nullptr;
	lightBufferCreateInfo.flags = 0;
	lightBufferCreateInfo.size = 32768;
	lightBufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	lightBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	lightBufferCreateInfo.queueFamilyIndexCount = 1;
	lightBufferCreateInfo.pQueueFamilyIndices = &m_graphicsQueueFamilyIndex;

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &lightBufferCreateInfo, &bufferAllocationCreateInfo, &m_lightBuffers[i].handle, &m_lightBuffers[i].allocation, &bufferAllocationInfo));
		m_lightBuffers[i].address = bufferAllocationInfo.pMappedData;
	}

	createDescriptorSets();

	createDefaultResources();

	// Resize buffers according to number of frames in flight and swapchain size
	m_renderingCommandPools.resize(m_framesInFlight);
	m_renderingCommandBuffers.resize(m_framesInFlight);

	m_fences.resize(m_framesInFlight);
	m_imageAvailableSemaphores.resize(m_framesInFlight);
	m_renderFinishedSemaphores.resize(m_imageCount);

	VkCommandPoolCreateInfo commandPoolCreateInfo = {};
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.pNext = nullptr;
	commandPoolCreateInfo.flags = 0;
	commandPoolCreateInfo.queueFamilyIndex = m_graphicsQueueFamilyIndex;

	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.pNext = nullptr;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = 1;

	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.pNext = nullptr;
	fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreCreateInfo.pNext = nullptr;
	semaphoreCreateInfo.flags = 0;
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		// Create rendering command pools and buffers
		NTSHENGN_VK_CHECK(vkCreateCommandPool(m_device, &commandPoolCreateInfo, nullptr, &m_renderingCommandPools[i]));

		commandBufferAllocateInfo.commandPool = m_renderingCommandPools[i];
		NTSHENGN_VK_CHECK(vkAllocateCommandBuffers(m_device, &commandBufferAllocateInfo, &m_renderingCommandBuffers[i]));

		// Create sync objects
		NTSHENGN_VK_CHECK(vkCreateFence(m_device, &fenceCreateInfo, nullptr, &m_fences[i]));

		NTSHENGN_VK_CHECK(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_imageAvailableSemaphores[i]));
	}
	for (uint32_t i = 0; i < m_imageCount; i++) {
		NTSHENGN_VK_CHECK(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_renderFinishedSemaphores[i]));
	}

	// Set current frame-in-flight to 0
	m_currentFrameInFlight = 0;
}

void NtshEngn::GraphicsModule::update(double dt) {
	NTSHENGN_UNUSED(dt);

	if (windowModule && (!windowModule->isWindowOpen(windowModule->getMainWindowID()) || ((windowModule->getWindowWidth(windowModule->getMainWindowID()) == 0) || (windowModule->getWindowHeight(windowModule->getMainWindowID()) == 0)))) {
		// Do not update if the main window got closed or the window size is 0
		return;
	}

	NTSHENGN_VK_CHECK(vkWaitForFences(m_device, 1, &m_fences[m_currentFrameInFlight], VK_TRUE, std::numeric_limits<uint64_t>::max()));

	uint32_t imageIndex = m_imageCount - 1;
	if (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) {
		VkResult acquireNextImageResult = vkAcquireNextImageKHR(m_device, m_swapchain, std::numeric_limits<uint64_t>::max(), m_imageAvailableSemaphores[m_currentFrameInFlight], VK_NULL_HANDLE, &imageIndex);
		if (acquireNextImageResult == VK_ERROR_OUT_OF_DATE_KHR) {
			resize();
		}
		else if (acquireNextImageResult != VK_SUCCESS && acquireNextImageResult != VK_SUBOPTIMAL_KHR) {
			NTSHENGN_MODULE_ERROR("Next swapchain image acquire failed.", Result::ModuleError);
		}
	}
	else {
		VkSubmitInfo emptySignalSubmitInfo = {};
		emptySignalSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		emptySignalSubmitInfo.pNext = nullptr;
		emptySignalSubmitInfo.waitSemaphoreCount = 0;
		emptySignalSubmitInfo.pWaitSemaphores = nullptr;
		emptySignalSubmitInfo.pWaitDstStageMask = nullptr;
		emptySignalSubmitInfo.commandBufferCount = 0;
		emptySignalSubmitInfo.pCommandBuffers = nullptr;
		emptySignalSubmitInfo.signalSemaphoreCount = 1;
		emptySignalSubmitInfo.pSignalSemaphores = &m_imageAvailableSemaphores[m_currentFrameInFlight];
		NTSHENGN_VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &emptySignalSubmitInfo, VK_NULL_HANDLE));
	}

	// Update camera buffer
	if (m_mainCamera != NTSHENGN_ENTITY_UNKNOWN) {
		const Camera& camera = ecs->getComponent<Camera>(m_mainCamera);
		const Transform& cameraTransform = ecs->getComponent<Transform>(m_mainCamera);

		const Math::mat4 cameraRotation = Math::rotate(cameraTransform.rotation.x, Math::vec3(1.0f, 0.0f, 0.0f)) *
			Math::rotate(cameraTransform.rotation.y, Math::vec3(0.0f, 1.0f, 0.0f)) *
			Math::rotate(cameraTransform.rotation.z, Math::vec3(0.0f, 0.0f, 1.0f));
		Math::mat4 cameraView = cameraRotation * Math::lookAtRH(cameraTransform.position, cameraTransform.position + camera.forward, camera.up);
		Math::mat4 cameraProjection = Math::perspectiveRH(camera.fov, m_viewport.width / m_viewport.height, camera.nearPlane, camera.farPlane);
		cameraProjection[1][1] *= -1.0f;
		std::array<Math::mat4, 2> cameraMatrices{ cameraView, cameraProjection };
		Math::vec4 cameraPositionAsVec4 = { cameraTransform.rotation, 0.0f };

		memcpy(m_cameraBuffers[m_currentFrameInFlight].address, cameraMatrices.data(), sizeof(Math::mat4) * 2);
		memcpy(reinterpret_cast<char*>(m_cameraBuffers[m_currentFrameInFlight].address) + sizeof(Math::mat4) * 2, cameraPositionAsVec4.data(), sizeof(Math::vec4));

		if (m_sampleBatch != 0) {
			if ((cameraTransform.position != m_previousCamera.transform.position) ||
				(cameraTransform.rotation != m_previousCamera.transform.rotation) ||
				(camera.forward != m_previousCamera.camera.forward) ||
				(camera.up != m_previousCamera.camera.up) ||
				(camera.fov != m_previousCamera.camera.fov) ||
				(camera.nearPlane != m_previousCamera.camera.nearPlane) ||
				(camera.farPlane != m_previousCamera.camera.farPlane)) {
				m_sampleBatch = 0;

				m_previousCamera.transform = cameraTransform;
				m_previousCamera.camera = camera;
			}
		}
	}

	// Update objects buffer
	for (auto& it : m_objects) {
		if (loadRenderableForEntity(it.first)) {
			m_sampleBatch = 0;
		}

		const uint32_t meshID = (it.second.meshID < m_meshes.size()) ? it.second.meshID : 0;
		const uint32_t materialID = (it.second.materialIndex < m_materials.size()) ? it.second.materialIndex : 0;
		std::array<uint32_t, 2> meshAndTextureID = { meshID, materialID };

		if (meshID == 0) {
			continue;
		}

		size_t offset = (it.second.index * sizeof(Math::vec2));

		memcpy(reinterpret_cast<char*>(m_objectBuffers[m_currentFrameInFlight].address) + offset, meshAndTextureID.data(), 2 * sizeof(uint32_t));

		PreviousObject& previousObject = m_previousObjects[it.first];
		const Transform& objectTransform = ecs->getComponent<Transform>(it.first);
		if (m_sampleBatch != 0) {
			if ((objectTransform.position != previousObject.transform.position) ||
				(objectTransform.rotation != previousObject.transform.rotation) ||
				(objectTransform.scale != previousObject.transform.scale) ||
				(meshID != previousObject.meshID) ||
				(materialID != previousObject.materialIndex)) {
				m_sampleBatch = 0;

				previousObject.transform = objectTransform;
				previousObject.meshID = meshID;
				previousObject.materialIndex = materialID;
			}
		}
	}

	// Update meshes buffer
	for (size_t i = 0; i < m_meshes.size(); i++) {
		size_t offset = i * 2 * sizeof(VkDeviceAddress);

		memcpy(reinterpret_cast<char*>(m_meshBuffers[m_currentFrameInFlight].address) + offset, &m_meshes[i].vertexDeviceAddress, 2 * sizeof(VkDeviceAddress));
	}

	// Update materials buffer
	for (size_t i = 0; i < m_materials.size(); i++) {
		size_t offset = i * sizeof(InternalMaterial);

		memcpy(reinterpret_cast<char*>(m_materialBuffers[m_currentFrameInFlight].address) + offset, &m_materials[i], sizeof(InternalMaterial));
	}

	// Update lights buffer
	std::array<uint32_t, 4> lightsCount = { static_cast<uint32_t>(m_lights.directionalLights.size()), static_cast<uint32_t>(m_lights.pointLights.size()), static_cast<uint32_t>(m_lights.spotLights.size()), static_cast<uint32_t>(m_lights.ambientLights.size()) };
	memcpy(m_lightBuffers[m_currentFrameInFlight].address, lightsCount.data(), 4 * sizeof(uint32_t));

	size_t offset = sizeof(Math::vec4);
	for (Entity light : m_lights.directionalLights) {
		const Light& lightLight = ecs->getComponent<Light>(light);
		const Transform& lightTransform = ecs->getComponent<Transform>(light);

		const Math::vec3 baseLightDirection = Math::normalize(lightLight.direction);
		const float baseDirectionYaw = std::atan2(baseLightDirection.z, baseLightDirection.x);
		const float baseDirectionPitch = -std::asin(baseLightDirection.y);
		const Math::vec3 lightDirection = Math::normalize(Math::vec3(
			std::cos(baseDirectionPitch + lightTransform.rotation.x) * std::cos(baseDirectionYaw + lightTransform.rotation.y),
			-std::sin(baseDirectionPitch + lightTransform.rotation.x),
			std::cos(baseDirectionPitch + lightTransform.rotation.x) * std::sin(baseDirectionYaw + lightTransform.rotation.y)
		));

		InternalLight internalLight;
		internalLight.direction = Math::vec4(lightDirection, 0.0f);
		internalLight.color = Math::vec4(lightLight.color, lightLight.intensity);

		memcpy(reinterpret_cast<char*>(m_lightBuffers[m_currentFrameInFlight].address) + offset, &internalLight, sizeof(InternalLight));
		offset += sizeof(InternalLight);

		PreviousLight& previousLight = m_previousDirectionalLights[light];
		if (m_sampleBatch != 0) {
			if ((lightTransform.rotation != previousLight.transform.rotation) ||
				(lightLight.color != previousLight.light.color) ||
				(lightLight.direction != previousLight.light.direction)) {
				m_sampleBatch = 0;

				previousLight.transform = lightTransform;
				previousLight.light = lightLight;
			}
		}
	}
	for (Entity light : m_lights.pointLights) {
		const Light& lightLight = ecs->getComponent<Light>(light);
		const Transform& lightTransform = ecs->getComponent<Transform>(light);

		InternalLight internalLight;
		internalLight.position = Math::vec4(lightTransform.position, 0.0f);
		internalLight.color = Math::vec4(lightLight.color, lightLight.intensity);

		memcpy(reinterpret_cast<char*>(m_lightBuffers[m_currentFrameInFlight].address) + offset, &internalLight, sizeof(InternalLight));
		offset += sizeof(InternalLight);

		PreviousLight& previousLight = m_previousPointLights[light];
		if (m_sampleBatch != 0) {
			if ((lightTransform.position != previousLight.transform.position) ||
				(lightLight.color != previousLight.light.color)) {
				m_sampleBatch = 0;

				previousLight.transform = lightTransform;
				previousLight.light = lightLight;
			}
		}
	}
	for (Entity light : m_lights.spotLights) {
		const Light& lightLight = ecs->getComponent<Light>(light);
		const Transform& lightTransform = ecs->getComponent<Transform>(light);

		const Math::vec3 baseLightDirection = Math::normalize(lightLight.direction);
		const float baseDirectionYaw = std::atan2(baseLightDirection.z, baseLightDirection.x);
		const float baseDirectionPitch = -std::asin(baseLightDirection.y);
		const Math::vec3 lightDirection = Math::normalize(Math::vec3(
			std::cos(baseDirectionPitch + lightTransform.rotation.x) * std::cos(baseDirectionYaw + lightTransform.rotation.y),
			-std::sin(baseDirectionPitch + lightTransform.rotation.x),
			std::cos(baseDirectionPitch + lightTransform.rotation.x) * std::sin(baseDirectionYaw + lightTransform.rotation.y)
		));

		InternalLight internalLight;
		internalLight.position = Math::vec4(lightTransform.position, 0.0f);
		internalLight.direction = Math::vec4(lightDirection, 0.0f);
		internalLight.color = Math::vec4(lightLight.color, lightLight.intensity);
		internalLight.cutoff = Math::vec4(lightLight.cutoff, 0.0f, 0.0f);

		memcpy(reinterpret_cast<char*>(m_lightBuffers[m_currentFrameInFlight].address) + offset, &internalLight, sizeof(InternalLight));
		offset += sizeof(InternalLight);

		PreviousLight& previousLight = m_previousSpotLights[light];
		if (m_sampleBatch != 0) {
			if ((lightTransform.position != previousLight.transform.position) ||
				(lightTransform.rotation != previousLight.transform.rotation) ||
				(lightLight.color != previousLight.light.color) ||
				(lightLight.direction != previousLight.light.direction) ||
				(lightLight.cutoff != previousLight.light.cutoff)) {
				m_sampleBatch = 0;

				previousLight.transform = lightTransform;
				previousLight.light = lightLight;
			}
		}
	}
	for (Entity light : m_lights.ambientLights) {
		const Light& lightLight = ecs->getComponent<Light>(light);

		InternalLight internalLight;
		internalLight.color = Math::vec4(lightLight.color, lightLight.intensity);

		memcpy(reinterpret_cast<char*>(m_lightBuffers[m_currentFrameInFlight].address) + offset, &internalLight, sizeof(InternalLight));
		offset += sizeof(InternalLight);
	}

	// Update TLAS
	std::vector<VkAccelerationStructureInstanceKHR> tlasInstances;
	for (auto& it : m_objects) {
		if (it.second.meshID == 0) {
			continue;
		}

		const Transform& objectTransform = ecs->getComponent<Transform>(it.first);

		Math::mat4 objectModel = Math::transpose(Math::translate(objectTransform.position) *
			Math::rotate(objectTransform.rotation.x, Math::vec3(1.0f, 0.0f, 0.0f)) *
			Math::rotate(objectTransform.rotation.y, Math::vec3(0.0f, 1.0f, 0.0f)) *
			Math::rotate(objectTransform.rotation.z, Math::vec3(0.0f, 0.0f, 1.0f)) *
			Math::scale(objectTransform.scale));

		VkTransformMatrixKHR objectTransformMatrix = { { { objectModel.x.x, objectModel.x.y, objectModel.x.z, objectModel.x.w },
			{ objectModel.y.x, objectModel.y.y, objectModel.y.z, objectModel.y.w },
			{ objectModel.z.x, objectModel.z.y, objectModel.z.z, objectModel.z.w }
		} };

		VkAccelerationStructureInstanceKHR accelerationStructureInstance = {};
		accelerationStructureInstance.transform = objectTransformMatrix;
		accelerationStructureInstance.instanceCustomIndex = it.second.index;
		accelerationStructureInstance.mask = 0xFF;
		accelerationStructureInstance.instanceShaderBindingTableRecordOffset = 0;
		accelerationStructureInstance.flags = VK_GEOMETRY_INSTANCE_FORCE_OPAQUE_BIT_KHR;
		accelerationStructureInstance.accelerationStructureReference = m_meshes[it.second.meshID].blasDeviceAddress;
		tlasInstances.push_back(accelerationStructureInstance);
	}
	memcpy(m_topLevelAccelerationStructureInstancesStagingBuffers[m_currentFrameInFlight].address, tlasInstances.data(), tlasInstances.size() * sizeof(VkAccelerationStructureInstanceKHR));

	// Update descriptor sets if needed
	if (m_descriptorSetsNeedUpdate[m_currentFrameInFlight]) {
		updateDescriptorSet(m_currentFrameInFlight);

		m_descriptorSetsNeedUpdate[m_currentFrameInFlight] = false;
	}
	if (m_uiTextDescriptorSetsNeedUpdate[m_currentFrameInFlight]) {
		updateUITextDescriptorSet(m_currentFrameInFlight);

		m_uiTextDescriptorSetsNeedUpdate[m_currentFrameInFlight] = false;
	}
	if (m_uiImageDescriptorSetsNeedUpdate[m_currentFrameInFlight]) {
		updateUIImageDescriptorSet(m_currentFrameInFlight);

		m_uiImageDescriptorSetsNeedUpdate[m_currentFrameInFlight] = false;
	}

	// Record rendering commands
	NTSHENGN_VK_CHECK(vkResetCommandPool(m_device, m_renderingCommandPools[m_currentFrameInFlight], 0));

	// Begin command buffer recording
	VkCommandBufferBeginInfo commandBufferBeginInfo = {};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.pNext = nullptr;
	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	NTSHENGN_VK_CHECK(vkBeginCommandBuffer(m_renderingCommandBuffers[m_currentFrameInFlight], &commandBufferBeginInfo));

	// Copy TLAS instances
	if (tlasInstances.size() != 0) {
		VkBufferMemoryBarrier2 beforeTlasInstancesCopyBufferMemoryBarrier = {};
		beforeTlasInstancesCopyBufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
		beforeTlasInstancesCopyBufferMemoryBarrier.pNext = nullptr;
		beforeTlasInstancesCopyBufferMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
		beforeTlasInstancesCopyBufferMemoryBarrier.srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;
		beforeTlasInstancesCopyBufferMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
		beforeTlasInstancesCopyBufferMemoryBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		beforeTlasInstancesCopyBufferMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
		beforeTlasInstancesCopyBufferMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
		beforeTlasInstancesCopyBufferMemoryBarrier.buffer = m_topLevelAccelerationStructureInstancesBuffer;
		beforeTlasInstancesCopyBufferMemoryBarrier.offset = 0;
		beforeTlasInstancesCopyBufferMemoryBarrier.size = tlasInstances.size() * sizeof(VkAccelerationStructureInstanceKHR);

		VkDependencyInfo beforeTlasInstancesCopyBufferDependencyInfo = {};
		beforeTlasInstancesCopyBufferDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		beforeTlasInstancesCopyBufferDependencyInfo.pNext = nullptr;
		beforeTlasInstancesCopyBufferDependencyInfo.dependencyFlags = 0;
		beforeTlasInstancesCopyBufferDependencyInfo.memoryBarrierCount = 0;
		beforeTlasInstancesCopyBufferDependencyInfo.pMemoryBarriers = nullptr;
		beforeTlasInstancesCopyBufferDependencyInfo.bufferMemoryBarrierCount = 1;
		beforeTlasInstancesCopyBufferDependencyInfo.pBufferMemoryBarriers = &beforeTlasInstancesCopyBufferMemoryBarrier;
		beforeTlasInstancesCopyBufferDependencyInfo.imageMemoryBarrierCount = 0;
		beforeTlasInstancesCopyBufferDependencyInfo.pImageMemoryBarriers = nullptr;
		m_vkCmdPipelineBarrier2KHR(m_renderingCommandBuffers[m_currentFrameInFlight], &beforeTlasInstancesCopyBufferDependencyInfo);

		VkBufferCopy tlasInstancesCopy = {};
		tlasInstancesCopy.srcOffset = 0;
		tlasInstancesCopy.dstOffset = 0;
		tlasInstancesCopy.size = tlasInstances.size() * sizeof(VkAccelerationStructureInstanceKHR);
		vkCmdCopyBuffer(m_renderingCommandBuffers[m_currentFrameInFlight], m_topLevelAccelerationStructureInstancesStagingBuffers[m_currentFrameInFlight].handle, m_topLevelAccelerationStructureInstancesBuffer, 1, &tlasInstancesCopy);
	}

	// Layout transition VK_IMAGE_LAYOUT_UNDEFINED -> VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_UNDEFINED -> VK_IMAGE_LAYOUT_GENERAL and TLAS instances copy sync
	VkImageMemoryBarrier2 undefinedToColorAttachmentImageMemoryBarrier = {};
	undefinedToColorAttachmentImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	undefinedToColorAttachmentImageMemoryBarrier.pNext = nullptr;
	undefinedToColorAttachmentImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	undefinedToColorAttachmentImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_NONE;
	undefinedToColorAttachmentImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	undefinedToColorAttachmentImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	undefinedToColorAttachmentImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	undefinedToColorAttachmentImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	undefinedToColorAttachmentImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	undefinedToColorAttachmentImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	undefinedToColorAttachmentImageMemoryBarrier.image = (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) ? m_swapchainImages[imageIndex] : m_drawImage;
	undefinedToColorAttachmentImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	undefinedToColorAttachmentImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	undefinedToColorAttachmentImageMemoryBarrier.subresourceRange.levelCount = 1;
	undefinedToColorAttachmentImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	undefinedToColorAttachmentImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkImageMemoryBarrier2 undefinedToGeneralImageMemoryBarrier = {};
	undefinedToGeneralImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	undefinedToGeneralImageMemoryBarrier.pNext = nullptr;
	undefinedToGeneralImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	undefinedToGeneralImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	undefinedToGeneralImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
	undefinedToGeneralImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	undefinedToGeneralImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	undefinedToGeneralImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
	undefinedToGeneralImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	undefinedToGeneralImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	undefinedToGeneralImageMemoryBarrier.image = m_colorImage;
	undefinedToGeneralImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	undefinedToGeneralImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	undefinedToGeneralImageMemoryBarrier.subresourceRange.levelCount = 1;
	undefinedToGeneralImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	undefinedToGeneralImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkBufferMemoryBarrier2 tlasInstancesCopyBufferMemoryBarrier = {};
	tlasInstancesCopyBufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	tlasInstancesCopyBufferMemoryBarrier.pNext = nullptr;
	tlasInstancesCopyBufferMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
	tlasInstancesCopyBufferMemoryBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	tlasInstancesCopyBufferMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
	tlasInstancesCopyBufferMemoryBarrier.dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;
	tlasInstancesCopyBufferMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	tlasInstancesCopyBufferMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	tlasInstancesCopyBufferMemoryBarrier.buffer = m_topLevelAccelerationStructureInstancesBuffer;
	tlasInstancesCopyBufferMemoryBarrier.offset = 0;
	tlasInstancesCopyBufferMemoryBarrier.size = tlasInstances.size() * sizeof(VkAccelerationStructureInstanceKHR);

	std::array<VkImageMemoryBarrier2, 2> imageMemoryBarriers = { undefinedToColorAttachmentImageMemoryBarrier, undefinedToGeneralImageMemoryBarrier };
	VkDependencyInfo undefinedToColorAttachmentAndTLASInstancesCopyDependencyInfo = {};
	undefinedToColorAttachmentAndTLASInstancesCopyDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	undefinedToColorAttachmentAndTLASInstancesCopyDependencyInfo.pNext = nullptr;
	undefinedToColorAttachmentAndTLASInstancesCopyDependencyInfo.dependencyFlags = 0;
	undefinedToColorAttachmentAndTLASInstancesCopyDependencyInfo.memoryBarrierCount = 0;
	undefinedToColorAttachmentAndTLASInstancesCopyDependencyInfo.pMemoryBarriers = nullptr;
	undefinedToColorAttachmentAndTLASInstancesCopyDependencyInfo.bufferMemoryBarrierCount = tlasInstances.size() != 0 ? 1 : 0;
	undefinedToColorAttachmentAndTLASInstancesCopyDependencyInfo.pBufferMemoryBarriers = tlasInstances.size() != 0 ? &tlasInstancesCopyBufferMemoryBarrier : nullptr;
	undefinedToColorAttachmentAndTLASInstancesCopyDependencyInfo.imageMemoryBarrierCount = static_cast<uint32_t>(imageMemoryBarriers.size());
	undefinedToColorAttachmentAndTLASInstancesCopyDependencyInfo.pImageMemoryBarriers = imageMemoryBarriers.data();
	m_vkCmdPipelineBarrier2KHR(m_renderingCommandBuffers[m_currentFrameInFlight], &undefinedToColorAttachmentAndTLASInstancesCopyDependencyInfo);

	// Build TLAS
	VkAccelerationStructureGeometryInstancesDataKHR accelerationStructureGeometryInstancesData = {};
	accelerationStructureGeometryInstancesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	accelerationStructureGeometryInstancesData.pNext = nullptr;
	accelerationStructureGeometryInstancesData.arrayOfPointers = false;
	accelerationStructureGeometryInstancesData.data.deviceAddress = m_topLevelAccelerationStructureInstancesBufferDeviceAddress;

	VkAccelerationStructureGeometryKHR accelerationStructureGeometry = {};
	accelerationStructureGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	accelerationStructureGeometry.pNext = nullptr;
	accelerationStructureGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	accelerationStructureGeometry.geometry.instances = accelerationStructureGeometryInstancesData;
	accelerationStructureGeometry.flags = 0;

	VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo = {};
	accelerationStructureBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	accelerationStructureBuildGeometryInfo.pNext = nullptr;
	accelerationStructureBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	accelerationStructureBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	accelerationStructureBuildGeometryInfo.srcAccelerationStructure = VK_NULL_HANDLE;
	accelerationStructureBuildGeometryInfo.dstAccelerationStructure = m_topLevelAccelerationStructure;
	accelerationStructureBuildGeometryInfo.geometryCount = 1;
	accelerationStructureBuildGeometryInfo.pGeometries = &accelerationStructureGeometry;
	accelerationStructureBuildGeometryInfo.ppGeometries = nullptr;
	accelerationStructureBuildGeometryInfo.scratchData.deviceAddress = m_topLevelAccelerationStructureScratchBufferDeviceAddress;

	VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo = {};
	accelerationStructureBuildRangeInfo.primitiveCount = static_cast<uint32_t>(tlasInstances.size());
	accelerationStructureBuildRangeInfo.primitiveOffset = 0;
	accelerationStructureBuildRangeInfo.firstVertex = 0;
	accelerationStructureBuildRangeInfo.transformOffset = 0;
	std::array<VkAccelerationStructureBuildRangeInfoKHR*, 1> accelerationStructureBuildRangeInfos = { &accelerationStructureBuildRangeInfo };
	m_vkCmdBuildAccelerationStructuresKHR(m_renderingCommandBuffers[m_currentFrameInFlight], 1, &accelerationStructureBuildGeometryInfo, accelerationStructureBuildRangeInfos.data());

	VkBufferMemoryBarrier2 tlasBuildBufferMemoryBarrier = {};
	tlasBuildBufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	tlasBuildBufferMemoryBarrier.pNext = nullptr;
	tlasBuildBufferMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
	tlasBuildBufferMemoryBarrier.srcAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
	tlasBuildBufferMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
	tlasBuildBufferMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
	tlasBuildBufferMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	tlasBuildBufferMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	tlasBuildBufferMemoryBarrier.buffer = m_topLevelAccelerationStructureBuffer;
	tlasBuildBufferMemoryBarrier.offset = 0;
	tlasBuildBufferMemoryBarrier.size = m_topLevelAccelerationStructureBufferSize;

	VkDependencyInfo tlasBuildDependencyInfo = {};
	tlasBuildDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	tlasBuildDependencyInfo.pNext = nullptr;
	tlasBuildDependencyInfo.dependencyFlags = 0;
	tlasBuildDependencyInfo.memoryBarrierCount = 0;
	tlasBuildDependencyInfo.pMemoryBarriers = nullptr;
	tlasBuildDependencyInfo.bufferMemoryBarrierCount = 1;
	tlasBuildDependencyInfo.pBufferMemoryBarriers = &tlasBuildBufferMemoryBarrier;
	tlasBuildDependencyInfo.imageMemoryBarrierCount = 0;
	tlasBuildDependencyInfo.pImageMemoryBarriers = nullptr;
	m_vkCmdPipelineBarrier2KHR(m_renderingCommandBuffers[m_currentFrameInFlight], &tlasBuildDependencyInfo);

	// Bind descriptor set 0
	vkCmdBindDescriptorSets(m_renderingCommandBuffers[m_currentFrameInFlight], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_rayTracingPipelineLayout, 0, 1, &m_descriptorSets[m_currentFrameInFlight], 0, nullptr);

	// Bind ray tracing pipeline
	vkCmdBindPipeline(m_renderingCommandBuffers[m_currentFrameInFlight], VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_rayTracingPipeline);

	// Push constant
	vkCmdPushConstants(m_renderingCommandBuffers[m_currentFrameInFlight], m_rayTracingPipelineLayout, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, sizeof(uint32_t), &m_sampleBatch);
	vkCmdPushConstants(m_renderingCommandBuffers[m_currentFrameInFlight], m_rayTracingPipelineLayout, VK_SHADER_STAGE_MISS_BIT_KHR, sizeof(Math::vec4), sizeof(Math::vec4), m_backgroundColor.data());

	// Trace rays
	m_vkCmdTraceRaysKHR(m_renderingCommandBuffers[m_currentFrameInFlight], &m_rayGenRegion, &m_rayMissRegion, &m_rayHitRegion, &m_rayCallRegion, static_cast<uint32_t>(m_viewport.width), static_cast<uint32_t>(m_viewport.height), 1);

	// Layout transition VK_IMAGE_LAYOUT_GENERAL -> VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	VkImageMemoryBarrier2 generalToShaderReadOnlyImageMemoryBarrier = {};
	generalToShaderReadOnlyImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	generalToShaderReadOnlyImageMemoryBarrier.pNext = nullptr;
	generalToShaderReadOnlyImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
	generalToShaderReadOnlyImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	generalToShaderReadOnlyImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	generalToShaderReadOnlyImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	generalToShaderReadOnlyImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
	generalToShaderReadOnlyImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	generalToShaderReadOnlyImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	generalToShaderReadOnlyImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	generalToShaderReadOnlyImageMemoryBarrier.image = m_colorImage;
	generalToShaderReadOnlyImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	generalToShaderReadOnlyImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	generalToShaderReadOnlyImageMemoryBarrier.subresourceRange.levelCount = 1;
	generalToShaderReadOnlyImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	generalToShaderReadOnlyImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkDependencyInfo generalToShaderReadOnlyDependencyInfo = {};
	generalToShaderReadOnlyDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	generalToShaderReadOnlyDependencyInfo.pNext = nullptr;
	generalToShaderReadOnlyDependencyInfo.dependencyFlags = 0;
	generalToShaderReadOnlyDependencyInfo.memoryBarrierCount = 0;
	generalToShaderReadOnlyDependencyInfo.pMemoryBarriers = nullptr;
	generalToShaderReadOnlyDependencyInfo.bufferMemoryBarrierCount = 0;
	generalToShaderReadOnlyDependencyInfo.pBufferMemoryBarriers = nullptr;
	generalToShaderReadOnlyDependencyInfo.imageMemoryBarrierCount = 1;
	generalToShaderReadOnlyDependencyInfo.pImageMemoryBarriers = &generalToShaderReadOnlyImageMemoryBarrier;
	m_vkCmdPipelineBarrier2KHR(m_renderingCommandBuffers[m_currentFrameInFlight], &generalToShaderReadOnlyDependencyInfo);

	// Tone mapping
	VkRenderingAttachmentInfo renderingSwapchainAttachmentInfo = {};
	renderingSwapchainAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	renderingSwapchainAttachmentInfo.pNext = nullptr;
	renderingSwapchainAttachmentInfo.imageView = (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) ? m_swapchainImageViews[imageIndex] : m_drawImageView;
	renderingSwapchainAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	renderingSwapchainAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
	renderingSwapchainAttachmentInfo.resolveImageView = VK_NULL_HANDLE;
	renderingSwapchainAttachmentInfo.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	renderingSwapchainAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	renderingSwapchainAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	renderingSwapchainAttachmentInfo.clearValue.color = { 0.0f, 0.0f, 0.0f, 0.0f };

	VkRenderingInfo toneMappingRenderingInfo = {};
	toneMappingRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	toneMappingRenderingInfo.pNext = nullptr;
	toneMappingRenderingInfo.flags = 0;
	toneMappingRenderingInfo.renderArea = m_scissor;
	toneMappingRenderingInfo.layerCount = 1;
	toneMappingRenderingInfo.viewMask = 0;
	toneMappingRenderingInfo.colorAttachmentCount = 1;
	toneMappingRenderingInfo.pColorAttachments = &renderingSwapchainAttachmentInfo;
	toneMappingRenderingInfo.pDepthAttachment = nullptr;
	toneMappingRenderingInfo.pStencilAttachment = nullptr;
	m_vkCmdBeginRenderingKHR(m_renderingCommandBuffers[m_currentFrameInFlight], &toneMappingRenderingInfo);

	vkCmdBindDescriptorSets(m_renderingCommandBuffers[m_currentFrameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_toneMappingGraphicsPipelineLayout, 0, 1, &m_toneMappingDescriptorSet, 0, nullptr);
	vkCmdBindPipeline(m_renderingCommandBuffers[m_currentFrameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_toneMappingGraphicsPipeline);
	vkCmdSetViewport(m_renderingCommandBuffers[m_currentFrameInFlight], 0, 1, &m_viewport);
	vkCmdSetScissor(m_renderingCommandBuffers[m_currentFrameInFlight], 0, 1, &m_scissor);

	vkCmdDraw(m_renderingCommandBuffers[m_currentFrameInFlight], 3, 1, 0, 0);

	m_vkCmdEndRenderingKHR(m_renderingCommandBuffers[m_currentFrameInFlight]);

	if (!m_uiElements.empty()) {
		// Image memory barrier between tonemapping and UI
		VkImageMemoryBarrier2 tonemappingUIImageMemoryBarrier = {};
		tonemappingUIImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		tonemappingUIImageMemoryBarrier.pNext = nullptr;
		tonemappingUIImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		tonemappingUIImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		tonemappingUIImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		tonemappingUIImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		tonemappingUIImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		tonemappingUIImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		tonemappingUIImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
		tonemappingUIImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
		tonemappingUIImageMemoryBarrier.image = (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) ? m_swapchainImages[imageIndex] : m_drawImage;
		tonemappingUIImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		tonemappingUIImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
		tonemappingUIImageMemoryBarrier.subresourceRange.levelCount = 1;
		tonemappingUIImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
		tonemappingUIImageMemoryBarrier.subresourceRange.layerCount = 1;

		VkDependencyInfo tonemappingUIDependencyInfo = {};
		tonemappingUIDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		tonemappingUIDependencyInfo.pNext = nullptr;
		tonemappingUIDependencyInfo.dependencyFlags = 0;
		tonemappingUIDependencyInfo.memoryBarrierCount = 0;
		tonemappingUIDependencyInfo.pMemoryBarriers = nullptr;
		tonemappingUIDependencyInfo.bufferMemoryBarrierCount = 0;
		tonemappingUIDependencyInfo.pBufferMemoryBarriers = nullptr;
		tonemappingUIDependencyInfo.imageMemoryBarrierCount = 1;
		tonemappingUIDependencyInfo.pImageMemoryBarriers = &tonemappingUIImageMemoryBarrier;
		m_vkCmdPipelineBarrier2KHR(m_renderingCommandBuffers[m_currentFrameInFlight], &tonemappingUIDependencyInfo);

		// UI
		renderingSwapchainAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;

		VkRenderingInfo uiRenderingInfo = {};
		uiRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		uiRenderingInfo.pNext = nullptr;
		uiRenderingInfo.flags = 0;
		uiRenderingInfo.renderArea = m_scissor;
		uiRenderingInfo.layerCount = 1;
		uiRenderingInfo.viewMask = 0;
		uiRenderingInfo.colorAttachmentCount = 1;
		uiRenderingInfo.pColorAttachments = &renderingSwapchainAttachmentInfo;
		uiRenderingInfo.pDepthAttachment = nullptr;
		uiRenderingInfo.pStencilAttachment = nullptr;
		m_vkCmdBeginRenderingKHR(m_renderingCommandBuffers[m_currentFrameInFlight], &uiRenderingInfo);

		vkCmdBindDescriptorSets(m_renderingCommandBuffers[m_currentFrameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_uiTextGraphicsPipelineLayout, 0, 1, &m_uiTextDescriptorSets[m_currentFrameInFlight], 0, nullptr);
		vkCmdBindPipeline(m_renderingCommandBuffers[m_currentFrameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_uiTextGraphicsPipeline);
		vkCmdSetViewport(m_renderingCommandBuffers[m_currentFrameInFlight], 0, 1, &m_viewport);
		vkCmdSetScissor(m_renderingCommandBuffers[m_currentFrameInFlight], 0, 1, &m_scissor);

		while (!m_uiElements.empty()) {
			const UIElement uiElement = m_uiElements.front();
			if (uiElement == UIElement::Text) {
				const InternalUIText& uiText = m_uiTexts.front();

				vkCmdBindPipeline(m_renderingCommandBuffers[m_currentFrameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_uiTextGraphicsPipeline);
				vkCmdBindDescriptorSets(m_renderingCommandBuffers[m_currentFrameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_uiTextGraphicsPipelineLayout, 0, 1, &m_uiTextDescriptorSets[m_currentFrameInFlight], 0, nullptr);
				vkCmdPushConstants(m_renderingCommandBuffers[m_currentFrameInFlight], m_uiTextGraphicsPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t), &uiText.bufferOffset);
				vkCmdPushConstants(m_renderingCommandBuffers[m_currentFrameInFlight], m_uiTextGraphicsPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(Math::vec4), sizeof(Math::vec4) + sizeof(uint32_t), &uiText.color);

				vkCmdDraw(m_renderingCommandBuffers[m_currentFrameInFlight], uiText.charactersCount * 6, 1, 0, 0);

				m_uiTexts.pop();
			}
			else if (uiElement == UIElement::Line) {
				const InternalUILine& uiLine = m_uiLines.front();

				vkCmdBindPipeline(m_renderingCommandBuffers[m_currentFrameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_uiLineGraphicsPipeline);
				vkCmdPushConstants(m_renderingCommandBuffers[m_currentFrameInFlight], m_uiLineGraphicsPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Math::vec2) + sizeof(Math::vec2), &uiLine.positions);
				vkCmdPushConstants(m_renderingCommandBuffers[m_currentFrameInFlight], m_uiLineGraphicsPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(Math::vec2) + sizeof(Math::vec2), sizeof(Math::vec4), &uiLine.color);

				vkCmdDraw(m_renderingCommandBuffers[m_currentFrameInFlight], 2, 1, 0, 0);

				m_uiLines.pop();
			}
			else if (uiElement == UIElement::Rectangle) {
				const InternalUIRectangle& uiRectangle = m_uiRectangles.front();

				vkCmdBindPipeline(m_renderingCommandBuffers[m_currentFrameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_uiRectangleGraphicsPipeline);
				vkCmdPushConstants(m_renderingCommandBuffers[m_currentFrameInFlight], m_uiRectangleGraphicsPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(Math::vec2) + sizeof(Math::vec2), &uiRectangle.positions);
				vkCmdPushConstants(m_renderingCommandBuffers[m_currentFrameInFlight], m_uiRectangleGraphicsPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(Math::vec2) + sizeof(Math::vec2), sizeof(Math::vec4), &uiRectangle.color);

				vkCmdDraw(m_renderingCommandBuffers[m_currentFrameInFlight], 6, 1, 0, 0);

				m_uiRectangles.pop();
			}
			else if (uiElement == UIElement::Image) {
				const InternalUIImage& uiImage = m_uiImages.front();

				vkCmdBindPipeline(m_renderingCommandBuffers[m_currentFrameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_uiImageGraphicsPipeline);
				vkCmdBindDescriptorSets(m_renderingCommandBuffers[m_currentFrameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_uiImageGraphicsPipelineLayout, 0, 1, &m_uiImageDescriptorSets[m_currentFrameInFlight], 0, nullptr);
				vkCmdPushConstants(m_renderingCommandBuffers[m_currentFrameInFlight], m_uiImageGraphicsPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, 5 * sizeof(Math::vec2), &uiImage.v0);
				vkCmdPushConstants(m_renderingCommandBuffers[m_currentFrameInFlight], m_uiImageGraphicsPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 5 * sizeof(Math::vec2) + sizeof(Math::vec2), sizeof(Math::vec4) + sizeof(uint32_t), &uiImage.color);

				vkCmdDraw(m_renderingCommandBuffers[m_currentFrameInFlight], 6, 1, 0, 0);

				m_uiImages.pop();
			}

			m_uiElements.pop();
		}

		m_vkCmdEndRenderingKHR(m_renderingCommandBuffers[m_currentFrameInFlight]);
	}

	// Layout transition VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL -> VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	if (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) {
		VkImageMemoryBarrier2 colorAttachmentToPresentSrcImageMemoryBarrier = {};
		colorAttachmentToPresentSrcImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		colorAttachmentToPresentSrcImageMemoryBarrier.pNext = nullptr;
		colorAttachmentToPresentSrcImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		colorAttachmentToPresentSrcImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		colorAttachmentToPresentSrcImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
		colorAttachmentToPresentSrcImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_NONE;
		colorAttachmentToPresentSrcImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		colorAttachmentToPresentSrcImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		colorAttachmentToPresentSrcImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
		colorAttachmentToPresentSrcImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
		colorAttachmentToPresentSrcImageMemoryBarrier.image = m_swapchainImages[imageIndex];
		colorAttachmentToPresentSrcImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		colorAttachmentToPresentSrcImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
		colorAttachmentToPresentSrcImageMemoryBarrier.subresourceRange.levelCount = 1;
		colorAttachmentToPresentSrcImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
		colorAttachmentToPresentSrcImageMemoryBarrier.subresourceRange.layerCount = 1;

		VkDependencyInfo colorAttachmentToPresentSrcDependencyInfo = {};
		colorAttachmentToPresentSrcDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		colorAttachmentToPresentSrcDependencyInfo.pNext = nullptr;
		colorAttachmentToPresentSrcDependencyInfo.dependencyFlags = 0;
		colorAttachmentToPresentSrcDependencyInfo.memoryBarrierCount = 0;
		colorAttachmentToPresentSrcDependencyInfo.pMemoryBarriers = nullptr;
		colorAttachmentToPresentSrcDependencyInfo.bufferMemoryBarrierCount = 0;
		colorAttachmentToPresentSrcDependencyInfo.pBufferMemoryBarriers = nullptr;
		colorAttachmentToPresentSrcDependencyInfo.imageMemoryBarrierCount = 1;
		colorAttachmentToPresentSrcDependencyInfo.pImageMemoryBarriers = &colorAttachmentToPresentSrcImageMemoryBarrier;
		m_vkCmdPipelineBarrier2KHR(m_renderingCommandBuffers[m_currentFrameInFlight], &colorAttachmentToPresentSrcDependencyInfo);
	}

	// End command buffer recording
	NTSHENGN_VK_CHECK(vkEndCommandBuffer(m_renderingCommandBuffers[m_currentFrameInFlight]));

	NTSHENGN_VK_CHECK(vkResetFences(m_device, 1, &m_fences[m_currentFrameInFlight]));

	VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &m_imageAvailableSemaphores[m_currentFrameInFlight];
	submitInfo.pWaitDstStageMask = &waitDstStageMask;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_renderingCommandBuffers[m_currentFrameInFlight];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &m_renderFinishedSemaphores[imageIndex];
	NTSHENGN_VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_fences[m_currentFrameInFlight]));

	m_sampleBatch++;

	if (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) {
		VkPresentInfoKHR presentInfo = {};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.pNext = nullptr;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = &m_renderFinishedSemaphores[imageIndex];
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = &m_swapchain;
		presentInfo.pImageIndices = &imageIndex;
		presentInfo.pResults = nullptr;
		VkResult queuePresentResult = vkQueuePresentKHR(m_graphicsQueue, &presentInfo);
		if (queuePresentResult == VK_ERROR_OUT_OF_DATE_KHR || queuePresentResult == VK_SUBOPTIMAL_KHR) {
			resize();
		}
		else if (queuePresentResult != VK_SUCCESS) {
			NTSHENGN_MODULE_ERROR("Queue present swapchain image failed.", Result::ModuleError);
		}
	}
	else {
		VkPipelineStageFlags emptyWaitDstStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
		VkSubmitInfo emptyWaitSubmitInfo = {};
		emptyWaitSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		emptyWaitSubmitInfo.pNext = nullptr;
		emptyWaitSubmitInfo.waitSemaphoreCount = 1;
		emptyWaitSubmitInfo.pWaitSemaphores = &m_renderFinishedSemaphores[imageIndex];
		emptyWaitSubmitInfo.pWaitDstStageMask = &emptyWaitDstStageMask;
		emptyWaitSubmitInfo.commandBufferCount = 0;
		emptyWaitSubmitInfo.pCommandBuffers = nullptr;
		emptyWaitSubmitInfo.signalSemaphoreCount = 0;
		emptyWaitSubmitInfo.pSignalSemaphores = nullptr;
		NTSHENGN_VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &emptyWaitSubmitInfo, VK_NULL_HANDLE));
	}

	m_uiTextBufferOffset = 0;

	m_currentFrameInFlight = (m_currentFrameInFlight + 1) % m_framesInFlight;
}

void NtshEngn::GraphicsModule::destroy() {
	NTSHENGN_VK_CHECK(vkQueueWaitIdle(m_graphicsQueue));

	// Destroy sync objects
	for (uint32_t i = 0; i < m_imageCount; i++) {
		vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
	}
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);

		vkDestroyFence(m_device, m_fences[i], nullptr);

		// Destroy rendering command pools
		vkDestroyCommandPool(m_device, m_renderingCommandPools[i], nullptr);
	}

	// Destroy light buffers
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		vmaDestroyBuffer(m_allocator, m_lightBuffers[i].handle, m_lightBuffers[i].allocation);
	}

	// Destroy material buffers
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		vmaDestroyBuffer(m_allocator, m_materialBuffers[i].handle, m_materialBuffers[i].allocation);
	}

	// Destroy mesh buffers
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		vmaDestroyBuffer(m_allocator, m_meshBuffers[i].handle, m_meshBuffers[i].allocation);
	}

	// Destroy object buffers
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		vmaDestroyBuffer(m_allocator, m_objectBuffers[i].handle, m_objectBuffers[i].allocation);
	}

	// Destroy camera buffers
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		vmaDestroyBuffer(m_allocator, m_cameraBuffers[i].handle, m_cameraBuffers[i].allocation);
	}

	// Destroy UI resources
	vkDestroyDescriptorPool(m_device, m_uiImageDescriptorPool, nullptr);
	vkDestroyPipeline(m_device, m_uiImageGraphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_uiImageGraphicsPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(m_device, m_uiImageDescriptorSetLayout, nullptr);

	vkDestroyPipeline(m_device, m_uiRectangleGraphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_uiRectangleGraphicsPipelineLayout, nullptr);

	vkDestroyPipeline(m_device, m_uiLineGraphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_uiLineGraphicsPipelineLayout, nullptr);

	for (size_t i = 0; i < m_fonts.size(); i++) {
		vkDestroyImageView(m_device, m_fonts[i].imageView, nullptr);
		vmaDestroyImage(m_allocator, m_fonts[i].image, m_fonts[i].imageAllocation);
	}
	vkDestroyDescriptorPool(m_device, m_uiTextDescriptorPool, nullptr);
	vkDestroyPipeline(m_device, m_uiTextGraphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_uiTextGraphicsPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(m_device, m_uiTextDescriptorSetLayout, nullptr);
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		vmaDestroyBuffer(m_allocator, m_uiTextBuffers[i].handle, m_uiTextBuffers[i].allocation);
	}

	vkDestroySampler(m_device, m_uiLinearSampler, nullptr);
	vkDestroySampler(m_device, m_uiNearestSampler, nullptr);

	// Destroy tone mapping resources
	vkDestroyDescriptorPool(m_device, m_toneMappingDescriptorPool, nullptr);
	vkDestroyPipeline(m_device, m_toneMappingGraphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_toneMappingGraphicsPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(m_device, m_toneMappingDescriptorSetLayout, nullptr);
	vkDestroySampler(m_device, m_toneMappingSampler, nullptr);

	// Destroy descriptor pool
	vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);

	// Destroy ray tracing shader binding table buffer
	vmaDestroyBuffer(m_allocator, m_rayTracingShaderBindingTableBuffer, m_rayTracingShaderBindingTableBufferAllocation);

	// Destroy ray tracing pipeline
	vkDestroyPipeline(m_device, m_rayTracingPipeline, nullptr);

	// Destroy ray tracing pipeline layout
	vkDestroyPipelineLayout(m_device, m_rayTracingPipelineLayout, nullptr);

	// Destroy descriptor set layout
	vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);

	// Destroy color image and image view
	vkDestroyImageView(m_device, m_colorImageView, nullptr);
	vmaDestroyImage(m_allocator, m_colorImage, m_colorImageAllocation);

	// Destroy samplers
	for (const auto& sampler : m_textureSamplers) {
		vkDestroySampler(m_device, sampler.second, nullptr);
	}

	// Destroy textures
	for (size_t i = 0; i < m_textureImages.size(); i++) {
		vkDestroyImageView(m_device, m_textureImageViews[i], nullptr);
		vmaDestroyImage(m_allocator, m_textureImages[i], m_textureImageAllocations[i]);
	}

	// Destroy acceleration structures
	for (size_t i = 0; i < m_bottomLevelAccelerationStructures.size(); i++) {
		m_vkDestroyAccelerationStructureKHR(m_device, m_bottomLevelAccelerationStructures[i], nullptr);
	}
	m_vkDestroyAccelerationStructureKHR(m_device, m_topLevelAccelerationStructure, nullptr);

	// Destroy vertex, index and acceleration structure buffers
	vmaDestroyBuffer(m_allocator, m_topLevelAccelerationStructureScratchBuffer, m_topLevelAccelerationStructureScratchBufferAllocation);
	vmaDestroyBuffer(m_allocator, m_topLevelAccelerationStructureBuffer, m_topLevelAccelerationStructureBufferAllocation);

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		vmaDestroyBuffer(m_allocator, m_topLevelAccelerationStructureInstancesStagingBuffers[i].handle, m_topLevelAccelerationStructureInstancesStagingBuffers[i].allocation);
	}
	vmaDestroyBuffer(m_allocator, m_topLevelAccelerationStructureInstancesBuffer, m_topLevelAccelerationStructureInstancesBufferAllocation);
	vmaDestroyBuffer(m_allocator, m_bottomLevelAccelerationStructureBuffer, m_bottomLevelAccelerationStructureBufferAllocation);
	vmaDestroyBuffer(m_allocator, m_indexBuffer, m_indexBufferAllocation);
	vmaDestroyBuffer(m_allocator, m_vertexBuffer, m_vertexBufferAllocation);

	// Destroy initialization fence
	vkDestroyFence(m_device, m_initializationFence, nullptr);

	// Destroy initialization command pool
	vkDestroyCommandPool(m_device, m_initializationCommandPool, nullptr);

	// Destroy swapchain
	if (m_swapchain != VK_NULL_HANDLE) {
		for (VkImageView& swapchainImageView : m_swapchainImageViews) {
			vkDestroyImageView(m_device, swapchainImageView, nullptr);
		}
		vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
	}
	// Or destroy the image
	else {
		vkDestroyImageView(m_device, m_drawImageView, nullptr);
		vmaDestroyImage(m_allocator, m_drawImage, m_drawImageAllocation);
	}

	// Destroy VMA Allocator
	vmaDestroyAllocator(m_allocator);

	// Destroy device
	vkDestroyDevice(m_device, nullptr);

	// Destroy surface
	if (m_surface != VK_NULL_HANDLE) {
		vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
	}

#if defined(NTSHENGN_DEBUG)
	// Destroy debug messenger
	auto destroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT");
	destroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
#endif

	// Destroy instance
	vkDestroyInstance(m_instance, nullptr);
}

NtshEngn::MeshID NtshEngn::GraphicsModule::load(const Mesh& mesh) {
	if (m_meshAddresses.find(&mesh) != m_meshAddresses.end()) {
		return m_meshAddresses[&mesh];
	}

	// Vertex and Index staging buffer
	VkBuffer vertexAndIndexStagingBuffer;
	VmaAllocation vertexAndIndexStagingBufferAllocation;

	VkBufferCreateInfo vertexAndIndexStagingBufferCreateInfo = {};
	vertexAndIndexStagingBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	vertexAndIndexStagingBufferCreateInfo.pNext = nullptr;
	vertexAndIndexStagingBufferCreateInfo.flags = 0;
	vertexAndIndexStagingBufferCreateInfo.size = (mesh.vertices.size() * sizeof(Vertex)) + (mesh.indices.size() * sizeof(uint32_t));
	vertexAndIndexStagingBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	vertexAndIndexStagingBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	vertexAndIndexStagingBufferCreateInfo.queueFamilyIndexCount = 1;
	vertexAndIndexStagingBufferCreateInfo.pQueueFamilyIndices = &m_graphicsQueueFamilyIndex;

	VmaAllocationCreateInfo vertexAndIndexStagingBufferAllocationCreateInfo = {};
	vertexAndIndexStagingBufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	vertexAndIndexStagingBufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
	NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &vertexAndIndexStagingBufferCreateInfo, &vertexAndIndexStagingBufferAllocationCreateInfo, &vertexAndIndexStagingBuffer, &vertexAndIndexStagingBufferAllocation, nullptr));

	void* data;

	NTSHENGN_VK_CHECK(vmaMapMemory(m_allocator, vertexAndIndexStagingBufferAllocation, &data));
	memcpy(data, mesh.vertices.data(), mesh.vertices.size() * sizeof(Vertex));
	memcpy(reinterpret_cast<char*>(data) + (mesh.vertices.size() * sizeof(Vertex)), mesh.indices.data(), mesh.indices.size() * sizeof(uint32_t));
	vmaUnmapMemory(m_allocator, vertexAndIndexStagingBufferAllocation);

	// BLAS
	VkAccelerationStructureGeometryTrianglesDataKHR blasGeometryTrianglesData = {};
	blasGeometryTrianglesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
	blasGeometryTrianglesData.pNext = nullptr;
	blasGeometryTrianglesData.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
	blasGeometryTrianglesData.vertexData.deviceAddress = m_vertexBufferDeviceAddress + (static_cast<size_t>(m_currentVertexOffset) * sizeof(Vertex));
	blasGeometryTrianglesData.vertexStride = sizeof(Vertex);
	blasGeometryTrianglesData.maxVertex = static_cast<uint32_t>(mesh.vertices.size());
	blasGeometryTrianglesData.indexType = VK_INDEX_TYPE_UINT32;
	blasGeometryTrianglesData.indexData.deviceAddress = m_indexBufferDeviceAddress + (static_cast<size_t>(m_currentIndexOffset) * sizeof(uint32_t));
	blasGeometryTrianglesData.transformData = {};

	VkAccelerationStructureGeometryKHR blasGeometry = {};
	blasGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	blasGeometry.pNext = nullptr;
	blasGeometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
	blasGeometry.geometry.triangles = blasGeometryTrianglesData;
	blasGeometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;

	VkAccelerationStructureBuildRangeInfoKHR blasBuildRangeInfo = {};
	blasBuildRangeInfo.primitiveCount = static_cast<uint32_t>(mesh.indices.size()) / 3;
	blasBuildRangeInfo.primitiveOffset = 0;
	blasBuildRangeInfo.firstVertex = 0;
	blasBuildRangeInfo.transformOffset = 0;

	VkAccelerationStructureBuildGeometryInfoKHR blasBuildGeometryInfo = {};
	blasBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	blasBuildGeometryInfo.pNext = nullptr;
	blasBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	blasBuildGeometryInfo.flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
	blasBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	blasBuildGeometryInfo.srcAccelerationStructure = VK_NULL_HANDLE;
	blasBuildGeometryInfo.dstAccelerationStructure = VK_NULL_HANDLE;
	blasBuildGeometryInfo.geometryCount = 1;
	blasBuildGeometryInfo.pGeometries = &blasGeometry;
	blasBuildGeometryInfo.ppGeometries = nullptr;
	blasBuildGeometryInfo.scratchData = {};

	VkAccelerationStructureBuildSizesInfoKHR blasBuildSizesInfo = {};
	blasBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	blasBuildSizesInfo.pNext = nullptr;
	m_vkGetAccelerationStructureBuildSizesKHR(m_device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &blasBuildGeometryInfo, &blasBuildRangeInfo.primitiveCount, &blasBuildSizesInfo);

	VkBuffer blasScratchBuffer;
	VmaAllocation blasScratchBufferAllocation;

	VkBufferCreateInfo blasScratchBufferCreateInfo = {};
	blasScratchBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	blasScratchBufferCreateInfo.pNext = nullptr;
	blasScratchBufferCreateInfo.flags = 0;
	blasScratchBufferCreateInfo.size = blasBuildSizesInfo.buildScratchSize;
	blasScratchBufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	blasScratchBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	blasScratchBufferCreateInfo.queueFamilyIndexCount = 1;
	blasScratchBufferCreateInfo.pQueueFamilyIndices = &m_graphicsQueueFamilyIndex;

	VmaAllocationCreateInfo blasScratchBufferAllocationCreateInfo = {};
	blasScratchBufferAllocationCreateInfo.flags = 0;
	blasScratchBufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &blasScratchBufferCreateInfo, &blasScratchBufferAllocationCreateInfo, &blasScratchBuffer, &blasScratchBufferAllocation, nullptr));

	VkBufferDeviceAddressInfoKHR blasScratchBufferDeviceAddressInfo = {};
	blasScratchBufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR;
	blasScratchBufferDeviceAddressInfo.pNext = nullptr;
	blasScratchBufferDeviceAddressInfo.buffer = blasScratchBuffer;
	VkDeviceAddress blasScratchBufferDeviceAddress = m_vkGetBufferDeviceAddressKHR(m_device, &blasScratchBufferDeviceAddressInfo);

	blasBuildGeometryInfo.scratchData.deviceAddress = blasScratchBufferDeviceAddress;

	VkAccelerationStructureKHR blas;

	VkAccelerationStructureCreateInfoKHR blasCreateInfo = {};
	blasCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	blasCreateInfo.pNext = nullptr;
	blasCreateInfo.createFlags = 0;
	blasCreateInfo.buffer = m_bottomLevelAccelerationStructureBuffer;
	blasCreateInfo.offset = m_currentBottomLevelAccelerationStructureOffset;
	blasCreateInfo.size = blasBuildSizesInfo.accelerationStructureSize;
	blasCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
	blasCreateInfo.deviceAddress = 0;
	NTSHENGN_VK_CHECK(m_vkCreateAccelerationStructureKHR(m_device, &blasCreateInfo, nullptr, &blas));

	m_bottomLevelAccelerationStructures.push_back(blas);

	blasBuildGeometryInfo.dstAccelerationStructure = blas;

	// Copy staging buffer and build BLAS
	NTSHENGN_VK_CHECK(vkResetCommandPool(m_device, m_initializationCommandPool, 0));

	VkCommandBufferBeginInfo vertexAndIndexBuffersCopyBeginInfo = {};
	vertexAndIndexBuffersCopyBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	vertexAndIndexBuffersCopyBeginInfo.pNext = nullptr;
	vertexAndIndexBuffersCopyBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vertexAndIndexBuffersCopyBeginInfo.pInheritanceInfo = nullptr;
	NTSHENGN_VK_CHECK(vkBeginCommandBuffer(m_initializationCommandBuffer, &vertexAndIndexBuffersCopyBeginInfo));

	VkBufferCopy vertexBufferCopy = {};
	vertexBufferCopy.srcOffset = 0;
	vertexBufferCopy.dstOffset = m_currentVertexOffset * sizeof(Vertex);
	vertexBufferCopy.size = mesh.vertices.size() * sizeof(Vertex);
	vkCmdCopyBuffer(m_initializationCommandBuffer, vertexAndIndexStagingBuffer, m_vertexBuffer, 1, &vertexBufferCopy);

	VkBufferCopy indexBufferCopy = {};
	indexBufferCopy.srcOffset = mesh.vertices.size() * sizeof(Vertex);
	indexBufferCopy.dstOffset = m_currentIndexOffset * sizeof(uint32_t);
	indexBufferCopy.size = mesh.indices.size() * sizeof(uint32_t);
	vkCmdCopyBuffer(m_initializationCommandBuffer, vertexAndIndexStagingBuffer, m_indexBuffer, 1, &indexBufferCopy);

	VkBufferMemoryBarrier2 copyToBLASVertexBufferMemoryBarrier = {};
	copyToBLASVertexBufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	copyToBLASVertexBufferMemoryBarrier.pNext = nullptr;
	copyToBLASVertexBufferMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
	copyToBLASVertexBufferMemoryBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	copyToBLASVertexBufferMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
	copyToBLASVertexBufferMemoryBarrier.dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;
	copyToBLASVertexBufferMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	copyToBLASVertexBufferMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	copyToBLASVertexBufferMemoryBarrier.buffer = m_vertexBuffer;
	copyToBLASVertexBufferMemoryBarrier.offset = m_currentVertexOffset * sizeof(Vertex);
	copyToBLASVertexBufferMemoryBarrier.size = mesh.vertices.size() * sizeof(Vertex);

	VkBufferMemoryBarrier2 copyToBLASIndexBufferMemoryBarrier = {};
	copyToBLASIndexBufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	copyToBLASIndexBufferMemoryBarrier.pNext = nullptr;
	copyToBLASIndexBufferMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
	copyToBLASIndexBufferMemoryBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	copyToBLASIndexBufferMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
	copyToBLASIndexBufferMemoryBarrier.dstAccessMask = VK_ACCESS_2_ACCELERATION_STRUCTURE_READ_BIT_KHR;
	copyToBLASIndexBufferMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	copyToBLASIndexBufferMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	copyToBLASIndexBufferMemoryBarrier.buffer = m_indexBuffer;
	copyToBLASIndexBufferMemoryBarrier.offset = m_currentIndexOffset * sizeof(uint32_t);
	copyToBLASIndexBufferMemoryBarrier.size = mesh.indices.size() * sizeof(uint32_t);

	std::array<VkBufferMemoryBarrier2, 2> copyToBLASBufferMemoryBarriers = { copyToBLASVertexBufferMemoryBarrier, copyToBLASIndexBufferMemoryBarrier };
	VkDependencyInfo copyToBLASDependencyInfo = {};
	copyToBLASDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	copyToBLASDependencyInfo.pNext = nullptr;
	copyToBLASDependencyInfo.dependencyFlags = 0;
	copyToBLASDependencyInfo.memoryBarrierCount = 0;
	copyToBLASDependencyInfo.pMemoryBarriers = 0;
	copyToBLASDependencyInfo.bufferMemoryBarrierCount = 2;
	copyToBLASDependencyInfo.pBufferMemoryBarriers = copyToBLASBufferMemoryBarriers.data();
	copyToBLASDependencyInfo.imageMemoryBarrierCount = 0;
	copyToBLASDependencyInfo.pImageMemoryBarriers = nullptr;
	m_vkCmdPipelineBarrier2KHR(m_initializationCommandBuffer, &copyToBLASDependencyInfo);

	std::array<VkAccelerationStructureBuildRangeInfoKHR*, 1> blasBuildRangeInfos = { &blasBuildRangeInfo };
	m_vkCmdBuildAccelerationStructuresKHR(m_initializationCommandBuffer, 1, &blasBuildGeometryInfo, blasBuildRangeInfos.data());

	NTSHENGN_VK_CHECK(vkEndCommandBuffer(m_initializationCommandBuffer));

	VkSubmitInfo buffersCopySubmitInfo = {};
	buffersCopySubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	buffersCopySubmitInfo.pNext = nullptr;
	buffersCopySubmitInfo.waitSemaphoreCount = 0;
	buffersCopySubmitInfo.pWaitSemaphores = nullptr;
	buffersCopySubmitInfo.pWaitDstStageMask = nullptr;
	buffersCopySubmitInfo.commandBufferCount = 1;
	buffersCopySubmitInfo.pCommandBuffers = &m_initializationCommandBuffer;
	buffersCopySubmitInfo.signalSemaphoreCount = 0;
	buffersCopySubmitInfo.pSignalSemaphores = nullptr;
	NTSHENGN_VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &buffersCopySubmitInfo, m_initializationFence));
	NTSHENGN_VK_CHECK(vkWaitForFences(m_device, 1, &m_initializationFence, VK_TRUE, std::numeric_limits<uint64_t>::max()));
	NTSHENGN_VK_CHECK(vkResetFences(m_device, 1, &m_initializationFence));

	vmaDestroyBuffer(m_allocator, blasScratchBuffer, blasScratchBufferAllocation);
	vmaDestroyBuffer(m_allocator, vertexAndIndexStagingBuffer, vertexAndIndexStagingBufferAllocation);

	VkAccelerationStructureDeviceAddressInfoKHR blasDeviceAddressInfo = {};
	blasDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
	blasDeviceAddressInfo.pNext = nullptr;
	blasDeviceAddressInfo.accelerationStructure = blas;
	VkDeviceAddress blasDeviceAddress = m_vkGetAccelerationStructureDeviceAddressKHR(m_device, &blasDeviceAddressInfo);

	m_meshes.push_back({ static_cast<uint32_t>(mesh.indices.size()), m_currentIndexOffset, m_currentVertexOffset, m_vertexBufferDeviceAddress + (static_cast<size_t>(m_currentVertexOffset) * sizeof(Vertex)), m_indexBufferDeviceAddress + (static_cast<size_t>(m_currentIndexOffset) * sizeof(uint32_t)), blasDeviceAddress });
	m_meshAddresses[&mesh] = static_cast<MeshID>(m_meshes.size() - 1);

	m_currentVertexOffset += static_cast<int32_t>(mesh.vertices.size());
	m_currentIndexOffset += static_cast<uint32_t>(mesh.indices.size());
	VkDeviceSize bottomLevelAccelerationStructureOffsetWithSize = (m_currentBottomLevelAccelerationStructureOffset + blasBuildSizesInfo.accelerationStructureSize);
	VkDeviceSize bottomLevelAccelerationStructureAlignment = 256;
	m_currentBottomLevelAccelerationStructureOffset = (bottomLevelAccelerationStructureOffsetWithSize + (bottomLevelAccelerationStructureAlignment - 1)) & ~(bottomLevelAccelerationStructureAlignment - 1);

	return static_cast<MeshID>(m_meshes.size() - 1);
}

NtshEngn::ImageID NtshEngn::GraphicsModule::load(const Image& image) {
	if (m_imageAddresses.find(&image) != m_imageAddresses.end()) {
		return m_imageAddresses[&image];
	}

	m_imageAddresses[&image] = static_cast<ImageID>(m_textureImages.size());

	VkFormat imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
	size_t numComponents = 4;
	size_t sizeComponent = 1;

	if (image.colorSpace == ImageColorSpace::SRGB) {
		switch (image.format) {
		case ImageFormat::R8:
			imageFormat = VK_FORMAT_R8_SRGB;
			numComponents = 1;
			sizeComponent = 1;
			break;
		case ImageFormat::R8G8:
			imageFormat = VK_FORMAT_R8G8_SRGB;
			numComponents = 2;
			sizeComponent = 1;
			break;
		case ImageFormat::R8G8B8:
			imageFormat = VK_FORMAT_R8G8B8_SRGB;
			numComponents = 3;
			sizeComponent = 1;
			break;
		case ImageFormat::R8G8B8A8:
			imageFormat = VK_FORMAT_R8G8B8A8_SRGB;
			numComponents = 4;
			sizeComponent = 1;
			break;
		case ImageFormat::R16:
			imageFormat = VK_FORMAT_R16_SFLOAT;
			numComponents = 1;
			sizeComponent = 2;
			break;
		case ImageFormat::R16G16:
			imageFormat = VK_FORMAT_R16G16_SFLOAT;
			numComponents = 2;
			sizeComponent = 2;
			break;
		case ImageFormat::R16G16B16:
			imageFormat = VK_FORMAT_R16G16B16_SFLOAT;
			numComponents = 3;
			sizeComponent = 2;
			break;
		case ImageFormat::R16G16B16A16:
			imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
			numComponents = 4;
			sizeComponent = 2;
			break;
		case ImageFormat::R32:
			imageFormat = VK_FORMAT_R32_SFLOAT;
			numComponents = 1;
			sizeComponent = 4;
			break;
		case ImageFormat::R32G32:
			imageFormat = VK_FORMAT_R32G32_SFLOAT;
			numComponents = 2;
			sizeComponent = 4;
			break;
		case ImageFormat::R32G32B32:
			imageFormat = VK_FORMAT_R32G32B32_SFLOAT;
			numComponents = 3;
			sizeComponent = 4;
			break;
		case ImageFormat::R32G32B32A32:
			imageFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
			numComponents = 4;
			sizeComponent = 4;
			break;
		default:
			NTSHENGN_MODULE_ERROR("Image format unrecognized.", Result::ModuleError);
		}
	}
	else if (image.colorSpace == ImageColorSpace::Linear) {
		switch (image.format) {
		case ImageFormat::R8:
			imageFormat = VK_FORMAT_R8_UNORM;
			numComponents = 1;
			sizeComponent = 1;
			break;
		case ImageFormat::R8G8:
			imageFormat = VK_FORMAT_R8G8_UNORM;
			numComponents = 2;
			sizeComponent = 1;
			break;
		case ImageFormat::R8G8B8:
			imageFormat = VK_FORMAT_R8G8B8_UNORM;
			numComponents = 3;
			sizeComponent = 1;
			break;
		case ImageFormat::R8G8B8A8:
			imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
			numComponents = 4;
			sizeComponent = 1;
			break;
		case ImageFormat::R16:
			imageFormat = VK_FORMAT_R16_UNORM;
			numComponents = 1;
			sizeComponent = 2;
			break;
		case ImageFormat::R16G16:
			imageFormat = VK_FORMAT_R16G16_UNORM;
			numComponents = 2;
			sizeComponent = 2;
			break;
		case ImageFormat::R16G16B16:
			imageFormat = VK_FORMAT_R16G16B16_UNORM;
			numComponents = 3;
			sizeComponent = 2;
			break;
		case ImageFormat::R16G16B16A16:
			imageFormat = VK_FORMAT_R16G16B16A16_UNORM;
			numComponents = 4;
			sizeComponent = 2;
			break;
		case ImageFormat::R32:
			imageFormat = VK_FORMAT_R32_SFLOAT;
			numComponents = 1;
			sizeComponent = 4;
			break;
		case ImageFormat::R32G32:
			imageFormat = VK_FORMAT_R32G32_SFLOAT;
			numComponents = 2;
			sizeComponent = 4;
			break;
		case ImageFormat::R32G32B32:
			imageFormat = VK_FORMAT_R32G32B32_SFLOAT;
			numComponents = 3;
			sizeComponent = 4;
			break;
		case ImageFormat::R32G32B32A32:
			imageFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
			numComponents = 4;
			sizeComponent = 4;
			break;
		default:
			NTSHENGN_MODULE_ERROR("Image format unrecognized.", Result::ModuleError);
		}
	}

	// Create texture
	VkImage textureImage;
	VmaAllocation textureImageAllocation;
	VkImageView textureImageView;

	VkImageCreateInfo textureImageCreateInfo = {};
	textureImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	textureImageCreateInfo.pNext = nullptr;
	textureImageCreateInfo.flags = 0;
	textureImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	textureImageCreateInfo.format = imageFormat;
	textureImageCreateInfo.extent.width = image.width;
	textureImageCreateInfo.extent.height = image.height;
	textureImageCreateInfo.extent.depth = 1;
	textureImageCreateInfo.mipLevels = findMipLevels(image.width, image.height);
	textureImageCreateInfo.arrayLayers = 1;
	textureImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	textureImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	textureImageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	textureImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	textureImageCreateInfo.queueFamilyIndexCount = 1;
	textureImageCreateInfo.pQueueFamilyIndices = &m_graphicsQueueFamilyIndex;
	textureImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo textureImageAllocationCreateInfo = {};
	textureImageAllocationCreateInfo.flags = 0;
	textureImageAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	NTSHENGN_VK_CHECK(vmaCreateImage(m_allocator, &textureImageCreateInfo, &textureImageAllocationCreateInfo, &textureImage, &textureImageAllocation, nullptr));

	VkImageViewCreateInfo textureImageViewCreateInfo = {};
	textureImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	textureImageViewCreateInfo.pNext = nullptr;
	textureImageViewCreateInfo.flags = 0;
	textureImageViewCreateInfo.image = textureImage;
	textureImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	textureImageViewCreateInfo.format = imageFormat;
	textureImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
	textureImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
	textureImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
	textureImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
	textureImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	textureImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	textureImageViewCreateInfo.subresourceRange.levelCount = textureImageCreateInfo.mipLevels;
	textureImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	textureImageViewCreateInfo.subresourceRange.layerCount = 1;
	NTSHENGN_VK_CHECK(vkCreateImageView(m_device, &textureImageViewCreateInfo, nullptr, &textureImageView));

	// Create texture staging buffer
	VkBuffer textureStagingBuffer;
	VmaAllocation textureStagingBufferAllocation;

	VkBufferCreateInfo textureStagingBufferCreateInfo = {};
	textureStagingBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	textureStagingBufferCreateInfo.pNext = nullptr;
	textureStagingBufferCreateInfo.flags = 0;
	textureStagingBufferCreateInfo.size = static_cast<size_t>(image.width) * static_cast<size_t>(image.height) * numComponents * sizeComponent;
	textureStagingBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	textureStagingBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	textureStagingBufferCreateInfo.queueFamilyIndexCount = 1;
	textureStagingBufferCreateInfo.pQueueFamilyIndices = &m_graphicsQueueFamilyIndex;

	VmaAllocationCreateInfo textureStagingBufferAllocationCreateInfo = {};
	textureStagingBufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	textureStagingBufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
	NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &textureStagingBufferCreateInfo, &textureStagingBufferAllocationCreateInfo, &textureStagingBuffer, &textureStagingBufferAllocation, nullptr));

	void* data;
	NTSHENGN_VK_CHECK(vmaMapMemory(m_allocator, textureStagingBufferAllocation, &data));
	memcpy(data, image.data.data(), static_cast<size_t>(image.width) * static_cast<size_t>(image.height) * numComponents * sizeComponent);
	vmaUnmapMemory(m_allocator, textureStagingBufferAllocation);

	// Copy staging buffer
	NTSHENGN_VK_CHECK(vkResetCommandPool(m_device, m_initializationCommandPool, 0));

	VkCommandBufferBeginInfo textureCopyBeginInfo = {};
	textureCopyBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	textureCopyBeginInfo.pNext = nullptr;
	textureCopyBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	textureCopyBeginInfo.pInheritanceInfo = nullptr;
	NTSHENGN_VK_CHECK(vkBeginCommandBuffer(m_initializationCommandBuffer, &textureCopyBeginInfo));

	VkImageMemoryBarrier2 undefinedToTransferDstImageMemoryBarrier = {};
	undefinedToTransferDstImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	undefinedToTransferDstImageMemoryBarrier.pNext = nullptr;
	undefinedToTransferDstImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
	undefinedToTransferDstImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_NONE;
	undefinedToTransferDstImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT | VK_PIPELINE_STAGE_2_BLIT_BIT;
	undefinedToTransferDstImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	undefinedToTransferDstImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	undefinedToTransferDstImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	undefinedToTransferDstImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	undefinedToTransferDstImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	undefinedToTransferDstImageMemoryBarrier.image = textureImage;
	undefinedToTransferDstImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	undefinedToTransferDstImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	undefinedToTransferDstImageMemoryBarrier.subresourceRange.levelCount = textureImageCreateInfo.mipLevels;
	undefinedToTransferDstImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	undefinedToTransferDstImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkDependencyInfo undefinedToTransferDstDependencyInfo = {};
	undefinedToTransferDstDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	undefinedToTransferDstDependencyInfo.pNext = nullptr;
	undefinedToTransferDstDependencyInfo.dependencyFlags = 0;
	undefinedToTransferDstDependencyInfo.memoryBarrierCount = 0;
	undefinedToTransferDstDependencyInfo.pMemoryBarriers = nullptr;
	undefinedToTransferDstDependencyInfo.bufferMemoryBarrierCount = 0;
	undefinedToTransferDstDependencyInfo.pBufferMemoryBarriers = nullptr;
	undefinedToTransferDstDependencyInfo.imageMemoryBarrierCount = 1;
	undefinedToTransferDstDependencyInfo.pImageMemoryBarriers = &undefinedToTransferDstImageMemoryBarrier;
	m_vkCmdPipelineBarrier2KHR(m_initializationCommandBuffer, &undefinedToTransferDstDependencyInfo);

	VkBufferImageCopy textureBufferCopy = {};
	textureBufferCopy.bufferOffset = 0;
	textureBufferCopy.bufferRowLength = 0;
	textureBufferCopy.bufferImageHeight = 0;
	textureBufferCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	textureBufferCopy.imageSubresource.mipLevel = 0;
	textureBufferCopy.imageSubresource.baseArrayLayer = 0;
	textureBufferCopy.imageSubresource.layerCount = 1;
	textureBufferCopy.imageOffset.x = 0;
	textureBufferCopy.imageOffset.y = 0;
	textureBufferCopy.imageOffset.z = 0;
	textureBufferCopy.imageExtent.width = image.width;
	textureBufferCopy.imageExtent.height = image.height;
	textureBufferCopy.imageExtent.depth = 1;
	vkCmdCopyBufferToImage(m_initializationCommandBuffer, textureStagingBuffer, textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &textureBufferCopy);

	VkImageMemoryBarrier2 mipmapGenerationImageMemoryBarrier = {};
	mipmapGenerationImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	mipmapGenerationImageMemoryBarrier.pNext = nullptr;
	mipmapGenerationImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	mipmapGenerationImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	mipmapGenerationImageMemoryBarrier.image = textureImage;
	mipmapGenerationImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	mipmapGenerationImageMemoryBarrier.subresourceRange.levelCount = 1;
	mipmapGenerationImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	mipmapGenerationImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkDependencyInfo mipmapGenerationDependencyInfo = {};
	mipmapGenerationDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	mipmapGenerationDependencyInfo.pNext = nullptr;
	mipmapGenerationDependencyInfo.dependencyFlags = 0;
	mipmapGenerationDependencyInfo.memoryBarrierCount = 0;
	mipmapGenerationDependencyInfo.pMemoryBarriers = nullptr;
	mipmapGenerationDependencyInfo.bufferMemoryBarrierCount = 0;
	mipmapGenerationDependencyInfo.pBufferMemoryBarriers = nullptr;
	mipmapGenerationDependencyInfo.imageMemoryBarrierCount = 1;
	mipmapGenerationDependencyInfo.pImageMemoryBarriers = &mipmapGenerationImageMemoryBarrier;

	uint32_t mipWidth = image.width;
	uint32_t mipHeight = image.height;
	for (uint32_t i = 1; i < textureImageCreateInfo.mipLevels; i++) {
		mipmapGenerationImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT | VK_PIPELINE_STAGE_2_BLIT_BIT;
		mipmapGenerationImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		mipmapGenerationImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;
		mipmapGenerationImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
		mipmapGenerationImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		mipmapGenerationImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		mipmapGenerationImageMemoryBarrier.subresourceRange.baseMipLevel = i - 1;

		m_vkCmdPipelineBarrier2KHR(m_initializationCommandBuffer, &mipmapGenerationDependencyInfo);

		VkImageBlit imageBlit = {};
		imageBlit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBlit.srcSubresource.mipLevel = i - 1;
		imageBlit.srcSubresource.baseArrayLayer = 0;
		imageBlit.srcSubresource.layerCount = 1;
		imageBlit.srcOffsets[0].x = 0;
		imageBlit.srcOffsets[0].y = 0;
		imageBlit.srcOffsets[0].z = 0;
		imageBlit.srcOffsets[1].x = mipWidth;
		imageBlit.srcOffsets[1].y = mipHeight;
		imageBlit.srcOffsets[1].z = 1;
		imageBlit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		imageBlit.dstSubresource.mipLevel = i;
		imageBlit.dstSubresource.baseArrayLayer = 0;
		imageBlit.dstSubresource.layerCount = 1;
		imageBlit.dstOffsets[0].x = 0;
		imageBlit.dstOffsets[0].y = 0;
		imageBlit.dstOffsets[0].z = 0;
		imageBlit.dstOffsets[1].x = mipWidth > 1 ? mipWidth / 2 : 1;
		imageBlit.dstOffsets[1].y = mipHeight > 1 ? mipHeight / 2 : 1;
		imageBlit.dstOffsets[1].z = 1;
		vkCmdBlitImage(m_initializationCommandBuffer, textureImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageBlit, VK_FILTER_LINEAR);

		mipmapGenerationImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;
		mipmapGenerationImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
		mipmapGenerationImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		mipmapGenerationImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
		mipmapGenerationImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		mipmapGenerationImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		m_vkCmdPipelineBarrier2KHR(m_initializationCommandBuffer, &mipmapGenerationDependencyInfo);

		mipWidth = mipWidth > 1 ? mipWidth / 2 : 1;
		mipHeight = mipHeight > 1 ? mipHeight / 2 : 1;
	}

	if (textureImageCreateInfo.mipLevels == 1) {
		mipmapGenerationImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
	}
	else {
		mipmapGenerationImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_BLIT_BIT;
	}
	mipmapGenerationImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	mipmapGenerationImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	mipmapGenerationImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	mipmapGenerationImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	mipmapGenerationImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	mipmapGenerationImageMemoryBarrier.subresourceRange.baseMipLevel = textureImageCreateInfo.mipLevels - 1;

	m_vkCmdPipelineBarrier2KHR(m_initializationCommandBuffer, &mipmapGenerationDependencyInfo);

	NTSHENGN_VK_CHECK(vkEndCommandBuffer(m_initializationCommandBuffer));

	VkSubmitInfo buffersCopySubmitInfo = {};
	buffersCopySubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	buffersCopySubmitInfo.pNext = nullptr;
	buffersCopySubmitInfo.waitSemaphoreCount = 0;
	buffersCopySubmitInfo.pWaitSemaphores = nullptr;
	buffersCopySubmitInfo.pWaitDstStageMask = nullptr;
	buffersCopySubmitInfo.commandBufferCount = 1;
	buffersCopySubmitInfo.pCommandBuffers = &m_initializationCommandBuffer;
	buffersCopySubmitInfo.signalSemaphoreCount = 0;
	buffersCopySubmitInfo.pSignalSemaphores = nullptr;
	NTSHENGN_VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &buffersCopySubmitInfo, m_initializationFence));
	NTSHENGN_VK_CHECK(vkWaitForFences(m_device, 1, &m_initializationFence, VK_TRUE, std::numeric_limits<uint64_t>::max()));
	NTSHENGN_VK_CHECK(vkResetFences(m_device, 1, &m_initializationFence));

	vmaDestroyBuffer(m_allocator, textureStagingBuffer, textureStagingBufferAllocation);

	m_textureImages.push_back(textureImage);
	m_textureImageAllocations.push_back(textureImageAllocation);
	m_textureImageViews.push_back(textureImageView);
	m_textureSizes.push_back({ static_cast<float>(image.width), static_cast<float>(image.height) });

	// Mark descriptor sets for update
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		m_descriptorSetsNeedUpdate[i] = true;
	}

	return static_cast<ImageID>(m_textureImages.size() - 1);
}

NtshEngn::FontID NtshEngn::GraphicsModule::load(const Font& font) {
	if (m_fontAddresses.find(&font) != m_fontAddresses.end()) {
		return m_fontAddresses[&font];
	}

	m_fontAddresses[&font] = static_cast<FontID>(m_fonts.size());

	// Create texture
	VkImage textureImage;
	VmaAllocation textureImageAllocation;
	VkImageView textureImageView;

	VkImageCreateInfo textureImageCreateInfo = {};
	textureImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	textureImageCreateInfo.pNext = nullptr;
	textureImageCreateInfo.flags = 0;
	textureImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	textureImageCreateInfo.format = VK_FORMAT_R8_UNORM;
	textureImageCreateInfo.extent.width = font.image->width;
	textureImageCreateInfo.extent.height = font.image->height;
	textureImageCreateInfo.extent.depth = 1;
	textureImageCreateInfo.mipLevels = 1;
	textureImageCreateInfo.arrayLayers = 1;
	textureImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	textureImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	textureImageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
	textureImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	textureImageCreateInfo.queueFamilyIndexCount = 1;
	textureImageCreateInfo.pQueueFamilyIndices = &m_graphicsQueueFamilyIndex;
	textureImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo textureImageAllocationCreateInfo = {};
	textureImageAllocationCreateInfo.flags = 0;
	textureImageAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	NTSHENGN_VK_CHECK(vmaCreateImage(m_allocator, &textureImageCreateInfo, &textureImageAllocationCreateInfo, &textureImage, &textureImageAllocation, nullptr));

	VkImageViewCreateInfo textureImageViewCreateInfo = {};
	textureImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	textureImageViewCreateInfo.pNext = nullptr;
	textureImageViewCreateInfo.flags = 0;
	textureImageViewCreateInfo.image = textureImage;
	textureImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	textureImageViewCreateInfo.format = VK_FORMAT_R8_UNORM;
	textureImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
	textureImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
	textureImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
	textureImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
	textureImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	textureImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	textureImageViewCreateInfo.subresourceRange.levelCount = 1;
	textureImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	textureImageViewCreateInfo.subresourceRange.layerCount = 1;
	NTSHENGN_VK_CHECK(vkCreateImageView(m_device, &textureImageViewCreateInfo, nullptr, &textureImageView));

	// Create texture staging buffer
	VkBuffer textureStagingBuffer;
	VmaAllocation textureStagingBufferAllocation;

	VkBufferCreateInfo textureStagingBufferCreateInfo = {};
	textureStagingBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	textureStagingBufferCreateInfo.pNext = nullptr;
	textureStagingBufferCreateInfo.flags = 0;
	textureStagingBufferCreateInfo.size = static_cast<size_t>(font.image->width) * static_cast<size_t>(font.image->height);
	textureStagingBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	textureStagingBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	textureStagingBufferCreateInfo.queueFamilyIndexCount = 1;
	textureStagingBufferCreateInfo.pQueueFamilyIndices = &m_graphicsQueueFamilyIndex;

	VmaAllocationCreateInfo textureStagingBufferAllocationCreateInfo = {};
	textureStagingBufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	textureStagingBufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
	NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &textureStagingBufferCreateInfo, &textureStagingBufferAllocationCreateInfo, &textureStagingBuffer, &textureStagingBufferAllocation, nullptr));

	void* data;
	NTSHENGN_VK_CHECK(vmaMapMemory(m_allocator, textureStagingBufferAllocation, &data));
	memcpy(data, font.image->data.data(), static_cast<size_t>(font.image->width) * static_cast<size_t>(font.image->height));
	vmaUnmapMemory(m_allocator, textureStagingBufferAllocation);

	// Copy staging buffer
	NTSHENGN_VK_CHECK(vkResetCommandPool(m_device, m_initializationCommandPool, 0));

	VkCommandBufferBeginInfo textureCopyBeginInfo = {};
	textureCopyBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	textureCopyBeginInfo.pNext = nullptr;
	textureCopyBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	textureCopyBeginInfo.pInheritanceInfo = nullptr;
	NTSHENGN_VK_CHECK(vkBeginCommandBuffer(m_initializationCommandBuffer, &textureCopyBeginInfo));

	VkImageMemoryBarrier2 undefinedToTransferDstImageMemoryBarrier = {};
	undefinedToTransferDstImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	undefinedToTransferDstImageMemoryBarrier.pNext = nullptr;
	undefinedToTransferDstImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
	undefinedToTransferDstImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_NONE;
	undefinedToTransferDstImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
	undefinedToTransferDstImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	undefinedToTransferDstImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	undefinedToTransferDstImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	undefinedToTransferDstImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	undefinedToTransferDstImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	undefinedToTransferDstImageMemoryBarrier.image = textureImage;
	undefinedToTransferDstImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	undefinedToTransferDstImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	undefinedToTransferDstImageMemoryBarrier.subresourceRange.levelCount = textureImageCreateInfo.mipLevels;
	undefinedToTransferDstImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	undefinedToTransferDstImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkDependencyInfo undefinedToTransferDstDependencyInfo = {};
	undefinedToTransferDstDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	undefinedToTransferDstDependencyInfo.pNext = nullptr;
	undefinedToTransferDstDependencyInfo.dependencyFlags = 0;
	undefinedToTransferDstDependencyInfo.memoryBarrierCount = 0;
	undefinedToTransferDstDependencyInfo.pMemoryBarriers = nullptr;
	undefinedToTransferDstDependencyInfo.bufferMemoryBarrierCount = 0;
	undefinedToTransferDstDependencyInfo.pBufferMemoryBarriers = nullptr;
	undefinedToTransferDstDependencyInfo.imageMemoryBarrierCount = 1;
	undefinedToTransferDstDependencyInfo.pImageMemoryBarriers = &undefinedToTransferDstImageMemoryBarrier;
	m_vkCmdPipelineBarrier2KHR(m_initializationCommandBuffer, &undefinedToTransferDstDependencyInfo);

	VkBufferImageCopy textureBufferCopy = {};
	textureBufferCopy.bufferOffset = 0;
	textureBufferCopy.bufferRowLength = 0;
	textureBufferCopy.bufferImageHeight = 0;
	textureBufferCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	textureBufferCopy.imageSubresource.mipLevel = 0;
	textureBufferCopy.imageSubresource.baseArrayLayer = 0;
	textureBufferCopy.imageSubresource.layerCount = 1;
	textureBufferCopy.imageOffset.x = 0;
	textureBufferCopy.imageOffset.y = 0;
	textureBufferCopy.imageOffset.z = 0;
	textureBufferCopy.imageExtent.width = font.image->width;
	textureBufferCopy.imageExtent.height = font.image->height;
	textureBufferCopy.imageExtent.depth = 1;
	vkCmdCopyBufferToImage(m_initializationCommandBuffer, textureStagingBuffer, textureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &textureBufferCopy);

	VkImageMemoryBarrier2 transferDstToShaderReadOnlyImageMemoryBarrier = {};
	transferDstToShaderReadOnlyImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	transferDstToShaderReadOnlyImageMemoryBarrier.pNext = nullptr;
	transferDstToShaderReadOnlyImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
	transferDstToShaderReadOnlyImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	transferDstToShaderReadOnlyImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	transferDstToShaderReadOnlyImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	transferDstToShaderReadOnlyImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	transferDstToShaderReadOnlyImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	transferDstToShaderReadOnlyImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	transferDstToShaderReadOnlyImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	transferDstToShaderReadOnlyImageMemoryBarrier.image = textureImage;
	transferDstToShaderReadOnlyImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	transferDstToShaderReadOnlyImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	transferDstToShaderReadOnlyImageMemoryBarrier.subresourceRange.levelCount = textureImageCreateInfo.mipLevels;
	transferDstToShaderReadOnlyImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	transferDstToShaderReadOnlyImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkDependencyInfo transferDstToShaderReadOnlyDependencyInfo = {};
	transferDstToShaderReadOnlyDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	transferDstToShaderReadOnlyDependencyInfo.pNext = nullptr;
	transferDstToShaderReadOnlyDependencyInfo.dependencyFlags = 0;
	transferDstToShaderReadOnlyDependencyInfo.memoryBarrierCount = 0;
	transferDstToShaderReadOnlyDependencyInfo.pMemoryBarriers = nullptr;
	transferDstToShaderReadOnlyDependencyInfo.bufferMemoryBarrierCount = 0;
	transferDstToShaderReadOnlyDependencyInfo.pBufferMemoryBarriers = nullptr;
	transferDstToShaderReadOnlyDependencyInfo.imageMemoryBarrierCount = 1;
	transferDstToShaderReadOnlyDependencyInfo.pImageMemoryBarriers = &transferDstToShaderReadOnlyImageMemoryBarrier;
	m_vkCmdPipelineBarrier2KHR(m_initializationCommandBuffer, &transferDstToShaderReadOnlyDependencyInfo);

	NTSHENGN_VK_CHECK(vkEndCommandBuffer(m_initializationCommandBuffer));

	VkSubmitInfo buffersCopySubmitInfo = {};
	buffersCopySubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	buffersCopySubmitInfo.pNext = nullptr;
	buffersCopySubmitInfo.waitSemaphoreCount = 0;
	buffersCopySubmitInfo.pWaitSemaphores = nullptr;
	buffersCopySubmitInfo.pWaitDstStageMask = nullptr;
	buffersCopySubmitInfo.commandBufferCount = 1;
	buffersCopySubmitInfo.pCommandBuffers = &m_initializationCommandBuffer;
	buffersCopySubmitInfo.signalSemaphoreCount = 0;
	buffersCopySubmitInfo.pSignalSemaphores = nullptr;
	NTSHENGN_VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &buffersCopySubmitInfo, m_initializationFence));
	NTSHENGN_VK_CHECK(vkWaitForFences(m_device, 1, &m_initializationFence, VK_TRUE, std::numeric_limits<uint64_t>::max()));
	NTSHENGN_VK_CHECK(vkResetFences(m_device, 1, &m_initializationFence));

	vmaDestroyBuffer(m_allocator, textureStagingBuffer, textureStagingBufferAllocation);

	m_fonts.push_back({ textureImage, textureImageAllocation, textureImageView, font.imageSamplerFilter, font.glyphs });

	// Mark descriptor sets for update
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		m_uiTextDescriptorSetsNeedUpdate[i] = true;
	}

	return static_cast<FontID>(m_fonts.size() - 1);
}

void NtshEngn::GraphicsModule::setBackgroundColor(const Math::vec4& backgroundColor) {
	m_backgroundColor = backgroundColor;
	m_sampleBatch = 0;
}

void NtshEngn::GraphicsModule::playAnimation(Entity entity, uint32_t animationIndex) {
	NTSHENGN_UNUSED(entity);
	NTSHENGN_UNUSED(animationIndex);
	NTSHENGN_MODULE_FUNCTION_NOT_IMPLEMENTED();
}

void NtshEngn::GraphicsModule::pauseAnimation(Entity entity) {
	NTSHENGN_UNUSED(entity);
	NTSHENGN_MODULE_FUNCTION_NOT_IMPLEMENTED();
}

void NtshEngn::GraphicsModule::stopAnimation(Entity entity) {
	NTSHENGN_UNUSED(entity);
	NTSHENGN_MODULE_FUNCTION_NOT_IMPLEMENTED();
}

bool NtshEngn::GraphicsModule::isAnimationPlaying(Entity entity, uint32_t animationIndex) {
	NTSHENGN_UNUSED(entity);
	NTSHENGN_UNUSED(animationIndex);
	NTSHENGN_MODULE_FUNCTION_NOT_IMPLEMENTED();

	return false;
}

void NtshEngn::GraphicsModule::drawUIText(FontID fontID, const std::string& text, const Math::vec2& position, const Math::vec4& color) {
	NTSHENGN_ASSERT(fontID < m_fonts.size());

	float positionAdvance = 0.0f;
	std::vector<Math::vec2> positionsAndUVs;
	for (const char& c : text) {
		const Math::vec2 topLeft = position + m_fonts[fontID].glyphs[c].positionTopLeft + Math::vec2(positionAdvance, 0.0f);
		const Math::vec2 bottomRight = position + m_fonts[fontID].glyphs[c].positionBottomRight + Math::vec2(positionAdvance, 0.0f);
		positionsAndUVs.push_back(Math::vec2((topLeft.x / m_viewport.width) * 2.0f - 1.0f, (topLeft.y / m_viewport.height) * 2.0f - 1.0f));
		positionsAndUVs.push_back(Math::vec2((bottomRight.x / m_viewport.width) * 2.0f - 1.0f, (bottomRight.y / m_viewport.height) * 2.0f - 1.0f));
		positionsAndUVs.push_back(m_fonts[fontID].glyphs[c].uvTopLeft);
		positionsAndUVs.push_back(m_fonts[fontID].glyphs[c].uvBottomRight);

		positionAdvance += m_fonts[fontID].glyphs[c].positionAdvance;
	}

	size_t offset = m_uiTextBufferOffset * sizeof(Math::vec2) * 4;
	memcpy(reinterpret_cast<uint8_t*>(m_uiTextBuffers[m_currentFrameInFlight].address) + offset, positionsAndUVs.data(), sizeof(Math::vec2) * 4 * text.size());

	InternalUIText uiText;
	uiText.fontID = fontID;
	uiText.color = color;
	uiText.charactersCount = static_cast<uint32_t>(text.size());
	uiText.bufferOffset = m_uiTextBufferOffset;
	m_uiTexts.push(uiText);

	m_uiTextBufferOffset += static_cast<uint32_t>(uiText.charactersCount);

	m_uiElements.push(UIElement::Text);
}

void NtshEngn::GraphicsModule::drawUILine(const Math::vec2& start, const Math::vec2& end, const Math::vec4& color) {
	InternalUILine uiLine;
	uiLine.positions = Math::vec4((start.x / m_viewport.width) * 2.0f - 1.0f, (start.y / m_viewport.height) * 2.0f - 1.0f, (end.x / m_viewport.width) * 2.0f - 1.0f, (end.y / m_viewport.height) * 2.0f - 1.0f);
	uiLine.color = color;
	m_uiLines.push(uiLine);

	m_uiElements.push(UIElement::Line);
}

void NtshEngn::GraphicsModule::drawUIRectangle(const Math::vec2& position, const Math::vec2& size, const Math::vec4& color) {
	InternalUIRectangle uiRectangle;
	const Math::vec2 topLeft = Math::vec2((position.x / m_viewport.width) * 2.0f - 1.0f, (position.y / m_viewport.height) * 2.0f - 1.0f);
	const Math::vec2 bottomRight = Math::vec2(((position.x + size.x) / m_viewport.width) * 2.0f - 1.0f, ((position.y + size.y) / m_viewport.height) * 2.0f - 1.0f);
	uiRectangle.positions = Math::vec4(topLeft, bottomRight);
	uiRectangle.color = color;
	m_uiRectangles.push(uiRectangle);

	m_uiElements.push(UIElement::Rectangle);
}

void NtshEngn::GraphicsModule::drawUIImage(ImageID imageID, ImageSamplerFilter imageSamplerFilter, const Math::vec2& position, float rotation, const Math::vec2& scale, const Math::vec4& color) {
	NTSHENGN_ASSERT(imageID < m_textureImages.size());

	const Math::mat3 transform = Math::translate(position) * Math::rotate(rotation) * Math::scale(Math::vec2(std::abs(scale.x), std::abs(scale.y)));
	const float x = (m_textureSizes[imageID].x) / 2.0f;
	const float y = (m_textureSizes[imageID].y) / 2.0f;

	InternalUIImage uiImage;
	bool foundUITexture = false;
	for (size_t i = 0; i < m_uiTextures.size(); i++) {
		if ((m_uiTextures[i].first == imageID) &&
			(m_uiTextures[i].second == imageSamplerFilter)) {
			uiImage.uiTextureIndex = static_cast<uint32_t>(i);

			foundUITexture = true;
			break;
		}
	}
	if (!foundUITexture) {
		m_uiTextures.push_back({ imageID, imageSamplerFilter });

		uiImage.uiTextureIndex = static_cast<uint32_t>(m_uiTextures.size() - 1);

		for (uint32_t i = 0; i < m_framesInFlight; i++) {
			m_uiImageDescriptorSetsNeedUpdate[i] = true;
		}
	}

	const Math::vec2 v0 = Math::vec2(transform * Math::vec3(x, -y, 1.0f));
	const Math::vec2 v1 = Math::vec2(transform * Math::vec3(-x, -y, 1.0f));
	const Math::vec2 v2 = Math::vec2(transform * Math::vec3(-x, y, 1.0f));
	const Math::vec2 v3 = Math::vec2(transform * Math::vec3(x, y, 1.0f));

	uiImage.v0 = Math::vec2((v0.x / m_viewport.width) * 2.0f - 1.0f, (v0.y / m_viewport.height) * 2.0f - 1.0f);
	uiImage.v1 = Math::vec2((v1.x / m_viewport.width) * 2.0f - 1.0f, (v1.y / m_viewport.height) * 2.0f - 1.0f);
	uiImage.v2 = Math::vec2((v2.x / m_viewport.width) * 2.0f - 1.0f, (v2.y / m_viewport.height) * 2.0f - 1.0f);
	uiImage.v3 = Math::vec2((v3.x / m_viewport.width) * 2.0f - 1.0f, (v3.y / m_viewport.height) * 2.0f - 1.0f);
	uiImage.reverseUV.x = (scale.x >= 0.0f) ? 0.0f : 1.0f;
	uiImage.reverseUV.y = (scale.y >= 0.0f) ? 0.0f : 1.0f;
	uiImage.color = color;
	m_uiImages.push(uiImage);

	m_uiElements.push(UIElement::Image);
}

const NtshEngn::ComponentMask NtshEngn::GraphicsModule::getComponentMask() const {
	ComponentMask componentMask;
	componentMask.set(ecs->getComponentID<Renderable>());
	componentMask.set(ecs->getComponentID<Camera>());
	componentMask.set(ecs->getComponentID<Light>());

	return componentMask;
}

void NtshEngn::GraphicsModule::onEntityComponentAdded(Entity entity, Component componentID) {
	if (componentID == ecs->getComponentID<Renderable>()) {
		InternalObject object;
		object.index = attributeObjectIndex();
		m_objects[entity] = object;

		PreviousObject previousObject;
		previousObject.transform = ecs->getComponent<Transform>(entity);
		previousObject.meshID = object.meshID;
		previousObject.materialIndex = object.materialIndex;
		m_previousObjects[entity] = previousObject;

		m_sampleBatch = 0;

		m_lastKnownMaterial[entity] = Material();
		loadRenderableForEntity(entity);
	}
	else if (componentID == ecs->getComponentID<Camera>()) {
		if (m_mainCamera == NTSHENGN_ENTITY_UNKNOWN) {
			m_mainCamera = entity;
			
			PreviousCamera previousCamera;
			previousCamera.transform = ecs->getComponent<Transform>(entity);
			previousCamera.camera = ecs->getComponent<Camera>(entity);
			m_previousCamera = previousCamera;

			m_sampleBatch = 0;
		}
	}
	else if (componentID == ecs->getComponentID<Light>()) {
		const Light& light = ecs->getComponent<Light>(entity);

		PreviousLight previousLight;
		previousLight.transform = ecs->getComponent<Transform>(entity);
		previousLight.light = light;

		switch (light.type) {
		case LightType::Directional:
			m_lights.directionalLights.insert(entity);
			m_previousDirectionalLights[entity] = previousLight;
			break;
			
		case LightType::Point:
			m_lights.pointLights.insert(entity);
			m_previousPointLights[entity] = previousLight;
			break;

		case LightType::Spot:
			m_lights.spotLights.insert(entity);
			m_previousSpotLights[entity] = previousLight;
			break;

		case LightType::Ambient:
			m_lights.ambientLights.insert(entity);
			m_previousAmbientLights[entity] = previousLight;
			break;

		default: // Arbitrarily consider it a directional light
			m_lights.directionalLights.insert(entity);
			m_previousDirectionalLights[entity] = previousLight;
			break;
		}

		m_sampleBatch = 0;
	}
}

void NtshEngn::GraphicsModule::onEntityComponentRemoved(Entity entity, Component componentID) {
	if (componentID == ecs->getComponentID<Renderable>()) {
		const InternalObject& object = m_objects[entity];

		retrieveObjectIndex(object.index);

		m_lastKnownMaterial.erase(entity);

		m_objects.erase(entity);

		m_previousObjects.erase(entity);

		m_sampleBatch = 0;
	}
	else if (componentID == ecs->getComponentID<Camera>()) {
		if (m_mainCamera == entity) {
			m_mainCamera = NTSHENGN_ENTITY_UNKNOWN;

			m_sampleBatch = 0;
		}
	}
	else if (componentID == ecs->getComponentID<Light>()) {
		const Light& light = ecs->getComponent<Light>(entity);

		switch (light.type) {
		case LightType::Directional:
			m_lights.directionalLights.erase(entity);
			m_previousDirectionalLights.erase(entity);
			break;

		case LightType::Point:
			m_lights.pointLights.erase(entity);
			m_previousPointLights.erase(entity);
			break;

		case LightType::Spot:
			m_lights.spotLights.erase(entity);
			m_previousSpotLights.erase(entity);
			break;

		case LightType::Ambient:
			m_lights.ambientLights.erase(entity);
			m_previousAmbientLights.erase(entity);
			break;

		default: // Arbitrarily consider it a directional light
			m_lights.directionalLights.erase(entity);
			m_previousDirectionalLights.erase(entity);
			break;
		}

		m_sampleBatch = 0;
	}
}

VkSurfaceCapabilitiesKHR NtshEngn::GraphicsModule::getSurfaceCapabilities() {
	VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo = {};
	surfaceInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR;
	surfaceInfo.pNext = nullptr;
	surfaceInfo.surface = m_surface;

	VkSurfaceCapabilities2KHR surfaceCapabilities;
	surfaceCapabilities.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR;
	surfaceCapabilities.pNext = nullptr;
	NTSHENGN_VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilities2KHR(m_physicalDevice, &surfaceInfo, &surfaceCapabilities));

	return surfaceCapabilities.surfaceCapabilities;
}

std::vector<VkSurfaceFormatKHR> NtshEngn::GraphicsModule::getSurfaceFormats() {
	VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo = {};
	surfaceInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR;
	surfaceInfo.pNext = nullptr;
	surfaceInfo.surface = m_surface;

	uint32_t surfaceFormatsCount;
	NTSHENGN_VK_CHECK(vkGetPhysicalDeviceSurfaceFormats2KHR(m_physicalDevice, &surfaceInfo, &surfaceFormatsCount, nullptr));
	std::vector<VkSurfaceFormat2KHR> surfaceFormats2(surfaceFormatsCount);
	for (VkSurfaceFormat2KHR& surfaceFormat2 : surfaceFormats2) {
		surfaceFormat2.sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR;
		surfaceFormat2.pNext = nullptr;
	}
	NTSHENGN_VK_CHECK(vkGetPhysicalDeviceSurfaceFormats2KHR(m_physicalDevice, &surfaceInfo, &surfaceFormatsCount, surfaceFormats2.data()));
	
	std::vector<VkSurfaceFormatKHR> surfaceFormats;
	for (const VkSurfaceFormat2KHR surfaceFormat2 : surfaceFormats2) {
		surfaceFormats.push_back(surfaceFormat2.surfaceFormat);
	}

	return surfaceFormats;
}

std::vector<VkPresentModeKHR> NtshEngn::GraphicsModule::getSurfacePresentModes() {
	uint32_t presentModesCount;
	NTSHENGN_VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModesCount, nullptr));
	std::vector<VkPresentModeKHR> presentModes(presentModesCount);
	NTSHENGN_VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModesCount, presentModes.data()));

	return presentModes;
}

VkPhysicalDeviceMemoryProperties NtshEngn::GraphicsModule::getMemoryProperties() {
	VkPhysicalDeviceMemoryProperties2 memoryProperties = {};
	memoryProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
	memoryProperties.pNext = nullptr;
	vkGetPhysicalDeviceMemoryProperties2(m_physicalDevice, &memoryProperties);

	return memoryProperties.memoryProperties;
}

uint32_t NtshEngn::GraphicsModule::findMipLevels(uint32_t width, uint32_t height) {
	return static_cast<uint32_t>(std::floor(std::log2(std::min(width, height)) + 1));
}

void NtshEngn::GraphicsModule::createSwapchain(VkSwapchainKHR oldSwapchain) {
	VkSurfaceCapabilitiesKHR surfaceCapabilities = getSurfaceCapabilities();
	uint32_t minImageCount = surfaceCapabilities.minImageCount + 1;
	if (surfaceCapabilities.maxImageCount > 0 && minImageCount > surfaceCapabilities.maxImageCount) {
		minImageCount = surfaceCapabilities.maxImageCount;
	}

	std::vector<VkSurfaceFormatKHR> surfaceFormats = getSurfaceFormats();
	m_drawImageFormat = surfaceFormats[0].format;
	VkColorSpaceKHR swapchainColorSpace = surfaceFormats[0].colorSpace;
	for (const VkSurfaceFormatKHR& surfaceFormat : surfaceFormats) {
		if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB && surfaceFormat.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR) {
			m_drawImageFormat = surfaceFormat.format;
			swapchainColorSpace = surfaceFormat.colorSpace;
			break;
		}
	}

	std::vector<VkPresentModeKHR> presentModes = getSurfacePresentModes();
	VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
	for (const VkPresentModeKHR& presentMode : presentModes) {
		if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
			swapchainPresentMode = presentMode;
			break;
		}
		else if (presentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
			swapchainPresentMode = presentMode;
		}
	}

	VkExtent2D swapchainExtent = {};
	swapchainExtent.width = static_cast<uint32_t>(windowModule->getWindowWidth(windowModule->getMainWindowID()));
	swapchainExtent.height = static_cast<uint32_t>(windowModule->getWindowHeight(windowModule->getMainWindowID()));

	m_viewport.x = 0.0f;
	m_viewport.y = 0.0f;
	m_viewport.width = static_cast<float>(swapchainExtent.width);
	m_viewport.height = static_cast<float>(swapchainExtent.height);
	m_viewport.minDepth = 0.0f;
	m_viewport.maxDepth = 1.0f;

	m_scissor.offset.x = 0;
	m_scissor.offset.y = 0;
	m_scissor.extent.width = swapchainExtent.width;
	m_scissor.extent.height = swapchainExtent.height;

	VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
	swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCreateInfo.pNext = nullptr;
	swapchainCreateInfo.flags = 0;
	swapchainCreateInfo.surface = m_surface;
	swapchainCreateInfo.minImageCount = minImageCount;
	swapchainCreateInfo.imageFormat = m_drawImageFormat;
	swapchainCreateInfo.imageColorSpace = swapchainColorSpace;
	swapchainCreateInfo.imageExtent = swapchainExtent;
	swapchainCreateInfo.imageArrayLayers = 1;
	swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchainCreateInfo.queueFamilyIndexCount = 0;
	swapchainCreateInfo.pQueueFamilyIndices = nullptr;
	swapchainCreateInfo.preTransform = surfaceCapabilities.currentTransform;
	swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainCreateInfo.presentMode = swapchainPresentMode;
	swapchainCreateInfo.clipped = VK_TRUE;
	swapchainCreateInfo.oldSwapchain = oldSwapchain;
	NTSHENGN_VK_CHECK(vkCreateSwapchainKHR(m_device, &swapchainCreateInfo, nullptr, &m_swapchain));

	if (oldSwapchain != VK_NULL_HANDLE) {
		vkDestroySwapchainKHR(m_device, oldSwapchain, nullptr);
	}

	NTSHENGN_VK_CHECK(vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_imageCount, nullptr));
	m_swapchainImages.resize(m_imageCount);
	NTSHENGN_VK_CHECK(vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_imageCount, m_swapchainImages.data()));

	// Create the swapchain image views
	m_swapchainImageViews.resize(m_imageCount);
	for (uint32_t i = 0; i < m_imageCount; i++) {
		VkImageViewCreateInfo swapchainImageViewCreateInfo = {};
		swapchainImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		swapchainImageViewCreateInfo.pNext = nullptr;
		swapchainImageViewCreateInfo.flags = 0;
		swapchainImageViewCreateInfo.image = m_swapchainImages[i];
		swapchainImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		swapchainImageViewCreateInfo.format = m_drawImageFormat;
		swapchainImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
		swapchainImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
		swapchainImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
		swapchainImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
		swapchainImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		swapchainImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
		swapchainImageViewCreateInfo.subresourceRange.levelCount = 1;
		swapchainImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
		swapchainImageViewCreateInfo.subresourceRange.layerCount = 1;
		NTSHENGN_VK_CHECK(vkCreateImageView(m_device, &swapchainImageViewCreateInfo, nullptr, &m_swapchainImageViews[i]));
	}
}

void NtshEngn::GraphicsModule::createVertexIndexAndAccelerationStructureBuffers() {
	VkBufferCreateInfo vertexIndexAndAccelerationStructureBufferCreateInfo = {};
	vertexIndexAndAccelerationStructureBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	vertexIndexAndAccelerationStructureBufferCreateInfo.pNext = nullptr;
	vertexIndexAndAccelerationStructureBufferCreateInfo.flags = 0;
	vertexIndexAndAccelerationStructureBufferCreateInfo.size = 67108864;
	vertexIndexAndAccelerationStructureBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	vertexIndexAndAccelerationStructureBufferCreateInfo.queueFamilyIndexCount = 1;
	vertexIndexAndAccelerationStructureBufferCreateInfo.pQueueFamilyIndices = &m_graphicsQueueFamilyIndex;

	VmaAllocationCreateInfo vertexIndexAndAccelerationStructureBufferAllocationCreateInfo = {};

	vertexIndexAndAccelerationStructureBufferCreateInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR;
	vertexIndexAndAccelerationStructureBufferAllocationCreateInfo.flags = 0;
	vertexIndexAndAccelerationStructureBufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &vertexIndexAndAccelerationStructureBufferCreateInfo, &vertexIndexAndAccelerationStructureBufferAllocationCreateInfo, &m_vertexBuffer, &m_vertexBufferAllocation, nullptr));

	vertexIndexAndAccelerationStructureBufferCreateInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR;
	vertexIndexAndAccelerationStructureBufferAllocationCreateInfo.flags = 0;
	vertexIndexAndAccelerationStructureBufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &vertexIndexAndAccelerationStructureBufferCreateInfo, &vertexIndexAndAccelerationStructureBufferAllocationCreateInfo, &m_indexBuffer, &m_indexBufferAllocation, nullptr));

	vertexIndexAndAccelerationStructureBufferCreateInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR;
	vertexIndexAndAccelerationStructureBufferAllocationCreateInfo.flags = 0;
	vertexIndexAndAccelerationStructureBufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &vertexIndexAndAccelerationStructureBufferCreateInfo, &vertexIndexAndAccelerationStructureBufferAllocationCreateInfo, &m_bottomLevelAccelerationStructureBuffer, &m_bottomLevelAccelerationStructureBufferAllocation, nullptr));

	vertexIndexAndAccelerationStructureBufferCreateInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR;
	vertexIndexAndAccelerationStructureBufferAllocationCreateInfo.flags = 0;
	vertexIndexAndAccelerationStructureBufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
	NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &vertexIndexAndAccelerationStructureBufferCreateInfo, &vertexIndexAndAccelerationStructureBufferAllocationCreateInfo, &m_topLevelAccelerationStructureInstancesBuffer, &m_topLevelAccelerationStructureInstancesBufferAllocation, nullptr));

	VmaAllocationInfo bufferAllocationInfo;

	vertexIndexAndAccelerationStructureBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	vertexIndexAndAccelerationStructureBufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
	vertexIndexAndAccelerationStructureBufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
	m_topLevelAccelerationStructureInstancesStagingBuffers.resize(m_framesInFlight);
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &vertexIndexAndAccelerationStructureBufferCreateInfo, &vertexIndexAndAccelerationStructureBufferAllocationCreateInfo, &m_topLevelAccelerationStructureInstancesStagingBuffers[i].handle, &m_topLevelAccelerationStructureInstancesStagingBuffers[i].allocation, &bufferAllocationInfo));
		m_topLevelAccelerationStructureInstancesStagingBuffers[i].address = bufferAllocationInfo.pMappedData;
	}

	VkBufferDeviceAddressInfoKHR vertexIndexAndAccelerationStructureBufferDeviceAddressInfo = {};
	vertexIndexAndAccelerationStructureBufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR;
	vertexIndexAndAccelerationStructureBufferDeviceAddressInfo.pNext = nullptr;
	vertexIndexAndAccelerationStructureBufferDeviceAddressInfo.buffer = m_vertexBuffer;
	m_vertexBufferDeviceAddress = m_vkGetBufferDeviceAddressKHR(m_device, &vertexIndexAndAccelerationStructureBufferDeviceAddressInfo);

	vertexIndexAndAccelerationStructureBufferDeviceAddressInfo.buffer = m_indexBuffer;
	m_indexBufferDeviceAddress = m_vkGetBufferDeviceAddressKHR(m_device, &vertexIndexAndAccelerationStructureBufferDeviceAddressInfo);

	vertexIndexAndAccelerationStructureBufferDeviceAddressInfo.buffer = m_topLevelAccelerationStructureInstancesBuffer;
	m_topLevelAccelerationStructureInstancesBufferDeviceAddress = m_vkGetBufferDeviceAddressKHR(m_device, &vertexIndexAndAccelerationStructureBufferDeviceAddressInfo);
}

void NtshEngn::GraphicsModule::createTopLevelAccelerationStructure() {
	VkAccelerationStructureGeometryInstancesDataKHR tlasGeometryInstancesData = {};
	tlasGeometryInstancesData.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
	tlasGeometryInstancesData.pNext = nullptr;
	tlasGeometryInstancesData.arrayOfPointers = false;
	tlasGeometryInstancesData.data.deviceAddress = m_topLevelAccelerationStructureInstancesBufferDeviceAddress;

	VkAccelerationStructureGeometryKHR tlasGeometry = {};
	tlasGeometry.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
	tlasGeometry.pNext = nullptr;
	tlasGeometry.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
	tlasGeometry.geometry.instances = tlasGeometryInstancesData;
	tlasGeometry.flags = 0;

	VkAccelerationStructureBuildGeometryInfoKHR tlasBuildGeometryInfo = {};
	tlasBuildGeometryInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
	tlasBuildGeometryInfo.pNext = nullptr;
	tlasBuildGeometryInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	tlasBuildGeometryInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
	tlasBuildGeometryInfo.srcAccelerationStructure = VK_NULL_HANDLE;
	tlasBuildGeometryInfo.dstAccelerationStructure = VK_NULL_HANDLE;
	tlasBuildGeometryInfo.geometryCount = 1;
	tlasBuildGeometryInfo.pGeometries = &tlasGeometry;
	tlasBuildGeometryInfo.ppGeometries = nullptr;
	tlasBuildGeometryInfo.scratchData = {};

	uint32_t maxPrimitiveCount = 131072;
	VkAccelerationStructureBuildSizesInfoKHR tlasBuildSizesInfo = {};
	tlasBuildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
	tlasBuildSizesInfo.pNext = nullptr;
	m_vkGetAccelerationStructureBuildSizesKHR(m_device, VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR, &tlasBuildGeometryInfo, &maxPrimitiveCount, &tlasBuildSizesInfo);

	VkBufferCreateInfo tlasBufferCreateInfo = {};
	tlasBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	tlasBufferCreateInfo.pNext = nullptr;
	tlasBufferCreateInfo.flags = 0;
	tlasBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	tlasBufferCreateInfo.queueFamilyIndexCount = 1;
	tlasBufferCreateInfo.pQueueFamilyIndices = &m_graphicsQueueFamilyIndex;

	VmaAllocationCreateInfo tlasBufferAllocationCreateInfo = {};
	tlasBufferAllocationCreateInfo.flags = 0;
	tlasBufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	tlasBufferCreateInfo.size = tlasBuildSizesInfo.accelerationStructureSize;
	tlasBufferCreateInfo.usage = VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR;
	NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &tlasBufferCreateInfo, &tlasBufferAllocationCreateInfo, &m_topLevelAccelerationStructureBuffer, &m_topLevelAccelerationStructureBufferAllocation, nullptr));

	m_topLevelAccelerationStructureBufferSize = tlasBuildSizesInfo.accelerationStructureSize;

	tlasBufferCreateInfo.size = tlasBuildSizesInfo.buildScratchSize;
	tlasBufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
	NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &tlasBufferCreateInfo, &tlasBufferAllocationCreateInfo, &m_topLevelAccelerationStructureScratchBuffer, &m_topLevelAccelerationStructureScratchBufferAllocation, nullptr));

	VkBufferDeviceAddressInfoKHR tlasScratchBufferDeviceAddressInfo = {};
	tlasScratchBufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR;
	tlasScratchBufferDeviceAddressInfo.pNext = nullptr;
	tlasScratchBufferDeviceAddressInfo.buffer = m_topLevelAccelerationStructureScratchBuffer;
	m_topLevelAccelerationStructureScratchBufferDeviceAddress = m_vkGetBufferDeviceAddressKHR(m_device, &tlasScratchBufferDeviceAddressInfo);

	tlasBuildGeometryInfo.scratchData.deviceAddress = m_topLevelAccelerationStructureScratchBufferDeviceAddress;

	VkAccelerationStructureCreateInfoKHR tlasCreateInfo = {};
	tlasCreateInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
	tlasCreateInfo.pNext = nullptr;
	tlasCreateInfo.createFlags = 0;
	tlasCreateInfo.buffer = m_topLevelAccelerationStructureBuffer;
	tlasCreateInfo.offset = 0;
	tlasCreateInfo.size = tlasBuildSizesInfo.accelerationStructureSize;
	tlasCreateInfo.type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
	tlasCreateInfo.deviceAddress = 0;
	NTSHENGN_VK_CHECK(m_vkCreateAccelerationStructureKHR(m_device, &tlasCreateInfo, nullptr, &m_topLevelAccelerationStructure));
}

void NtshEngn::GraphicsModule::createColorImage() {
	VkImageCreateInfo colorImageCreateInfo = {};
	colorImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	colorImageCreateInfo.pNext = nullptr;
	colorImageCreateInfo.flags = 0;
	colorImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	colorImageCreateInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	if (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) {
		colorImageCreateInfo.extent.width = static_cast<uint32_t>(windowModule->getWindowWidth(windowModule->getMainWindowID()));
		colorImageCreateInfo.extent.height = static_cast<uint32_t>(windowModule->getWindowHeight(windowModule->getMainWindowID()));
	}
	else {
		colorImageCreateInfo.extent.width = 1280;
		colorImageCreateInfo.extent.height = 720;
	}
	colorImageCreateInfo.extent.depth = 1;
	colorImageCreateInfo.mipLevels = 1;
	colorImageCreateInfo.arrayLayers = 1;
	colorImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	colorImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	colorImageCreateInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	colorImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	colorImageCreateInfo.queueFamilyIndexCount = 1;
	colorImageCreateInfo.pQueueFamilyIndices = &m_graphicsQueueFamilyIndex;
	colorImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo colorImageAllocationCreateInfo = {};
	colorImageAllocationCreateInfo.flags = 0;
	colorImageAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	NTSHENGN_VK_CHECK(vmaCreateImage(m_allocator, &colorImageCreateInfo, &colorImageAllocationCreateInfo, &m_colorImage, &m_colorImageAllocation, nullptr));

	VkImageViewCreateInfo colorImageViewCreateInfo = {};
	colorImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	colorImageViewCreateInfo.pNext = nullptr;
	colorImageViewCreateInfo.flags = 0;
	colorImageViewCreateInfo.image = m_colorImage;
	colorImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	colorImageViewCreateInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	colorImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
	colorImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
	colorImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
	colorImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
	colorImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	colorImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	colorImageViewCreateInfo.subresourceRange.levelCount = 1;
	colorImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	colorImageViewCreateInfo.subresourceRange.layerCount = 1;
	NTSHENGN_VK_CHECK(vkCreateImageView(m_device, &colorImageViewCreateInfo, nullptr, &m_colorImageView));

	// Layout transition VK_IMAGE_LAYOUT_UNDEFINED -> VK_IMAGE_LAYOUT_GENERAL
	NTSHENGN_VK_CHECK(vkResetCommandPool(m_device, m_initializationCommandPool, 0));

	VkCommandBufferBeginInfo colorImageTransitionCommandBufferBeginInfo = {};
	colorImageTransitionCommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	colorImageTransitionCommandBufferBeginInfo.pNext = nullptr;
	colorImageTransitionCommandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	colorImageTransitionCommandBufferBeginInfo.pInheritanceInfo = nullptr;
	NTSHENGN_VK_CHECK(vkBeginCommandBuffer(m_initializationCommandBuffer, &colorImageTransitionCommandBufferBeginInfo));

	VkImageMemoryBarrier2 undefinedToGeneralImageMemoryBarrier = {};
	undefinedToGeneralImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	undefinedToGeneralImageMemoryBarrier.pNext = nullptr;
	undefinedToGeneralImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
	undefinedToGeneralImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_NONE;
	undefinedToGeneralImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_RAY_TRACING_SHADER_BIT_KHR;
	undefinedToGeneralImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	undefinedToGeneralImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	undefinedToGeneralImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	undefinedToGeneralImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	undefinedToGeneralImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	undefinedToGeneralImageMemoryBarrier.image = m_colorImage;
	undefinedToGeneralImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	undefinedToGeneralImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	undefinedToGeneralImageMemoryBarrier.subresourceRange.levelCount = 1;
	undefinedToGeneralImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	undefinedToGeneralImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkDependencyInfo undefinedToGeneralDependencyInfo = {};
	undefinedToGeneralDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	undefinedToGeneralDependencyInfo.pNext = nullptr;
	undefinedToGeneralDependencyInfo.dependencyFlags = 0;
	undefinedToGeneralDependencyInfo.memoryBarrierCount = 0;
	undefinedToGeneralDependencyInfo.pMemoryBarriers = nullptr;
	undefinedToGeneralDependencyInfo.bufferMemoryBarrierCount = 0;
	undefinedToGeneralDependencyInfo.pBufferMemoryBarriers = nullptr;
	undefinedToGeneralDependencyInfo.imageMemoryBarrierCount = 1;
	undefinedToGeneralDependencyInfo.pImageMemoryBarriers = &undefinedToGeneralImageMemoryBarrier;
	m_vkCmdPipelineBarrier2KHR(m_initializationCommandBuffer, &undefinedToGeneralDependencyInfo);

	NTSHENGN_VK_CHECK(vkEndCommandBuffer(m_initializationCommandBuffer));

	VkSubmitInfo colorImageTransitionSubmitInfo = {};
	colorImageTransitionSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	colorImageTransitionSubmitInfo.pNext = nullptr;
	colorImageTransitionSubmitInfo.waitSemaphoreCount = 0;
	colorImageTransitionSubmitInfo.pWaitSemaphores = nullptr;
	colorImageTransitionSubmitInfo.pWaitDstStageMask = nullptr;
	colorImageTransitionSubmitInfo.commandBufferCount = 1;
	colorImageTransitionSubmitInfo.pCommandBuffers = &m_initializationCommandBuffer;
	colorImageTransitionSubmitInfo.signalSemaphoreCount = 0;
	colorImageTransitionSubmitInfo.pSignalSemaphores = nullptr;
	NTSHENGN_VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &colorImageTransitionSubmitInfo, m_initializationFence));
	NTSHENGN_VK_CHECK(vkWaitForFences(m_device, 1, &m_initializationFence, VK_TRUE, std::numeric_limits<uint64_t>::max()));
	NTSHENGN_VK_CHECK(vkResetFences(m_device, 1, &m_initializationFence));
}

void NtshEngn::GraphicsModule::createDescriptorSetLayout() {
	VkDescriptorSetLayoutBinding colorImageDescriptorSetLayoutBinding = {};
	colorImageDescriptorSetLayoutBinding.binding = 0;
	colorImageDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	colorImageDescriptorSetLayoutBinding.descriptorCount = 1;
	colorImageDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	colorImageDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding tlasDescriptorSetLayoutBinding = {};
	tlasDescriptorSetLayoutBinding.binding = 1;
	tlasDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	tlasDescriptorSetLayoutBinding.descriptorCount = 1;
	tlasDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	tlasDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding cameraDescriptorSetLayoutBinding = {};
	cameraDescriptorSetLayoutBinding.binding = 2;
	cameraDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	cameraDescriptorSetLayoutBinding.descriptorCount = 1;
	cameraDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	cameraDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding objectsDescriptorSetLayoutBinding = {};
	objectsDescriptorSetLayoutBinding.binding = 3;
	objectsDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	objectsDescriptorSetLayoutBinding.descriptorCount = 1;
	objectsDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	objectsDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding meshDescriptorSetLayoutBinding = {};
	meshDescriptorSetLayoutBinding.binding = 4;
	meshDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	meshDescriptorSetLayoutBinding.descriptorCount = 1;
	meshDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	meshDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding materialsDescriptorSetLayoutBinding = {};
	materialsDescriptorSetLayoutBinding.binding = 5;
	materialsDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	materialsDescriptorSetLayoutBinding.descriptorCount = 1;
	materialsDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	materialsDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding lightsDescriptorSetLayoutBinding = {};
	lightsDescriptorSetLayoutBinding.binding = 6;
	lightsDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	lightsDescriptorSetLayoutBinding.descriptorCount = 1;
	lightsDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	lightsDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding texturesDescriptorSetLayoutBinding = {};
	texturesDescriptorSetLayoutBinding.binding = 7;
	texturesDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	texturesDescriptorSetLayoutBinding.descriptorCount = 131072;
	texturesDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	texturesDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	std::array<VkDescriptorBindingFlags, 8> descriptorBindingFlags = { 0, 0, 0, 0, 0, 0, 0, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT };
	VkDescriptorSetLayoutBindingFlagsCreateInfo descriptorSetLayoutBindingFlagsCreateInfo = {};
	descriptorSetLayoutBindingFlagsCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	descriptorSetLayoutBindingFlagsCreateInfo.pNext = nullptr;
	descriptorSetLayoutBindingFlagsCreateInfo.bindingCount = static_cast<uint32_t>(descriptorBindingFlags.size());
	descriptorSetLayoutBindingFlagsCreateInfo.pBindingFlags = descriptorBindingFlags.data();

	std::array<VkDescriptorSetLayoutBinding, 8> descriptorSetLayoutBindings = { colorImageDescriptorSetLayoutBinding, tlasDescriptorSetLayoutBinding, cameraDescriptorSetLayoutBinding, objectsDescriptorSetLayoutBinding, meshDescriptorSetLayoutBinding, materialsDescriptorSetLayoutBinding, lightsDescriptorSetLayoutBinding, texturesDescriptorSetLayoutBinding };
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.pNext = &descriptorSetLayoutBindingFlagsCreateInfo;
	descriptorSetLayoutCreateInfo.flags = 0;
	descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(descriptorSetLayoutBindings.size());
	descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings.data();
	NTSHENGN_VK_CHECK(vkCreateDescriptorSetLayout(m_device, &descriptorSetLayoutCreateInfo, nullptr, &m_descriptorSetLayout));
}

std::vector<uint32_t> NtshEngn::GraphicsModule::compileShader(const std::string& shaderCode, ShaderType type) {
	if (!m_glslangInitialized) {
		glslang::InitializeProcess();
		m_glslangInitialized = true;
	}

	std::vector<uint32_t> spvCode;

	const char* shaderCodeCharPtr = shaderCode.c_str();

	EShLanguage shaderType = EShLangVertex;
	switch (type) {
	case ShaderType::Vertex:
		shaderType = EShLangVertex;
		break;

	case ShaderType::TesselationControl:
		shaderType = EShLangTessControl;
		break;

	case ShaderType::TesselationEvaluation:
		shaderType = EShLangTessEvaluation;
		break;

	case ShaderType::Geometry:
		shaderType = EShLangGeometry;
		break;

	case ShaderType::Fragment:
		shaderType = EShLangFragment;
		break;
	
	case ShaderType::RayGeneration:
		shaderType = EShLangRayGen;
		break;

	case ShaderType::RayIntersection:
		shaderType = EShLangIntersect;
		break;

	case ShaderType::RayAnyHit:
		shaderType = EShLangAnyHit;
		break;

	case ShaderType::RayClosestHit:
		shaderType = EShLangClosestHit;
		break;

	case ShaderType::RayMiss:
		shaderType = EShLangMiss;
		break;

	case ShaderType::RayCallable:
		shaderType = EShLangCallable;
		break;
	}

	glslang::TShader shader(shaderType);
	shader.setStrings(&shaderCodeCharPtr, 1);
	int clientInputSemanticsVersion = 110;
	glslang::EshTargetClientVersion vulkanClientVersion = glslang::EShTargetVulkan_1_1;
	glslang::EShTargetLanguageVersion spvLanguageVersion = glslang::EShTargetSpv_1_4;
	shader.setEnvInput(glslang::EShSourceGlsl, shaderType, glslang::EShClientVulkan, clientInputSemanticsVersion);
	shader.setEnvClient(glslang::EShClientVulkan, vulkanClientVersion);
	shader.setEnvTarget(glslang::EshTargetSpv, spvLanguageVersion);
	EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);
	int defaultVersion = 460;

	// Preprocess
	const TBuiltInResource defaultTBuiltInResource = {
		/* .MaxLights = */ 32,
		/* .MaxClipPlanes = */ 6,
		/* .MaxTextureUnits = */ 32,
		/* .MaxTextureCoords = */ 32,
		/* .MaxVertexAttribs = */ 64,
		/* .MaxVertexUniformComponents = */ 4096,
		/* .MaxVaryingFloats = */ 64,
		/* .MaxVertexTextureImageUnits = */ 32,
		/* .MaxCombinedTextureImageUnits = */ 80,
		/* .MaxTextureImageUnits = */ 32,
		/* .MaxFragmentUniformComponents = */ 4096,
		/* .MaxDrawBuffers = */ 32,
		/* .MaxVertexUniformVectors = */ 128,
		/* .MaxVaryingVectors = */ 8,
		/* .MaxFragmentUniformVectors = */ 16,
		/* .MaxVertexOutputVectors = */ 16,
		/* .MaxFragmentInputVectors = */ 15,
		/* .MinProgramTexelOffset = */ -8,
		/* .MaxProgramTexelOffset = */ 7,
		/* .MaxClipDistances = */ 8,
		/* .MaxComputeWorkGroupCountX = */ 65535,
		/* .MaxComputeWorkGroupCountY = */ 65535,
		/* .MaxComputeWorkGroupCountZ = */ 65535,
		/* .MaxComputeWorkGroupSizeX = */ 1024,
		/* .MaxComputeWorkGroupSizeY = */ 1024,
		/* .MaxComputeWorkGroupSizeZ = */ 64,
		/* .MaxComputeUniformComponents = */ 1024,
		/* .MaxComputeTextureImageUnits = */ 16,
		/* .MaxComputeImageUniforms = */ 8,
		/* .MaxComputeAtomicCounters = */ 8,
		/* .MaxComputeAtomicCounterBuffers = */ 1,
		/* .MaxVaryingComponents = */ 60,
		/* .MaxVertexOutputComponents = */ 64,
		/* .MaxGeometryInputComponents = */ 64,
		/* .MaxGeometryOutputComponents = */ 128,
		/* .MaxFragmentInputComponents = */ 128,
		/* .MaxImageUnits = */ 8,
		/* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
		/* .MaxCombinedShaderOutputResources = */ 8,
		/* .MaxImageSamples = */ 0,
		/* .MaxVertexImageUniforms = */ 0,
		/* .MaxTessControlImageUniforms = */ 0,
		/* .MaxTessEvaluationImageUniforms = */ 0,
		/* .MaxGeometryImageUniforms = */ 0,
		/* .MaxFragmentImageUniforms = */ 8,
		/* .MaxCombinedImageUniforms = */ 8,
		/* .MaxGeometryTextureImageUnits = */ 16,
		/* .MaxGeometryOutputVertices = */ 256,
		/* .MaxGeometryTotalOutputComponents = */ 1024,
		/* .MaxGeometryUniformComponents = */ 1024,
		/* .MaxGeometryVaryingComponents = */ 64,
		/* .MaxTessControlInputComponents = */ 128,
		/* .MaxTessControlOutputComponents = */ 128,
		/* .MaxTessControlTextureImageUnits = */ 16,
		/* .MaxTessControlUniformComponents = */ 1024,
		/* .MaxTessControlTotalOutputComponents = */ 4096,
		/* .MaxTessEvaluationInputComponents = */ 128,
		/* .MaxTessEvaluationOutputComponents = */ 128,
		/* .MaxTessEvaluationTextureImageUnits = */ 16,
		/* .MaxTessEvaluationUniformComponents = */ 1024,
		/* .MaxTessPatchComponents = */ 120,
		/* .MaxPatchVertices = */ 32,
		/* .MaxTessGenLevel = */ 64,
		/* .MaxViewports = */ 16,
		/* .MaxVertexAtomicCounters = */ 0,
		/* .MaxTessControlAtomicCounters = */ 0,
		/* .MaxTessEvaluationAtomicCounters = */ 0,
		/* .MaxGeometryAtomicCounters = */ 0,
		/* .MaxFragmentAtomicCounters = */ 8,
		/* .MaxCombinedAtomicCounters = */ 8,
		/* .MaxAtomicCounterBindings = */ 1,
		/* .MaxVertexAtomicCounterBuffers = */ 0,
		/* .MaxTessControlAtomicCounterBuffers = */ 0,
		/* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
		/* .MaxGeometryAtomicCounterBuffers = */ 0,
		/* .MaxFragmentAtomicCounterBuffers = */ 1,
		/* .MaxCombinedAtomicCounterBuffers = */ 1,
		/* .MaxAtomicCounterBufferSize = */ 16384,
		/* .MaxTransformFeedbackBuffers = */ 4,
		/* .MaxTransformFeedbackInterleavedComponents = */ 64,
		/* .MaxCullDistances = */ 8,
		/* .MaxCombinedClipAndCullDistances = */ 8,
		/* .MaxSamples = */ 4,
		/* .maxMeshOutputVerticesNV = */ 256,
		/* .maxMeshOutputPrimitivesNV = */ 512,
		/* .maxMeshWorkGroupSizeX_NV = */ 32,
		/* .maxMeshWorkGroupSizeY_NV = */ 1,
		/* .maxMeshWorkGroupSizeZ_NV = */ 1,
		/* .maxTaskWorkGroupSizeX_NV = */ 32,
		/* .maxTaskWorkGroupSizeY_NV = */ 1,
		/* .maxTaskWorkGroupSizeZ_NV = */ 1,
		/* .maxMeshViewCountNV = */ 4,
		/* .maxMeshOutputVerticesEXT = */ 256,
		/* .maxMeshOutputPrimitivesEXT = */ 256,
		/* .maxMeshWorkGroupSizeX_EXT = */ 128,
		/* .maxMeshWorkGroupSizeY_EXT = */ 128,
		/* .maxMeshWorkGroupSizeZ_EXT = */ 128,
		/* .maxTaskWorkGroupSizeX_EXT = */ 128,
		/* .maxTaskWorkGroupSizeY_EXT = */ 128,
		/* .maxTaskWorkGroupSizeZ_EXT = */ 128,
		/* .maxMeshViewCountEXT = */ 4,
		/* .maxDualSourceDrawBuffersEXT = */ 1,

		/* .limits = */ {
			/* .nonInductiveForLoops = */ 1,
			/* .whileLoops = */ 1,
			/* .doWhileLoops = */ 1,
			/* .generalUniformIndexing = */ 1,
			/* .generalAttributeMatrixVectorIndexing = */ 1,
			/* .generalVaryingIndexing = */ 1,
			/* .generalSamplerIndexing = */ 1,
			/* .generalVariableIndexing = */ 1,
			/* .generalConstantMatrixVectorIndexing = */ 1,
		} };
	DirStackFileIncluder includer;
	std::string preprocess;
	if (!shader.preprocess(&defaultTBuiltInResource, defaultVersion, ENoProfile, false, false, messages, &preprocess, includer)) {
		NTSHENGN_MODULE_WARNING("Shader preprocessing failed.\n" + std::string(shader.getInfoLog()));
		return spvCode;
	}

	// Parse
	const char* preprocessCharPtr = preprocess.c_str();
	shader.setStrings(&preprocessCharPtr, 1);
	if (!shader.parse(&defaultTBuiltInResource, defaultVersion, false, messages)) {
		NTSHENGN_MODULE_WARNING("Shader parsing failed.\n" + std::string(shader.getInfoLog()));
		return spvCode;
	}

	// Link
	glslang::TProgram program;
	program.addShader(&shader);
	if (!program.link(messages)) {
		NTSHENGN_MODULE_WARNING("Shader linking failed.");
		return spvCode;
	}

	// Compile
	spv::SpvBuildLogger buildLogger;
	glslang::SpvOptions spvOptions;
	glslang::GlslangToSpv(*program.getIntermediate(shaderType), spvCode, &buildLogger, &spvOptions);

	return spvCode;
}

void NtshEngn::GraphicsModule::createRayTracingPipeline() {
	const std::string rayGenShaderCode = R"GLSL(
		#version 460
		#extension GL_EXT_ray_tracing : require
		#extension GL_EXT_scalar_block_layout : enable

		#define M_PI 3.1415926535897932384626433832795

		layout(set = 0, binding = 0, rgba32f) uniform image2D image; 
		layout(set = 0, binding = 1) uniform accelerationStructureEXT tlas;

		layout(set = 0, binding = 2) uniform Camera {
			mat4 view;
			mat4 projection;
			vec3 position;
		} camera;

		layout(push_constant) uniform PushConstants {
			uint sampleBatch;
		} pC;

		struct HitPayload {
			vec3 directLighting;
			vec4 brdf;
			vec3 emissive;
			vec3 rayOrigin;
			vec3 rayDirection;
			uint rngState;
			bool hitBackground;
			bool dontAccumulate;
		};

		layout(location = 0) rayPayloadEXT HitPayload payload;

		float rngFloat(inout uint rngState) {
			rngState = rngState * 747796405 + 1;
			uint word = ((rngState >> ((rngState >> 28) + 4)) ^ rngState) * 277803737;
			word = (word >> 22) ^ word;

			return float(word) / 4294967295.0f;
		}

		vec2 randomGaussian(inout uint rngState) {
			const float u1 = max(1e-38, rngFloat(rngState));
			const float u2 = rngFloat(rngState);
			const float r = sqrt(-2.0 * log(u1));
			const float theta = 2.0 * M_PI * u2;
	
			return r * vec2(cos(theta), sin(theta));
		}

		void main() {
			payload.rngState = (pC.sampleBatch * gl_LaunchSizeEXT.y + gl_LaunchIDEXT.y) * gl_LaunchSizeEXT.x + gl_LaunchIDEXT.x;

			const vec2 pixelCenter = vec2(gl_LaunchIDEXT.xy) + vec2(0.5);
			const vec2 pixelCenterGaussianOffset = pixelCenter + 0.375 * randomGaussian(payload.rngState);
			const vec2 uv = pixelCenterGaussianOffset / vec2(gl_LaunchSizeEXT);
			const vec2 d = uv * 2.0 - 1.0;

			const mat4 inverseView = inverse(camera.view);
			const mat4 inverseProjection = inverse(camera.projection);
			vec3 origin = vec3(inverseView * vec4(0.0, 0.0, 0.0, 1.0));
			const vec4 target = inverseProjection * vec4(d, 1.0, 1.0);
			vec3 direction = vec3(inverseView * vec4(normalize(target.xyz), 0.0));

			vec3 color = vec3(0.0);
			vec3 beta = vec3(1.0);

			const uint rayFlags = gl_RayFlagsOpaqueEXT;
			const float tMin = 0.001;
			const float tMax = 10000.0;

			const uint NUM_BOUNCES = 2;
			for (uint i = 0; i < NUM_BOUNCES + 1; i++) {
				traceRayEXT(tlas, rayFlags, 0xFF, 0, 0, 0, origin, tMin, direction, tMax, 0);

				if (payload.dontAccumulate) {
					origin = payload.rayOrigin;
					direction = payload.rayDirection;
					continue;
				}

				color += payload.directLighting * beta;

				if (payload.hitBackground) {
					break;
				}

				color += payload.emissive * beta;

				if (payload.brdf.a > 0.0) {
					beta *= payload.brdf.rgb / payload.brdf.a;
				}

				origin = payload.rayOrigin;
				direction = payload.rayDirection;
			}

			if (pC.sampleBatch != 0) {
				const vec3 previousColor = imageLoad(image, ivec2(gl_LaunchIDEXT.xy)).rgb;

				color = (pC.sampleBatch * previousColor + color) / (pC.sampleBatch + 1);
			}
			imageStore(image, ivec2(gl_LaunchIDEXT.xy), vec4(color, 1.0));
		}
	)GLSL";
	const std::vector<uint32_t> rayGenShaderSpv = compileShader(rayGenShaderCode, ShaderType::RayGeneration);

	VkShaderModule rayGenShaderModule;
	VkShaderModuleCreateInfo rayGenShaderModuleCreateInfo = {};
	rayGenShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	rayGenShaderModuleCreateInfo.pNext = nullptr;
	rayGenShaderModuleCreateInfo.flags = 0;
	rayGenShaderModuleCreateInfo.codeSize = rayGenShaderSpv.size() * sizeof(uint32_t);
	rayGenShaderModuleCreateInfo.pCode = rayGenShaderSpv.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &rayGenShaderModuleCreateInfo, nullptr, &rayGenShaderModule));

	VkPipelineShaderStageCreateInfo rayGenShaderStageCreateInfo = {};
	rayGenShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	rayGenShaderStageCreateInfo.pNext = nullptr;
	rayGenShaderStageCreateInfo.flags = 0;
	rayGenShaderStageCreateInfo.stage = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	rayGenShaderStageCreateInfo.module = rayGenShaderModule;
	rayGenShaderStageCreateInfo.pName = "main";
	rayGenShaderStageCreateInfo.pSpecializationInfo = nullptr;

	VkRayTracingShaderGroupCreateInfoKHR rayGenShaderGroupCreateInfo = {};
	rayGenShaderGroupCreateInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	rayGenShaderGroupCreateInfo.pNext = nullptr;
	rayGenShaderGroupCreateInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	rayGenShaderGroupCreateInfo.generalShader = 0;
	rayGenShaderGroupCreateInfo.closestHitShader = VK_SHADER_UNUSED_KHR;
	rayGenShaderGroupCreateInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
	rayGenShaderGroupCreateInfo.intersectionShader = VK_SHADER_UNUSED_KHR;
	rayGenShaderGroupCreateInfo.pShaderGroupCaptureReplayHandle = nullptr;

	const std::string rayMissShaderCode = R"GLSL(
		#version 460
		#extension GL_EXT_ray_tracing : require

		struct HitPayload {
			vec3 directLighting;
			vec4 brdf;
			vec3 emissive;
			vec3 rayOrigin;
			vec3 rayDirection;
			uint rngState;
			bool hitBackground;
			bool dontAccumulate;
		};

		layout(location = 0) rayPayloadInEXT HitPayload payload;

	    layout(push_constant) uniform BackgroundColor {
            layout(offset = 16) vec4 color;
        } bg;

		void main() {
			payload.directLighting = bg.color.rgb;
			payload.hitBackground = true;
			payload.dontAccumulate = false;
		}
	)GLSL";
	const std::vector<uint32_t> rayMissShaderSpv = compileShader(rayMissShaderCode, ShaderType::RayMiss);

	VkShaderModule rayMissShaderModule;
	VkShaderModuleCreateInfo rayMissShaderModuleCreateInfo = {};
	rayMissShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	rayMissShaderModuleCreateInfo.pNext = nullptr;
	rayMissShaderModuleCreateInfo.flags = 0;
	rayMissShaderModuleCreateInfo.codeSize = rayMissShaderSpv.size() * sizeof(uint32_t);
	rayMissShaderModuleCreateInfo.pCode = rayMissShaderSpv.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &rayMissShaderModuleCreateInfo, nullptr, &rayMissShaderModule));

	VkPipelineShaderStageCreateInfo rayMissShaderStageCreateInfo = {};
	rayMissShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	rayMissShaderStageCreateInfo.pNext = nullptr;
	rayMissShaderStageCreateInfo.flags = 0;
	rayMissShaderStageCreateInfo.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
	rayMissShaderStageCreateInfo.module = rayMissShaderModule;
	rayMissShaderStageCreateInfo.pName = "main";
	rayMissShaderStageCreateInfo.pSpecializationInfo = nullptr;

	VkRayTracingShaderGroupCreateInfoKHR rayMissShaderGroupCreateInfo = {};
	rayMissShaderGroupCreateInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	rayMissShaderGroupCreateInfo.pNext = nullptr;
	rayMissShaderGroupCreateInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	rayMissShaderGroupCreateInfo.generalShader = 1;
	rayMissShaderGroupCreateInfo.closestHitShader = VK_SHADER_UNUSED_KHR;
	rayMissShaderGroupCreateInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
	rayMissShaderGroupCreateInfo.intersectionShader = VK_SHADER_UNUSED_KHR;
	rayMissShaderGroupCreateInfo.pShaderGroupCaptureReplayHandle = nullptr;

	const std::string rayShadowMissShaderCode = R"GLSL(
		#version 460
		#extension GL_EXT_ray_tracing : require

		layout(location = 1) rayPayloadInEXT bool isShadowed;

		void main() {
			isShadowed = false;
		}
	)GLSL";
	const std::vector<uint32_t> rayShadowMissShaderSpv = compileShader(rayShadowMissShaderCode, ShaderType::RayMiss);

	VkShaderModule rayShadowMissShaderModule;
	VkShaderModuleCreateInfo rayShadowMissShaderModuleCreateInfo = {};
	rayShadowMissShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	rayShadowMissShaderModuleCreateInfo.pNext = nullptr;
	rayShadowMissShaderModuleCreateInfo.flags = 0;
	rayShadowMissShaderModuleCreateInfo.codeSize = rayShadowMissShaderSpv.size() * sizeof(uint32_t);
	rayShadowMissShaderModuleCreateInfo.pCode = rayShadowMissShaderSpv.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &rayShadowMissShaderModuleCreateInfo, nullptr, &rayShadowMissShaderModule));

	VkPipelineShaderStageCreateInfo rayShadowMissShaderStageCreateInfo = {};
	rayShadowMissShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	rayShadowMissShaderStageCreateInfo.pNext = nullptr;
	rayShadowMissShaderStageCreateInfo.flags = 0;
	rayShadowMissShaderStageCreateInfo.stage = VK_SHADER_STAGE_MISS_BIT_KHR;
	rayShadowMissShaderStageCreateInfo.module = rayShadowMissShaderModule;
	rayShadowMissShaderStageCreateInfo.pName = "main";
	rayShadowMissShaderStageCreateInfo.pSpecializationInfo = nullptr;

	VkRayTracingShaderGroupCreateInfoKHR rayShadowMissShaderGroupCreateInfo = {};
	rayShadowMissShaderGroupCreateInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	rayShadowMissShaderGroupCreateInfo.pNext = nullptr;
	rayShadowMissShaderGroupCreateInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
	rayShadowMissShaderGroupCreateInfo.generalShader = 2;
	rayShadowMissShaderGroupCreateInfo.closestHitShader = VK_SHADER_UNUSED_KHR;
	rayShadowMissShaderGroupCreateInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
	rayShadowMissShaderGroupCreateInfo.intersectionShader = VK_SHADER_UNUSED_KHR;
	rayShadowMissShaderGroupCreateInfo.pShaderGroupCaptureReplayHandle = nullptr;

	const std::string rayClosestHitShaderCode = R"GLSL(
		#version 460
		#extension GL_EXT_ray_tracing : require
		#extension GL_EXT_nonuniform_qualifier : enable
		#extension GL_EXT_scalar_block_layout : enable
		#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
		#extension GL_EXT_buffer_reference2 : require

		#define M_PI 3.1415926535897932384626433832795

		struct ObjectInfo {
			uint meshID;
			uint materialID;
		};

		struct MeshInfo {
			uint64_t vertexAddress;
			uint64_t indexAddress;
		};

		struct MaterialInfo {
			uint diffuseTextureIndex;
			uint normalTextureIndex;
			uint metalnessTextureIndex;
			uint roughnessTextureIndex;
			uint occlusionTextureIndex;
			uint emissiveTextureIndex;
			float emissiveFactor;
			float alphaCutoff;
		};

		struct LightInfo {
			vec3 position;
			vec3 direction;
			vec3 color;
			float intensity;
			vec2 cutoff;
		};

		struct Vertex {
			vec3 position;
			vec3 normal;
			vec2 uv;
			vec3 color;
			vec4 tangent;
			vec4 joints;
			vec4 weights;
		};

		layout(set = 0, binding = 1) uniform accelerationStructureEXT tlas;

		layout(std430, set = 0, binding = 3) restrict readonly buffer Objects {
			ObjectInfo info[];
		} objects;

		layout(set = 0, binding = 4) restrict readonly buffer Meshes {
			MeshInfo info[];
		} meshes;

		layout(set = 0, binding = 5) restrict readonly buffer Materials {
			MaterialInfo info[];
		} materials;

		layout(set = 0, binding = 6) restrict readonly buffer Lights {
			uvec4 count;
			LightInfo info[];
		} lights;

		layout(set = 0, binding = 7) uniform sampler2D textures[];

		layout(buffer_reference, scalar) buffer Vertices {
			Vertex vertex[];
		};

		layout(buffer_reference, scalar) buffer Indices {
			uvec3 index[];
		};

		struct HitPayload {
			vec3 directLighting;
			vec4 brdf;
			vec3 emissive;
			vec3 rayOrigin;
			vec3 rayDirection;
			uint rngState;
			bool hitBackground;
			bool dontAccumulate;
		};

		layout(location = 0) rayPayloadInEXT HitPayload payload;
		layout(location = 1) rayPayloadEXT bool isShadowed;

		hitAttributeEXT vec2 attribs;

		float rngFloat(inout uint rngState) {
			rngState = rngState * 747796405 + 1;
			uint word = ((rngState >> ((rngState >> 28) + 4)) ^ rngState) * 277803737;
			word = (word >> 22) ^ word;
	
			return float(word) / 4294967295.0f;
		}

		// https://graphics.pixar.com/library/OrthonormalB/paper.pdf
		void pixarBasis(vec3 normal, inout vec3 b1, inout vec3 b2) {
			const float s = sign(normal.z);
			const float a = -1.0 / (s + normal.z);
			const float b = normal.x * normal.y * a;
			b1 = vec3(1.0 + s * normal.x * normal.x * a, s * b, -s * normal.x);
			b2 = vec3(b, s + normal.y * normal.y * a, -normal.y);
		}

		// https://www.jcgt.org/published/0007/04/01/sampleGGXVNDF.h
		vec3 sampleGGXVNDF(vec3 v, float r1, float r2, float u1, float u2) {
			const vec3 vh = normalize(vec3(r1 * v.x, r2 * v.y, v.z));
	
			const float lensq = vh.x * vh.x + vh.y * vh.y;
			const vec3 T1 = lensq > 0.0 ? vec3(-vh.y, vh.x, 0.0) * inversesqrt(lensq) : vec3(1.0, 0.0, 0.0);
			const vec3 T2 = cross(vh, T1);

			const float r = sqrt(u1);
			const float phi = 2.0 * M_PI * u2;
			const float t1 = r * cos(phi);
			float t2 = r * sin(phi);
			const float s = 0.5 * (1.0 + vh.z);
			t2 = (1.0 - s) * sqrt(1.0 - t1 * t1) + s * t2;
	
			const vec3 nh = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * vh;

			const vec3 n = normalize(vec3(r1 * nh.x, r2 * nh.y, max(0.0, nh.z)));
	
			return n;
		}

		vec3 offsetPositionAlongNormal(vec3 worldPosition, vec3 normal) {
			const float intScale = 256.0;
			const ivec3 intNormal = ivec3(intScale * normal);

			const vec3 p = vec3(
				intBitsToFloat(floatBitsToInt(worldPosition.x) + ((worldPosition.x < 0) ? -intNormal.x : intNormal.x)),
				intBitsToFloat(floatBitsToInt(worldPosition.y) + ((worldPosition.y < 0) ? -intNormal.y : intNormal.y)),
				intBitsToFloat(floatBitsToInt(worldPosition.z) + ((worldPosition.z < 0) ? -intNormal.z : intNormal.z))
				);

			const float origin = 1.0 / 32.0;
			const float floatScale = 1.0 / 65536.0;

			return vec3(
				abs(worldPosition.x) < origin ? worldPosition.x + floatScale * normal.x : p.x,
				abs(worldPosition.y) < origin ? worldPosition.y + floatScale * normal.y : p.y,
				abs(worldPosition.z) < origin ? worldPosition.z + floatScale * normal.z : p.z
				);
		}

		vec3 randomDiffuseDirection(vec3 normal, inout uint rngState) {
			const float theta = 2.0 * M_PI * rngFloat(rngState);
			const float u = 2.0 * rngFloat(rngState) - 1.0;
			const float r = sqrt(1.0 - u * u);
			const vec3 direction = normal + vec3(r * cos(theta), r * sin(theta), u);

			return normalize(direction);
		}

		// BRDF
		float distribution(float NdotH, float roughness) {
			const float a = roughness * roughness;
			const float aSquare = a * a;
			const float NdotHSquare = NdotH * NdotH;
			const float denom = max(NdotHSquare * (aSquare - 1.0) + 1.0, 0.001);

			return aSquare / (M_PI * denom * denom);
		}

		vec3 fresnel(float cosTheta, vec3 f0) {
			return f0 + (1.0 - f0) * pow(1.0 - cosTheta, 5.0);
		}

		float g(float NdotV, float roughness) {
			const float r = roughness + 1.0;
			const float k = (r * r) / 8.0;
			const float denom = NdotV * (1.0 - k) + k;

			return NdotV / denom;
		}

		float smith(float LdotN, float VdotN, float roughness) {
			const float gv = g(VdotN, roughness);
			const float gl = g(LdotN, roughness);

			return gv * gl;
		}

		float GGXVNDFpdf(float VdotN, float d, float g) {
			return (d * g) / max(4.0 * VdotN, 0.00001);
		}

		vec3 diffuseFresnelCorrection(vec3 ior) {
			const vec3 iorSquare = ior * ior;
			const bvec3 TIR = lessThan(ior, vec3(1.0));
			const vec3 invDenum = mix(vec3(1.0), vec3(1.0) / (iorSquare * iorSquare * (vec3(554.33) * 380.7 * ior)), TIR);
			vec3 num = ior * mix(vec3(0.1921156102251088), ior * 298.25 - 261.38 * iorSquare + 138.43, TIR);
			num += mix(vec3(0.8078843897748912), vec3(-1.07), TIR);

			return num * invDenum;
		}

		vec4 sampleBRDF(vec3 n, vec3 v, vec3 diffuse, float metalness, float roughness, inout uint rngState, inout vec3 nextRayDirection) {
			vec3 t;
			vec3 b;
			pixarBasis(n, t, b);
			vec3 h = sampleGGXVNDF(vec3(dot(v, t), dot(v, b), dot(v, n)), roughness, roughness, rngFloat(rngState), rngFloat(rngState));
			if (h.z < 0.0) {
				h = -h;
			}
			h = h.x * t + h.y * b + h.z * n;

			const vec3 f = fresnel(dot(v, h), mix(vec3(0.04), diffuse, metalness));

			float diffw = (1.0 - metalness);
			float specw = dot(f, vec3(0.299, 0.587, 0.114));
			const float invw = 1.0 / (diffw + specw);
			diffw *= invw;
			specw *= invw;

			vec4 brdf;
			const float partSample = rngFloat(rngState);
			if (partSample < diffw) {
				// Diffuse
				nextRayDirection = randomDiffuseDirection(n, rngState);

				const float LdotN = dot(nextRayDirection, n);
				const float VdotN = dot(v, n);
				
				if (LdotN <= 0.0 || VdotN <= 0.0) {
					return vec4(0.0);
				}

				vec3 h = vec3(v + nextRayDirection);
				const float LdotH = dot(nextRayDirection, h);
				const float pdf = LdotN / M_PI;

				const vec3 fT = fresnel(LdotN, mix(vec3(0.04), diffuse, metalness));
				const vec3 fTIR = fresnel(VdotN, mix(vec3(0.04), diffuse, metalness));
				const vec3 dfc = diffuseFresnelCorrection(vec3(1.05));

				const vec3 lambertian = diffuse / M_PI;

				const vec3 diff = ((vec3(1.0) - fT) * (vec3(1.0 - fTIR)) * lambertian) * dfc;
				brdf.rgb = diff * LdotN;
				brdf.a = diffw * pdf;
			}
			else {
				// Specular
				nextRayDirection = reflect(-v, h);

				const float LdotN = dot(nextRayDirection, n);
				const float VdotN = dot(v, n);

				if (LdotN <= 0.0 || VdotN <= 0.0) {
					return vec4(0.0);
				}

				const float NdotH = dot(n, h);
				const float d = distribution(NdotH, roughness);
				const float g = smith(LdotN, VdotN, roughness);
				const float pdf = GGXVNDFpdf(VdotN, d, g);

				const vec3 spec = (d * f * g) / max(4.0 * LdotN * VdotN, 0.001);
				brdf.rgb = spec * LdotN;
				brdf.a = specw * pdf;
			}

			return brdf;
		}

		vec3 shade(vec3 n, vec3 v, vec3 l, vec3 lc, vec3 diffuse, float metalness, float roughness) {
			const vec3 h = normalize(v + l);
			
			const float LdotH = max(dot(l, h), 0.0);
			const float NdotH = max(dot(n, h), 0.0);
			const float VdotH = max(dot(v, h), 0.0);
			const float LdotN = max(dot(l, n), 0.0);
			const float VdotN = max(dot(v, n), 0.0);

			const float d = distribution(NdotH, roughness);
			const vec3 f = fresnel(LdotH, mix(vec3(0.04), diffuse, metalness));
			const vec3 fT = fresnel(LdotN, mix(vec3(0.04), diffuse, metalness));
			const vec3 fTIR = fresnel(VdotN, mix(vec3(0.04), diffuse, metalness));
			const float g = smith(LdotN, VdotN, roughness);
			const vec3 dfc = diffuseFresnelCorrection(vec3(1.05));

			const vec3 lambertian = diffuse / M_PI;

			const vec3 brdf = (d * f * g) / max(4.0 * LdotN * VdotN, 0.001) + ((vec3(1.0) - fT) * (vec3(1.0 - fTIR)) * lambertian) * dfc;

			return lc * brdf * LdotN;
		}

		float shadows(vec3 l, float distanceToLight) {
			isShadowed = true;
			float tMin = 0.001;
			float tMax = distanceToLight;
			vec3 origin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
			vec3 direction = l;
			uint rayFlags = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;
			traceRayEXT(tlas, rayFlags, 0xFF, 0, 0, 1, origin, tMin, direction, tMax, 1);

			if (isShadowed) {
				return 0.0;
			}
			return 1.0;
		}

		void main() {
			ObjectInfo object = objects.info[gl_InstanceCustomIndexEXT];
			MeshInfo mesh = meshes.info[object.meshID];
			MaterialInfo material = materials.info[object.materialID];
			Vertices vertices = Vertices(mesh.vertexAddress);
			Indices indices = Indices(mesh.indexAddress);

			// Vertices
			uvec3 ind = indices.index[gl_PrimitiveID];
			Vertex v0 = vertices.vertex[ind.x];
			Vertex v1 = vertices.vertex[ind.y];
			Vertex v2 = vertices.vertex[ind.z];

			vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

			vec3 position = v0.position * barycentrics.x + v1.position * barycentrics.y + v2.position * barycentrics.z;
			vec3 worldPosition = vec3(gl_ObjectToWorldEXT * vec4(position, 1.0));

			vec3 normal = v0.normal * barycentrics.x + v1.normal * barycentrics.y + v2.normal * barycentrics.z;
			vec3 worldNormal = normalize(vec3(normal * gl_WorldToObjectEXT));

			vec3 tangent = v0.tangent.xyz * barycentrics.x + v1.tangent.xyz * barycentrics.y + v2.tangent.xyz * barycentrics.z;
			vec3 worldTangent = vec3(gl_ObjectToWorldEXT * vec4(tangent, 0.0));

			vec3 bitangent = cross(normal, tangent.xyz) * v0.tangent.w;
			vec3 worldBitangent = vec3(gl_ObjectToWorldEXT * vec4(bitangent, 0.0));

			mat3 TBN = mat3(worldTangent, worldBitangent, worldNormal);

			vec2 uv = v0.uv * barycentrics.x + v1.uv * barycentrics.y + v2.uv * barycentrics.z;

			// Material
			vec4 diffuseSample = texture(textures[nonuniformEXT(material.diffuseTextureIndex)], uv);
			if (diffuseSample.a < material.alphaCutoff) {
				payload.dontAccumulate = true;
				payload.rayOrigin = offsetPositionAlongNormal(worldPosition, gl_WorldRayDirectionEXT);
				payload.rayDirection = gl_WorldRayDirectionEXT;
				return;
			}
			vec3 normalSample = texture(textures[nonuniformEXT(material.normalTextureIndex)], uv).xyz;
			float metalnessSample = texture(textures[nonuniformEXT(material.metalnessTextureIndex)], uv).b;
			float roughnessSample = texture(textures[nonuniformEXT(material.roughnessTextureIndex)], uv).g;
			float occlusionSample = texture(textures[nonuniformEXT(material.occlusionTextureIndex)], uv).r;
			vec3 emissiveSample = texture(textures[nonuniformEXT(material.emissiveTextureIndex)], uv).rgb;

			vec3 d = diffuseSample.rgb;
			vec3 n = normalize(TBN * (normalSample * 2.0 - 1.0));
			vec3 v = -gl_WorldRayDirectionEXT;

			vec3 directLighting = vec3(0.0);

			// Pick a random light
			uint lightCount = lights.count.x + lights.count.y + lights.count.z;
			if (lightCount != 0) {
				uint lightIndex = uint(floor(rngFloat(payload.rngState) * lightCount));

				vec3 l;
				vec3 lc;
				float intensity = 1.0;
				float distance = 10000.0;

				// Directional Light
				if (lightIndex < lights.count.x) {
					l = -lights.info[lightIndex].direction;
					lc = lights.info[lightIndex].color * lights.info[lightIndex].intensity;
				}
				// Point Light
				else if (lightIndex < (lights.count.x + lights.count.y)) {
					l = normalize(lights.info[lightIndex].position - worldPosition);
					distance = length(lights.info[lightIndex].position - worldPosition);
					float attenuation = 1.0 / (distance * distance);
					lc = (lights.info[lightIndex].color * lights.info[lightIndex].intensity) * attenuation;
				}
				// Spot Light
				else {
					// If the random float between 0.0 and 1.0 is 1.0
					if (lightIndex == lightCount) {
						lightIndex--;
					}

					l = normalize(lights.info[lightIndex].position - worldPosition);
					lc = lights.info[lightIndex].color * lights.info[lightIndex].intensity;
					float theta = dot(l, -lights.info[lightIndex].direction);
					float epsilon = cos(lights.info[lightIndex].cutoff.y) - cos(lights.info[lightIndex].cutoff.x);
					intensity = clamp((theta - cos(lights.info[lightIndex].cutoff.x)) / epsilon, 0.0, 1.0);
					intensity = 1.0 - intensity;
					distance = length(lights.info[lightIndex].position - worldPosition);
				}

				directLighting = (shade(n, v, l, lc * intensity, d * intensity, metalnessSample, roughnessSample) / float(lightCount)) * shadows(l, distance);
				directLighting *= occlusionSample;
			}

			// Ambient Lights
			uint lightIndex = lightCount;
            for (uint i = 0; i < lights.count.w; i++) {
                directLighting += (lights.info[lightIndex].color * lights.info[lightIndex].intensity) * d;

                lightIndex++;
            }

			vec4 brdf = sampleBRDF(n, v, d, metalnessSample, roughnessSample, payload.rngState, payload.rayDirection);

			payload.directLighting = directLighting;
			payload.brdf = brdf;
			payload.emissive = emissiveSample * material.emissiveFactor;
			payload.rayOrigin = offsetPositionAlongNormal(worldPosition, n);
			payload.hitBackground = false;
			payload.dontAccumulate = false;
		}
	)GLSL";
	const std::vector<uint32_t> rayClosestHitShaderSpv = compileShader(rayClosestHitShaderCode, ShaderType::RayClosestHit);

	VkShaderModule rayClosestHitShaderModule;
	VkShaderModuleCreateInfo rayClosestHitShaderModuleCreateInfo = {};
	rayClosestHitShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	rayClosestHitShaderModuleCreateInfo.pNext = nullptr;
	rayClosestHitShaderModuleCreateInfo.flags = 0;
	rayClosestHitShaderModuleCreateInfo.codeSize = rayClosestHitShaderSpv.size() * sizeof(uint32_t);
	rayClosestHitShaderModuleCreateInfo.pCode = rayClosestHitShaderSpv.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &rayClosestHitShaderModuleCreateInfo, nullptr, &rayClosestHitShaderModule));

	VkPipelineShaderStageCreateInfo rayClosestHitShaderStageCreateInfo = {};
	rayClosestHitShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	rayClosestHitShaderStageCreateInfo.pNext = nullptr;
	rayClosestHitShaderStageCreateInfo.flags = 0;
	rayClosestHitShaderStageCreateInfo.stage = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
	rayClosestHitShaderStageCreateInfo.module = rayClosestHitShaderModule;
	rayClosestHitShaderStageCreateInfo.pName = "main";
	rayClosestHitShaderStageCreateInfo.pSpecializationInfo = nullptr;

	VkRayTracingShaderGroupCreateInfoKHR rayClosestHitShaderGroupCreateInfo = {};
	rayClosestHitShaderGroupCreateInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
	rayClosestHitShaderGroupCreateInfo.pNext = nullptr;
	rayClosestHitShaderGroupCreateInfo.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
	rayClosestHitShaderGroupCreateInfo.generalShader = VK_SHADER_UNUSED_KHR;
	rayClosestHitShaderGroupCreateInfo.closestHitShader = 3;
	rayClosestHitShaderGroupCreateInfo.anyHitShader = VK_SHADER_UNUSED_KHR;
	rayClosestHitShaderGroupCreateInfo.intersectionShader = VK_SHADER_UNUSED_KHR;
	rayClosestHitShaderGroupCreateInfo.pShaderGroupCaptureReplayHandle = nullptr;

	VkPushConstantRange raygenPushConstantRange = {};
	raygenPushConstantRange.stageFlags = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
	raygenPushConstantRange.offset = 0;
	raygenPushConstantRange.size = sizeof(uint32_t);

	VkPushConstantRange missPushConstantRange = {};
	missPushConstantRange.stageFlags = VK_SHADER_STAGE_MISS_BIT_KHR;
	missPushConstantRange.offset = sizeof(Math::vec4);
	missPushConstantRange.size = sizeof(Math::vec4);

	std::array<VkPushConstantRange, 2> pushConstantRanges = { raygenPushConstantRange, missPushConstantRange };
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.pNext = nullptr;
	pipelineLayoutCreateInfo.flags = 0;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &m_descriptorSetLayout;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 2;
	pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges.data();
	NTSHENGN_VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_rayTracingPipelineLayout));

	std::array<VkPipelineShaderStageCreateInfo, 4> shaderStageCreateInfos = { rayGenShaderStageCreateInfo, rayMissShaderStageCreateInfo, rayShadowMissShaderStageCreateInfo, rayClosestHitShaderStageCreateInfo };
	std::array<VkRayTracingShaderGroupCreateInfoKHR, 4> shaderGroupCreateInfos = { rayGenShaderGroupCreateInfo, rayMissShaderGroupCreateInfo, rayShadowMissShaderGroupCreateInfo, rayClosestHitShaderGroupCreateInfo };

	VkRayTracingPipelineCreateInfoKHR rayTracingPipelineCreateInfo = {};
	rayTracingPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
	rayTracingPipelineCreateInfo.pNext = nullptr;
	rayTracingPipelineCreateInfo.flags = 0;
	rayTracingPipelineCreateInfo.stageCount = static_cast<uint32_t>(shaderStageCreateInfos.size());
	rayTracingPipelineCreateInfo.pStages = shaderStageCreateInfos.data();
	rayTracingPipelineCreateInfo.groupCount = static_cast<uint32_t>(shaderGroupCreateInfos.size());
	rayTracingPipelineCreateInfo.pGroups = shaderGroupCreateInfos.data();
	rayTracingPipelineCreateInfo.maxPipelineRayRecursionDepth = 2;
	rayTracingPipelineCreateInfo.pLibraryInfo = nullptr;
	rayTracingPipelineCreateInfo.pLibraryInterface = nullptr;
	rayTracingPipelineCreateInfo.pDynamicState = nullptr;
	rayTracingPipelineCreateInfo.layout = m_rayTracingPipelineLayout;
	rayTracingPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	rayTracingPipelineCreateInfo.basePipelineIndex = 0;
	NTSHENGN_VK_CHECK(m_vkCreateRayTracingPipelinesKHR(m_device, {}, {}, 1, &rayTracingPipelineCreateInfo, nullptr, &m_rayTracingPipeline));

	vkDestroyShaderModule(m_device, rayGenShaderModule, nullptr);
	vkDestroyShaderModule(m_device, rayMissShaderModule, nullptr);
	vkDestroyShaderModule(m_device, rayShadowMissShaderModule, nullptr);
	vkDestroyShaderModule(m_device, rayClosestHitShaderModule, nullptr);
}

void NtshEngn::GraphicsModule::createRayTracingShaderBindingTable() {
	uint32_t missShaderCount = 2;
	uint32_t hitShaderCount = 1;
	uint32_t callShaderCount = 0;
	uint32_t shaderHandleCount = 1 + missShaderCount + hitShaderCount + callShaderCount;

	uint32_t shaderGroupHandleSizeAligned = (m_rayTracingPipelineShaderGroupHandleSize + (m_rayTracingPipelineShaderGroupHandleAlignment - 1)) & ~(m_rayTracingPipelineShaderGroupHandleAlignment - 1);
	uint32_t shaderGroupBaseAligned = (m_rayTracingPipelineShaderGroupHandleSize + (m_rayTracingPipelineShaderGroupBaseAlignment - 1)) & ~(m_rayTracingPipelineShaderGroupBaseAlignment - 1);

	m_rayGenRegion.stride = shaderGroupBaseAligned;
	m_rayGenRegion.size = m_rayGenRegion.stride;

	m_rayMissRegion.stride = shaderGroupHandleSizeAligned;
	m_rayMissRegion.size = ((missShaderCount * shaderGroupHandleSizeAligned) + (m_rayTracingPipelineShaderGroupBaseAlignment - 1)) & ~(m_rayTracingPipelineShaderGroupBaseAlignment - 1);

	m_rayHitRegion.stride = shaderGroupHandleSizeAligned;
	m_rayHitRegion.size = ((hitShaderCount * shaderGroupHandleSizeAligned) + (m_rayTracingPipelineShaderGroupBaseAlignment - 1)) & ~(m_rayTracingPipelineShaderGroupBaseAlignment - 1);

	m_rayCallRegion.stride = shaderGroupHandleSizeAligned;
	m_rayCallRegion.size = ((callShaderCount * shaderGroupHandleSizeAligned) + (m_rayTracingPipelineShaderGroupBaseAlignment - 1)) & ~(m_rayTracingPipelineShaderGroupBaseAlignment - 1);

	size_t dataSize = static_cast<size_t>(shaderHandleCount) * m_rayTracingPipelineShaderGroupHandleSize;
	std::vector<uint8_t> shaderHandles(dataSize);
	NTSHENGN_VK_CHECK(m_vkGetRayTracingShaderGroupHandlesKHR(m_device, m_rayTracingPipeline, 0, shaderHandleCount, dataSize, shaderHandles.data()));

	VkDeviceSize shaderBindingTableSize = m_rayGenRegion.size + m_rayMissRegion.size + m_rayHitRegion.size + m_rayCallRegion.size;
	VkBufferCreateInfo rayTracingShaderBindingTableBufferCreateInfo = {};
	rayTracingShaderBindingTableBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	rayTracingShaderBindingTableBufferCreateInfo.pNext = nullptr;
	rayTracingShaderBindingTableBufferCreateInfo.flags = 0;
	rayTracingShaderBindingTableBufferCreateInfo.size = shaderBindingTableSize;
	rayTracingShaderBindingTableBufferCreateInfo.usage = VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT_KHR;
	rayTracingShaderBindingTableBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	rayTracingShaderBindingTableBufferCreateInfo.queueFamilyIndexCount = 1;
	rayTracingShaderBindingTableBufferCreateInfo.pQueueFamilyIndices = &m_graphicsQueueFamilyIndex;

	VmaAllocationCreateInfo rayTracingShaderBindingTableBufferAllocationCreateInfo = {};
	rayTracingShaderBindingTableBufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	rayTracingShaderBindingTableBufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
	NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &rayTracingShaderBindingTableBufferCreateInfo, &rayTracingShaderBindingTableBufferAllocationCreateInfo, &m_rayTracingShaderBindingTableBuffer, &m_rayTracingShaderBindingTableBufferAllocation, nullptr));

	VkBufferDeviceAddressInfoKHR rayTracingShaderBindingTableBufferDeviceAddressInfo = {};
	rayTracingShaderBindingTableBufferDeviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO_KHR;
	rayTracingShaderBindingTableBufferDeviceAddressInfo.pNext = nullptr;
	rayTracingShaderBindingTableBufferDeviceAddressInfo.buffer = m_rayTracingShaderBindingTableBuffer;
	m_rayTracingShaderBindingTableBufferDeviceAddress = m_vkGetBufferDeviceAddressKHR(m_device, &rayTracingShaderBindingTableBufferDeviceAddressInfo);

	m_rayGenRegion.deviceAddress = m_rayTracingShaderBindingTableBufferDeviceAddress;
	m_rayMissRegion.deviceAddress = m_rayTracingShaderBindingTableBufferDeviceAddress + m_rayGenRegion.size;
	m_rayHitRegion.deviceAddress = m_rayTracingShaderBindingTableBufferDeviceAddress + m_rayGenRegion.size + m_rayMissRegion.size;
	m_rayCallRegion.deviceAddress = 0;

	void* data;
	NTSHENGN_VK_CHECK(vmaMapMemory(m_allocator, m_rayTracingShaderBindingTableBufferAllocation, &data));
	memcpy(data, shaderHandles.data(), m_rayTracingPipelineShaderGroupHandleSize); // Ray gen
	size_t currentShaderHandle = 1;
	for (size_t i = 0; i < missShaderCount; i++) { // Ray miss
		memcpy(reinterpret_cast<char*>(data) + m_rayGenRegion.size + (m_rayMissRegion.stride * i), shaderHandles.data() + currentShaderHandle * m_rayTracingPipelineShaderGroupHandleSize, m_rayTracingPipelineShaderGroupHandleSize);
		currentShaderHandle++;
	}
	for (size_t i = 0; i < hitShaderCount; i++) { // Ray hit
		memcpy(reinterpret_cast<char*>(data) + m_rayGenRegion.size + m_rayMissRegion.size + (m_rayHitRegion.stride * i), shaderHandles.data() + currentShaderHandle * m_rayTracingPipelineShaderGroupHandleSize, m_rayTracingPipelineShaderGroupHandleSize);
		currentShaderHandle++;
	}
	for (size_t i = 0; i < callShaderCount; i++) { // Ray call
		memcpy(reinterpret_cast<char*>(data) + m_rayGenRegion.size + m_rayMissRegion.size + m_rayHitRegion.size + (m_rayCallRegion.stride * i), shaderHandles.data() + currentShaderHandle * m_rayTracingPipelineShaderGroupHandleSize, m_rayTracingPipelineShaderGroupHandleSize);
		currentShaderHandle++;
	}
	vmaUnmapMemory(m_allocator, m_rayTracingShaderBindingTableBufferAllocation);
}

void NtshEngn::GraphicsModule::createDescriptorSets() {
	// Create descriptor pool
	VkDescriptorPoolSize colorImageDescriptorPoolSize = {};
	colorImageDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
	colorImageDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize tlasDescriptorPoolSize = {};
	tlasDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
	tlasDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize cameraDescriptorPoolSize = {};
	cameraDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	cameraDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize objectsDescriptorPoolSize = {};
	objectsDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	objectsDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize meshDescriptorPoolSize = {};
	meshDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	meshDescriptorPoolSize.descriptorCount = m_framesInFlight;
	
	VkDescriptorPoolSize materialsDescriptorPoolSize = {};
	materialsDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	materialsDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize lightsDescriptorPoolSize = {};
	lightsDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	lightsDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize texturesDescriptorPoolSize = {};
	texturesDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	texturesDescriptorPoolSize.descriptorCount = 131072 * m_framesInFlight;

	std::array<VkDescriptorPoolSize, 8> descriptorPoolSizes = { colorImageDescriptorPoolSize, tlasDescriptorPoolSize, cameraDescriptorPoolSize, objectsDescriptorPoolSize, meshDescriptorPoolSize, materialsDescriptorPoolSize, lightsDescriptorPoolSize, texturesDescriptorPoolSize };
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext = nullptr;
	descriptorPoolCreateInfo.flags = 0;
	descriptorPoolCreateInfo.maxSets = m_framesInFlight;
	descriptorPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size());
	descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes.data();
	NTSHENGN_VK_CHECK(vkCreateDescriptorPool(m_device, &descriptorPoolCreateInfo, nullptr, &m_descriptorPool));

	// Allocate descriptor sets
	m_descriptorSets.resize(m_framesInFlight);
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
		descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocateInfo.pNext = nullptr;
		descriptorSetAllocateInfo.descriptorPool = m_descriptorPool;
		descriptorSetAllocateInfo.descriptorSetCount = 1;
		descriptorSetAllocateInfo.pSetLayouts = &m_descriptorSetLayout;
		NTSHENGN_VK_CHECK(vkAllocateDescriptorSets(m_device, &descriptorSetAllocateInfo, &m_descriptorSets[i]));
	}

	// Update descriptor sets
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		std::vector<VkWriteDescriptorSet> writeDescriptorSets;
		VkDescriptorImageInfo colorImageDescriptorImageInfo;
		VkDescriptorBufferInfo cameraDescriptorBufferInfo;
		VkDescriptorBufferInfo objectsDescriptorBufferInfo;
		VkDescriptorBufferInfo meshDescriptorBufferInfo;
		VkDescriptorBufferInfo materialsDescriptorBufferInfo;
		VkDescriptorBufferInfo lightsDescriptorBufferInfo;

		colorImageDescriptorImageInfo.sampler = VK_NULL_HANDLE;
		colorImageDescriptorImageInfo.imageView = m_colorImageView;
		colorImageDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		VkWriteDescriptorSet colorImageDescriptorWriteDescriptorSet = {};
		colorImageDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		colorImageDescriptorWriteDescriptorSet.pNext = nullptr;
		colorImageDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		colorImageDescriptorWriteDescriptorSet.dstBinding = 0;
		colorImageDescriptorWriteDescriptorSet.dstArrayElement = 0;
		colorImageDescriptorWriteDescriptorSet.descriptorCount = 1;
		colorImageDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		colorImageDescriptorWriteDescriptorSet.pImageInfo = &colorImageDescriptorImageInfo;
		colorImageDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
		colorImageDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;
		writeDescriptorSets.push_back(colorImageDescriptorWriteDescriptorSet);

		VkWriteDescriptorSetAccelerationStructureKHR tlasWriteDescriptorSetAccelerationStructure;
		tlasWriteDescriptorSetAccelerationStructure.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR;
		tlasWriteDescriptorSetAccelerationStructure.pNext = nullptr;
		tlasWriteDescriptorSetAccelerationStructure.accelerationStructureCount = 1;
		tlasWriteDescriptorSetAccelerationStructure.pAccelerationStructures = &m_topLevelAccelerationStructure;

		VkWriteDescriptorSet tlasWriteDescriptorSet;
		tlasWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		tlasWriteDescriptorSet.pNext = &tlasWriteDescriptorSetAccelerationStructure;
		tlasWriteDescriptorSet.dstSet = m_descriptorSets[i];
		tlasWriteDescriptorSet.dstBinding = 1;
		tlasWriteDescriptorSet.dstArrayElement = 0;
		tlasWriteDescriptorSet.descriptorCount = 1;
		tlasWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
		tlasWriteDescriptorSet.pImageInfo = nullptr;
		tlasWriteDescriptorSet.pBufferInfo = nullptr;
		tlasWriteDescriptorSet.pTexelBufferView = nullptr;
		writeDescriptorSets.push_back(tlasWriteDescriptorSet);

		cameraDescriptorBufferInfo.buffer = m_cameraBuffers[i].handle;
		cameraDescriptorBufferInfo.offset = 0;
		cameraDescriptorBufferInfo.range = sizeof(Math::mat4) * 2 + sizeof(Math::vec4);

		VkWriteDescriptorSet cameraDescriptorWriteDescriptorSet = {};
		cameraDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		cameraDescriptorWriteDescriptorSet.pNext = nullptr;
		cameraDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		cameraDescriptorWriteDescriptorSet.dstBinding = 2;
		cameraDescriptorWriteDescriptorSet.dstArrayElement = 0;
		cameraDescriptorWriteDescriptorSet.descriptorCount = 1;
		cameraDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		cameraDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		cameraDescriptorWriteDescriptorSet.pBufferInfo = &cameraDescriptorBufferInfo;
		cameraDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;
		writeDescriptorSets.push_back(cameraDescriptorWriteDescriptorSet);

		objectsDescriptorBufferInfo.buffer = m_objectBuffers[i].handle;
		objectsDescriptorBufferInfo.offset = 0;
		objectsDescriptorBufferInfo.range = 32768;

		VkWriteDescriptorSet objectsDescriptorWriteDescriptorSet = {};
		objectsDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		objectsDescriptorWriteDescriptorSet.pNext = nullptr;
		objectsDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		objectsDescriptorWriteDescriptorSet.dstBinding = 3;
		objectsDescriptorWriteDescriptorSet.dstArrayElement = 0;
		objectsDescriptorWriteDescriptorSet.descriptorCount = 1;
		objectsDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		objectsDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		objectsDescriptorWriteDescriptorSet.pBufferInfo = &objectsDescriptorBufferInfo;
		objectsDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;
		writeDescriptorSets.push_back(objectsDescriptorWriteDescriptorSet);

		meshDescriptorBufferInfo.buffer = m_meshBuffers[i].handle;
		meshDescriptorBufferInfo.offset = 0;
		meshDescriptorBufferInfo.range = 32768;

		VkWriteDescriptorSet meshDescriptorWriteDescriptorSet = {};
		meshDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		meshDescriptorWriteDescriptorSet.pNext = nullptr;
		meshDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		meshDescriptorWriteDescriptorSet.dstBinding = 4;
		meshDescriptorWriteDescriptorSet.dstArrayElement = 0;
		meshDescriptorWriteDescriptorSet.descriptorCount = 1;
		meshDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		meshDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		meshDescriptorWriteDescriptorSet.pBufferInfo = &meshDescriptorBufferInfo;
		meshDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;
		writeDescriptorSets.push_back(meshDescriptorWriteDescriptorSet);

		materialsDescriptorBufferInfo.buffer = m_materialBuffers[i].handle;
		materialsDescriptorBufferInfo.offset = 0;
		materialsDescriptorBufferInfo.range = 32768;

		VkWriteDescriptorSet materialsDescriptorWriteDescriptorSet = {};
		materialsDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		materialsDescriptorWriteDescriptorSet.pNext = nullptr;
		materialsDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		materialsDescriptorWriteDescriptorSet.dstBinding = 5;
		materialsDescriptorWriteDescriptorSet.dstArrayElement = 0;
		materialsDescriptorWriteDescriptorSet.descriptorCount = 1;
		materialsDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		materialsDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		materialsDescriptorWriteDescriptorSet.pBufferInfo = &materialsDescriptorBufferInfo;
		materialsDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;
		writeDescriptorSets.push_back(materialsDescriptorWriteDescriptorSet);

		lightsDescriptorBufferInfo.buffer = m_lightBuffers[i].handle;
		lightsDescriptorBufferInfo.offset = 0;
		lightsDescriptorBufferInfo.range = 32768;

		VkWriteDescriptorSet lightsDescriptorWriteDescriptorSet = {};
		lightsDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		lightsDescriptorWriteDescriptorSet.pNext = nullptr;
		lightsDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		lightsDescriptorWriteDescriptorSet.dstBinding = 6;
		lightsDescriptorWriteDescriptorSet.dstArrayElement = 0;
		lightsDescriptorWriteDescriptorSet.descriptorCount = 1;
		lightsDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		lightsDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		lightsDescriptorWriteDescriptorSet.pBufferInfo = &lightsDescriptorBufferInfo;
		lightsDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;
		writeDescriptorSets.push_back(lightsDescriptorWriteDescriptorSet);

		vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}

	m_descriptorSetsNeedUpdate.resize(m_framesInFlight);
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		m_descriptorSetsNeedUpdate[i] = false;
	}
}

void NtshEngn::GraphicsModule::updateDescriptorSet(uint32_t frameInFlight) {
	std::vector<VkDescriptorImageInfo> texturesDescriptorImageInfos(m_textures.size());
	for (size_t i = 0; i < m_textures.size(); i++) {
		texturesDescriptorImageInfos[i].sampler = m_textureSamplers[m_textures[i].samplerKey];
		texturesDescriptorImageInfos[i].imageView = m_textureImageViews[m_textures[i].imageID];
		texturesDescriptorImageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}

	VkWriteDescriptorSet texturesDescriptorWriteDescriptorSet = {};
	texturesDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	texturesDescriptorWriteDescriptorSet.pNext = nullptr;
	texturesDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[frameInFlight];
	texturesDescriptorWriteDescriptorSet.dstBinding = 7;
	texturesDescriptorWriteDescriptorSet.dstArrayElement = 0;
	texturesDescriptorWriteDescriptorSet.descriptorCount = static_cast<uint32_t>(texturesDescriptorImageInfos.size());
	texturesDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	texturesDescriptorWriteDescriptorSet.pImageInfo = texturesDescriptorImageInfos.data();
	texturesDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
	texturesDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

	vkUpdateDescriptorSets(m_device, 1, &texturesDescriptorWriteDescriptorSet, 0, nullptr);
}

void NtshEngn::GraphicsModule::createToneMappingResources() {
	// Create descriptor set layout
	VkDescriptorSetLayoutBinding colorImageDescriptorSetLayoutBinding = {};
	colorImageDescriptorSetLayoutBinding.binding = 0;
	colorImageDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	colorImageDescriptorSetLayoutBinding.descriptorCount = 1;
	colorImageDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	colorImageDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.pNext = nullptr;
	descriptorSetLayoutCreateInfo.flags = 0;
	descriptorSetLayoutCreateInfo.bindingCount = 1;
	descriptorSetLayoutCreateInfo.pBindings = &colorImageDescriptorSetLayoutBinding;
	NTSHENGN_VK_CHECK(vkCreateDescriptorSetLayout(m_device, &descriptorSetLayoutCreateInfo, nullptr, &m_toneMappingDescriptorSetLayout));

	// Create graphics pipeline
	VkFormat pipelineRenderingColorFormat = m_drawImageFormat;

	VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {};
	pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	pipelineRenderingCreateInfo.pNext = nullptr;
	pipelineRenderingCreateInfo.viewMask = 0;
	pipelineRenderingCreateInfo.colorAttachmentCount = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &pipelineRenderingColorFormat;
	pipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
	pipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

	const std::string vertexShaderCode = R"GLSL(
		#version 460

		layout(location = 0) out vec2 outUv;

		void main() {
			outUv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
			gl_Position = vec4(outUv * 2.0 + -1.0, 0.0, 1.0);
		}
	)GLSL";
	const std::vector<uint32_t> vertexShaderSpv = compileShader(vertexShaderCode, ShaderType::Vertex);

	VkShaderModule vertexShaderModule;
	VkShaderModuleCreateInfo vertexShaderModuleCreateInfo = {};
	vertexShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertexShaderModuleCreateInfo.pNext = nullptr;
	vertexShaderModuleCreateInfo.flags = 0;
	vertexShaderModuleCreateInfo.codeSize = vertexShaderSpv.size() * sizeof(uint32_t);
	vertexShaderModuleCreateInfo.pCode = vertexShaderSpv.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &vertexShaderModuleCreateInfo, nullptr, &vertexShaderModule));

	VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo = {};
	vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderStageCreateInfo.pNext = nullptr;
	vertexShaderStageCreateInfo.flags = 0;
	vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexShaderStageCreateInfo.module = vertexShaderModule;
	vertexShaderStageCreateInfo.pName = "main";
	vertexShaderStageCreateInfo.pSpecializationInfo = nullptr;

	const std::string fragmentShaderCode = R"GLSL(
		#version 460

		layout(set = 0, binding = 0) uniform sampler2D imageSampler;

		layout(location = 0) in vec2 uv;

		layout(location = 0) out vec4 outColor;

		void main() {
			vec4 color = texture(imageSampler, uv);
			color.rgb /= color.rgb + vec3(1.0);

			outColor = color;
		}
	)GLSL";
	const std::vector<uint32_t> fragmentShaderSpv = compileShader(fragmentShaderCode, ShaderType::Fragment);

	VkShaderModule fragmentShaderModule;
	VkShaderModuleCreateInfo fragmentShaderModuleCreateInfo = {};
	fragmentShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	fragmentShaderModuleCreateInfo.pNext = nullptr;
	fragmentShaderModuleCreateInfo.flags = 0;
	fragmentShaderModuleCreateInfo.codeSize = fragmentShaderSpv.size() * sizeof(uint32_t);
	fragmentShaderModuleCreateInfo.pCode = fragmentShaderSpv.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &fragmentShaderModuleCreateInfo, nullptr, &fragmentShaderModule));

	VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo = {};
	fragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderStageCreateInfo.pNext = nullptr;
	fragmentShaderStageCreateInfo.flags = 0;
	fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentShaderStageCreateInfo.module = fragmentShaderModule;
	fragmentShaderStageCreateInfo.pName = "main";
	fragmentShaderStageCreateInfo.pSpecializationInfo = nullptr;

	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStageCreateInfos = { vertexShaderStageCreateInfo, fragmentShaderStageCreateInfo };

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
	vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateCreateInfo.pNext = nullptr;
	vertexInputStateCreateInfo.flags = 0;
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexBindingDescriptions = nullptr;
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = nullptr;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {};
	inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyStateCreateInfo.pNext = nullptr;
	inputAssemblyStateCreateInfo.flags = 0;
	inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.pNext = nullptr;
	viewportStateCreateInfo.flags = 0;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &m_viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &m_scissor;

	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {};
	rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateCreateInfo.pNext = nullptr;
	rasterizationStateCreateInfo.flags = 0;
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
	rasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
	rasterizationStateCreateInfo.depthBiasClamp = 0.0f;
	rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;
	rasterizationStateCreateInfo.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {};
	multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleStateCreateInfo.pNext = nullptr;
	multisampleStateCreateInfo.flags = 0;
	multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
	multisampleStateCreateInfo.minSampleShading = 0.0f;
	multisampleStateCreateInfo.pSampleMask = nullptr;
	multisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
	multisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;

	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {};
	depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilStateCreateInfo.pNext = nullptr;
	depthStencilStateCreateInfo.flags = 0;
	depthStencilStateCreateInfo.depthTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.depthWriteEnable = VK_FALSE;
	depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_NEVER;
	depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.front = {};
	depthStencilStateCreateInfo.back = {};
	depthStencilStateCreateInfo.minDepthBounds = 0.0f;
	depthStencilStateCreateInfo.maxDepthBounds = 1.0f;

	VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
	colorBlendAttachmentState.blendEnable = VK_FALSE;
	colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {};
	colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendStateCreateInfo.pNext = nullptr;
	colorBlendStateCreateInfo.flags = 0;
	colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
	colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	colorBlendStateCreateInfo.attachmentCount = 1;
	colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;

	std::array<VkDynamicState, 2> dynamicStates = { VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_VIEWPORT };
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.pNext = nullptr;
	dynamicStateCreateInfo.flags = 0;
	dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.pNext = nullptr;
	pipelineLayoutCreateInfo.flags = 0;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &m_toneMappingDescriptorSetLayout;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;
	NTSHENGN_VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_toneMappingGraphicsPipelineLayout));

	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
	graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPipelineCreateInfo.pNext = &pipelineRenderingCreateInfo;
	graphicsPipelineCreateInfo.flags = 0;
	graphicsPipelineCreateInfo.stageCount = 2;
	graphicsPipelineCreateInfo.pStages = shaderStageCreateInfos.data();
	graphicsPipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
	graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
	graphicsPipelineCreateInfo.pTessellationState = nullptr;
	graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	graphicsPipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
	graphicsPipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
	graphicsPipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	graphicsPipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	graphicsPipelineCreateInfo.layout = m_toneMappingGraphicsPipelineLayout;
	graphicsPipelineCreateInfo.renderPass = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.subpass = 0;
	graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.basePipelineIndex = 0;
	NTSHENGN_VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &m_toneMappingGraphicsPipeline));

	vkDestroyShaderModule(m_device, vertexShaderModule, nullptr);
	vkDestroyShaderModule(m_device, fragmentShaderModule, nullptr);

	// Create sampler
	VkSamplerCreateInfo samplerCreateInfo = {};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.pNext = nullptr;
	samplerCreateInfo.flags = 0;
	samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
	samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.mipLodBias = 0.0f;
	samplerCreateInfo.anisotropyEnable = VK_FALSE;
	samplerCreateInfo.maxAnisotropy = 0.0f;
	samplerCreateInfo.compareEnable = VK_FALSE;
	samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
	samplerCreateInfo.minLod = 0.0f;
	samplerCreateInfo.maxLod = 0.0f;
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
	NTSHENGN_VK_CHECK(vkCreateSampler(m_device, &samplerCreateInfo, nullptr, &m_toneMappingSampler));

	// Create descriptor pool
	VkDescriptorPoolSize colorImageDescriptorPoolSize = {};
	colorImageDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	colorImageDescriptorPoolSize.descriptorCount = 1;

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext = nullptr;
	descriptorPoolCreateInfo.flags = 0;
	descriptorPoolCreateInfo.maxSets = 1;
	descriptorPoolCreateInfo.poolSizeCount = 1;
	descriptorPoolCreateInfo.pPoolSizes = &colorImageDescriptorPoolSize;
	NTSHENGN_VK_CHECK(vkCreateDescriptorPool(m_device, &descriptorPoolCreateInfo, nullptr, &m_toneMappingDescriptorPool));

	// Allocate descriptor set
	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
	descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetAllocateInfo.pNext = nullptr;
	descriptorSetAllocateInfo.descriptorPool = m_toneMappingDescriptorPool;
	descriptorSetAllocateInfo.descriptorSetCount = 1;
	descriptorSetAllocateInfo.pSetLayouts = &m_toneMappingDescriptorSetLayout;
	NTSHENGN_VK_CHECK(vkAllocateDescriptorSets(m_device, &descriptorSetAllocateInfo, &m_toneMappingDescriptorSet));

	// Update descriptor set
	VkDescriptorImageInfo colorImageDescriptorImageInfo;
	colorImageDescriptorImageInfo.sampler = m_toneMappingSampler;
	colorImageDescriptorImageInfo.imageView = m_colorImageView;
	colorImageDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkWriteDescriptorSet colorImageDescriptorWriteDescriptorSet = {};
	colorImageDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	colorImageDescriptorWriteDescriptorSet.pNext = nullptr;
	colorImageDescriptorWriteDescriptorSet.dstSet = m_toneMappingDescriptorSet;
	colorImageDescriptorWriteDescriptorSet.dstBinding = 0;
	colorImageDescriptorWriteDescriptorSet.dstArrayElement = 0;
	colorImageDescriptorWriteDescriptorSet.descriptorCount = 1;
	colorImageDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	colorImageDescriptorWriteDescriptorSet.pImageInfo = &colorImageDescriptorImageInfo;
	colorImageDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
	colorImageDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

	vkUpdateDescriptorSets(m_device, 1, &colorImageDescriptorWriteDescriptorSet, 0, nullptr);
}

void NtshEngn::GraphicsModule::createUIResources() {
	// Create samplers
	VkSamplerCreateInfo samplerCreateInfo = {};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.pNext = nullptr;
	samplerCreateInfo.flags = 0;
	samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
	samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.mipLodBias = 0.0f;
	samplerCreateInfo.anisotropyEnable = VK_FALSE;
	samplerCreateInfo.maxAnisotropy = 0.0f;
	samplerCreateInfo.compareEnable = VK_FALSE;
	samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
	samplerCreateInfo.minLod = 0.0f;
	samplerCreateInfo.maxLod = VK_LOD_CLAMP_NONE;
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
	NTSHENGN_VK_CHECK(vkCreateSampler(m_device, &samplerCreateInfo, nullptr, &m_uiNearestSampler));

	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	NTSHENGN_VK_CHECK(vkCreateSampler(m_device, &samplerCreateInfo, nullptr, &m_uiLinearSampler));

	createUITextResources();
	createUILineResources();
	createUIRectangleResources();
	createUIImageResources();
}

void NtshEngn::GraphicsModule::createUITextResources() {
	// Create text buffers
	m_uiTextBuffers.resize(m_framesInFlight);
	VkBufferCreateInfo uiTextBufferCreateInfo = {};
	uiTextBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	uiTextBufferCreateInfo.pNext = nullptr;
	uiTextBufferCreateInfo.flags = 0;
	uiTextBufferCreateInfo.size = 32768;
	uiTextBufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	uiTextBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	uiTextBufferCreateInfo.queueFamilyIndexCount = 1;
	uiTextBufferCreateInfo.pQueueFamilyIndices = &m_graphicsQueueFamilyIndex;

	VmaAllocationInfo bufferAllocationInfo;

	VmaAllocationCreateInfo bufferAllocationCreateInfo = {};
	bufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
	bufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &uiTextBufferCreateInfo, &bufferAllocationCreateInfo, &m_uiTextBuffers[i].handle, &m_uiTextBuffers[i].allocation, &bufferAllocationInfo));
		m_uiTextBuffers[i].address = bufferAllocationInfo.pMappedData;
	}

	// Create descriptor set layout
	VkDescriptorSetLayoutBinding textBufferDescriptorSetLayoutBinding = {};
	textBufferDescriptorSetLayoutBinding.binding = 0;
	textBufferDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	textBufferDescriptorSetLayoutBinding.descriptorCount = 1;
	textBufferDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	textBufferDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding fontsDescriptorSetLayoutBinding = {};
	fontsDescriptorSetLayoutBinding.binding = 1;
	fontsDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	fontsDescriptorSetLayoutBinding.descriptorCount = 131072;
	fontsDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	fontsDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	std::array<VkDescriptorBindingFlags, 2> descriptorBindingFlags = { 0, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT };
	VkDescriptorSetLayoutBindingFlagsCreateInfo descriptorSetLayoutBindingFlagsCreateInfo = {};
	descriptorSetLayoutBindingFlagsCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	descriptorSetLayoutBindingFlagsCreateInfo.pNext = nullptr;
	descriptorSetLayoutBindingFlagsCreateInfo.bindingCount = static_cast<uint32_t>(descriptorBindingFlags.size());
	descriptorSetLayoutBindingFlagsCreateInfo.pBindingFlags = descriptorBindingFlags.data();

	std::array<VkDescriptorSetLayoutBinding, 2> descriptorSetLayoutBindings = { textBufferDescriptorSetLayoutBinding, fontsDescriptorSetLayoutBinding };
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.pNext = &descriptorSetLayoutBindingFlagsCreateInfo;
	descriptorSetLayoutCreateInfo.flags = 0;
	descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(descriptorSetLayoutBindings.size());
	descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings.data();
	NTSHENGN_VK_CHECK(vkCreateDescriptorSetLayout(m_device, &descriptorSetLayoutCreateInfo, nullptr, &m_uiTextDescriptorSetLayout));

	// Create graphics pipeline
	VkFormat pipelineRenderingColorFormat = m_drawImageFormat;

	VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {};
	pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	pipelineRenderingCreateInfo.pNext = nullptr;
	pipelineRenderingCreateInfo.viewMask = 0;
	pipelineRenderingCreateInfo.colorAttachmentCount = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &pipelineRenderingColorFormat;
	pipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
	pipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

	const std::string vertexShaderCode = R"GLSL(
		#version 460

		struct CharacterInfo {
			vec2 positionTopLeft;
			vec2 positionBottomRight;
			vec2 uvTopLeft;
			vec2 uvBottomRight;
		};

		layout(std430, set = 0, binding = 0) restrict readonly buffer Text {
			CharacterInfo char[];
		} text;

		layout(push_constant) uniform TextBufferInfo {
			uint bufferOffset;
		} tBI;

		layout(location = 0) out vec2 outUv;

		void main() {
			const uint charIndex = tBI.bufferOffset + uint(floor(gl_VertexIndex / 6));
			const uint vertexID = uint(mod(float(gl_VertexIndex), 6.0));
			if ((vertexID == 0) || (vertexID == 3)) {
				gl_Position = vec4(text.char[charIndex].positionBottomRight.x, text.char[charIndex].positionTopLeft.y, 0.0, 1.0);
				outUv = vec2(text.char[charIndex].uvBottomRight.x, text.char[charIndex].uvTopLeft.y);
			}
			else if (vertexID == 1) {
				gl_Position = vec4(text.char[charIndex].positionTopLeft, 0.0, 1.0);
				outUv = text.char[charIndex].uvTopLeft;
			}
			else if ((vertexID == 2) || (vertexID == 4)) {
				gl_Position = vec4(text.char[charIndex].positionTopLeft.x, text.char[charIndex].positionBottomRight.y, 0.0, 1.0);
				outUv = vec2(text.char[charIndex].uvTopLeft.x, text.char[charIndex].uvBottomRight.y);
			}
			else if (vertexID == 5) {
				gl_Position = vec4(text.char[charIndex].positionBottomRight, 0.0, 1.0);
				outUv = text.char[charIndex].uvBottomRight;
			}
		}
	)GLSL";
	const std::vector<uint32_t> vertexShaderSpv = compileShader(vertexShaderCode, ShaderType::Vertex);

	VkShaderModule vertexShaderModule;
	VkShaderModuleCreateInfo vertexShaderModuleCreateInfo = {};
	vertexShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertexShaderModuleCreateInfo.pNext = nullptr;
	vertexShaderModuleCreateInfo.flags = 0;
	vertexShaderModuleCreateInfo.codeSize = vertexShaderSpv.size() * sizeof(uint32_t);
	vertexShaderModuleCreateInfo.pCode = vertexShaderSpv.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &vertexShaderModuleCreateInfo, nullptr, &vertexShaderModule));

	VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo = {};
	vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderStageCreateInfo.pNext = nullptr;
	vertexShaderStageCreateInfo.flags = 0;
	vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexShaderStageCreateInfo.module = vertexShaderModule;
	vertexShaderStageCreateInfo.pName = "main";
	vertexShaderStageCreateInfo.pSpecializationInfo = nullptr;

	const std::string fragmentShaderCode = R"GLSL(
		#version 460
		#extension GL_EXT_nonuniform_qualifier : enable

		layout(set = 0, binding = 1) uniform sampler2D fonts[];

		layout(push_constant) uniform TextInfo {
			layout(offset = 16) vec4 color;
			uint fontID;
		} tI;

		layout(location = 0) in vec2 uv;

		layout(location = 0) out vec4 outColor;

		void main() {
			outColor = vec4(1.0, 1.0, 1.0, texture(fonts[nonuniformEXT(tI.fontID)], uv).r) * tI.color;
		}
	)GLSL";
	const std::vector<uint32_t> fragmentShaderSpv = compileShader(fragmentShaderCode, ShaderType::Fragment);

	VkShaderModule fragmentShaderModule;
	VkShaderModuleCreateInfo fragmentShaderModuleCreateInfo = {};
	fragmentShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	fragmentShaderModuleCreateInfo.pNext = nullptr;
	fragmentShaderModuleCreateInfo.flags = 0;
	fragmentShaderModuleCreateInfo.codeSize = fragmentShaderSpv.size() * sizeof(uint32_t);
	fragmentShaderModuleCreateInfo.pCode = fragmentShaderSpv.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &fragmentShaderModuleCreateInfo, nullptr, &fragmentShaderModule));

	VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo = {};
	fragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderStageCreateInfo.pNext = nullptr;
	fragmentShaderStageCreateInfo.flags = 0;
	fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentShaderStageCreateInfo.module = fragmentShaderModule;
	fragmentShaderStageCreateInfo.pName = "main";
	fragmentShaderStageCreateInfo.pSpecializationInfo = nullptr;

	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStageCreateInfos = { vertexShaderStageCreateInfo, fragmentShaderStageCreateInfo };

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
	vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateCreateInfo.pNext = nullptr;
	vertexInputStateCreateInfo.flags = 0;
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexBindingDescriptions = nullptr;
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = nullptr;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {};
	inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyStateCreateInfo.pNext = nullptr;
	inputAssemblyStateCreateInfo.flags = 0;
	inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.pNext = nullptr;
	viewportStateCreateInfo.flags = 0;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &m_viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &m_scissor;

	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {};
	rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateCreateInfo.pNext = nullptr;
	rasterizationStateCreateInfo.flags = 0;
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
	rasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
	rasterizationStateCreateInfo.depthBiasClamp = 0.0f;
	rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;
	rasterizationStateCreateInfo.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {};
	multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleStateCreateInfo.pNext = nullptr;
	multisampleStateCreateInfo.flags = 0;
	multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
	multisampleStateCreateInfo.minSampleShading = 0.0f;
	multisampleStateCreateInfo.pSampleMask = nullptr;
	multisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
	multisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;

	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {};
	depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilStateCreateInfo.pNext = nullptr;
	depthStencilStateCreateInfo.flags = 0;
	depthStencilStateCreateInfo.depthTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.depthWriteEnable = VK_FALSE;
	depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_NEVER;
	depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.front = {};
	depthStencilStateCreateInfo.back = {};
	depthStencilStateCreateInfo.minDepthBounds = 0.0f;
	depthStencilStateCreateInfo.maxDepthBounds = 1.0f;

	VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
	colorBlendAttachmentState.blendEnable = VK_TRUE;
	colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {};
	colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendStateCreateInfo.pNext = nullptr;
	colorBlendStateCreateInfo.flags = 0;
	colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
	colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	colorBlendStateCreateInfo.attachmentCount = 1;
	colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;

	std::array<VkDynamicState, 2> dynamicStates = { VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_VIEWPORT };
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.pNext = nullptr;
	dynamicStateCreateInfo.flags = 0;
	dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

	VkPushConstantRange vertexPushConstantRange = {};
	vertexPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	vertexPushConstantRange.offset = 0;
	vertexPushConstantRange.size = sizeof(uint32_t);

	VkPushConstantRange fragmentPushConstantRange = {};
	fragmentPushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentPushConstantRange.offset = sizeof(Math::vec4);
	fragmentPushConstantRange.size = sizeof(Math::vec4) + sizeof(uint32_t);

	std::array<VkPushConstantRange, 2> pushConstantRanges = { vertexPushConstantRange, fragmentPushConstantRange };
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.pNext = nullptr;
	pipelineLayoutCreateInfo.flags = 0;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &m_uiTextDescriptorSetLayout;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 2;
	pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges.data();
	NTSHENGN_VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_uiTextGraphicsPipelineLayout));

	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
	graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPipelineCreateInfo.pNext = &pipelineRenderingCreateInfo;
	graphicsPipelineCreateInfo.flags = 0;
	graphicsPipelineCreateInfo.stageCount = 2;
	graphicsPipelineCreateInfo.pStages = shaderStageCreateInfos.data();
	graphicsPipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
	graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
	graphicsPipelineCreateInfo.pTessellationState = nullptr;
	graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	graphicsPipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
	graphicsPipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
	graphicsPipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	graphicsPipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	graphicsPipelineCreateInfo.layout = m_uiTextGraphicsPipelineLayout;
	graphicsPipelineCreateInfo.renderPass = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.subpass = 0;
	graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.basePipelineIndex = 0;
	NTSHENGN_VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &m_uiTextGraphicsPipeline));

	vkDestroyShaderModule(m_device, vertexShaderModule, nullptr);
	vkDestroyShaderModule(m_device, fragmentShaderModule, nullptr);

	// Create descriptor pool
	VkDescriptorPoolSize textBufferDescriptorPoolSize = {};
	textBufferDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	textBufferDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize fontsDescriptorPoolSize = {};
	fontsDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	fontsDescriptorPoolSize.descriptorCount = 131072 * m_framesInFlight;

	std::array<VkDescriptorPoolSize, 2> descriptorPoolSizes = { textBufferDescriptorPoolSize, fontsDescriptorPoolSize };
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext = nullptr;
	descriptorPoolCreateInfo.flags = 0;
	descriptorPoolCreateInfo.maxSets = m_framesInFlight;
	descriptorPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size());
	descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes.data();
	NTSHENGN_VK_CHECK(vkCreateDescriptorPool(m_device, &descriptorPoolCreateInfo, nullptr, &m_uiTextDescriptorPool));

	// Allocate descriptor sets
	m_uiTextDescriptorSets.resize(m_framesInFlight);
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
		descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocateInfo.pNext = nullptr;
		descriptorSetAllocateInfo.descriptorPool = m_uiTextDescriptorPool;
		descriptorSetAllocateInfo.descriptorSetCount = 1;
		descriptorSetAllocateInfo.pSetLayouts = &m_uiTextDescriptorSetLayout;
		NTSHENGN_VK_CHECK(vkAllocateDescriptorSets(m_device, &descriptorSetAllocateInfo, &m_uiTextDescriptorSets[i]));
	}

	// Update descriptor sets
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		VkDescriptorBufferInfo textDescriptorBufferInfo;
		textDescriptorBufferInfo.buffer = m_uiTextBuffers[i].handle;
		textDescriptorBufferInfo.offset = 0;
		textDescriptorBufferInfo.range = 32768;

		VkWriteDescriptorSet textDescriptorWriteDescriptorSet = {};
		textDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		textDescriptorWriteDescriptorSet.pNext = nullptr;
		textDescriptorWriteDescriptorSet.dstSet = m_uiTextDescriptorSets[i];
		textDescriptorWriteDescriptorSet.dstBinding = 0;
		textDescriptorWriteDescriptorSet.dstArrayElement = 0;
		textDescriptorWriteDescriptorSet.descriptorCount = 1;
		textDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		textDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		textDescriptorWriteDescriptorSet.pBufferInfo = &textDescriptorBufferInfo;
		textDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

		vkUpdateDescriptorSets(m_device, 1, &textDescriptorWriteDescriptorSet, 0, nullptr);
	}

	m_uiTextDescriptorSetsNeedUpdate.resize(m_framesInFlight);
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		m_uiTextDescriptorSetsNeedUpdate[i] = false;
	}
}

void NtshEngn::GraphicsModule::updateUITextDescriptorSet(uint32_t frameInFlight) {
	std::vector<VkDescriptorImageInfo> fontsDescriptorImageInfos(m_fonts.size());
	for (size_t i = 0; i < m_fonts.size(); i++) {
		fontsDescriptorImageInfos[i].sampler = (m_fonts[i].filter == ImageSamplerFilter::Nearest) ? m_uiNearestSampler : m_uiLinearSampler;
		fontsDescriptorImageInfos[i].imageView = m_fonts[i].imageView;
		fontsDescriptorImageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}

	VkWriteDescriptorSet fontsDescriptorWriteDescriptorSet = {};
	fontsDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	fontsDescriptorWriteDescriptorSet.pNext = nullptr;
	fontsDescriptorWriteDescriptorSet.dstSet = m_uiTextDescriptorSets[frameInFlight];
	fontsDescriptorWriteDescriptorSet.dstBinding = 1;
	fontsDescriptorWriteDescriptorSet.dstArrayElement = 0;
	fontsDescriptorWriteDescriptorSet.descriptorCount = static_cast<uint32_t>(fontsDescriptorImageInfos.size());
	fontsDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	fontsDescriptorWriteDescriptorSet.pImageInfo = fontsDescriptorImageInfos.data();
	fontsDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
	fontsDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

	vkUpdateDescriptorSets(m_device, 1, &fontsDescriptorWriteDescriptorSet, 0, nullptr);
}

void NtshEngn::GraphicsModule::createUILineResources() {
	// Create graphics pipeline
	VkFormat pipelineRenderingColorFormat = m_drawImageFormat;

	VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {};
	pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	pipelineRenderingCreateInfo.pNext = nullptr;
	pipelineRenderingCreateInfo.viewMask = 0;
	pipelineRenderingCreateInfo.colorAttachmentCount = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &pipelineRenderingColorFormat;
	pipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
	pipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

	const std::string vertexShaderCode = R"GLSL(
		#version 460

		layout(push_constant) uniform LinePositionInfo {
			vec2 start;
			vec2 end;
		} lPI;

		void main() {
			if (gl_VertexIndex == 0) {
				gl_Position = vec4(lPI.start, 0.0, 1.0);
			}
			else {
				gl_Position = vec4(lPI.end, 0.0, 1.0);
			}
		}
	)GLSL";
	const std::vector<uint32_t> vertexShaderSpv = compileShader(vertexShaderCode, ShaderType::Vertex);

	VkShaderModule vertexShaderModule;
	VkShaderModuleCreateInfo vertexShaderModuleCreateInfo = {};
	vertexShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertexShaderModuleCreateInfo.pNext = nullptr;
	vertexShaderModuleCreateInfo.flags = 0;
	vertexShaderModuleCreateInfo.codeSize = vertexShaderSpv.size() * sizeof(uint32_t);
	vertexShaderModuleCreateInfo.pCode = vertexShaderSpv.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &vertexShaderModuleCreateInfo, nullptr, &vertexShaderModule));

	VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo = {};
	vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderStageCreateInfo.pNext = nullptr;
	vertexShaderStageCreateInfo.flags = 0;
	vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexShaderStageCreateInfo.module = vertexShaderModule;
	vertexShaderStageCreateInfo.pName = "main";
	vertexShaderStageCreateInfo.pSpecializationInfo = nullptr;

	const std::string fragmentShaderCode = R"GLSL(
		#version 460

		layout(push_constant) uniform LineColorInfo {
			layout(offset = 16) vec4 color;
		} lCI;

		layout(location = 0) out vec4 outColor;

		void main() {
			outColor = lCI.color;
		}
	)GLSL";
	const std::vector<uint32_t> fragmentShaderSpv = compileShader(fragmentShaderCode, ShaderType::Fragment);

	VkShaderModule fragmentShaderModule;
	VkShaderModuleCreateInfo fragmentShaderModuleCreateInfo = {};
	fragmentShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	fragmentShaderModuleCreateInfo.pNext = nullptr;
	fragmentShaderModuleCreateInfo.flags = 0;
	fragmentShaderModuleCreateInfo.codeSize = fragmentShaderSpv.size() * sizeof(uint32_t);
	fragmentShaderModuleCreateInfo.pCode = fragmentShaderSpv.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &fragmentShaderModuleCreateInfo, nullptr, &fragmentShaderModule));

	VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo = {};
	fragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderStageCreateInfo.pNext = nullptr;
	fragmentShaderStageCreateInfo.flags = 0;
	fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentShaderStageCreateInfo.module = fragmentShaderModule;
	fragmentShaderStageCreateInfo.pName = "main";
	fragmentShaderStageCreateInfo.pSpecializationInfo = nullptr;

	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStageCreateInfos = { vertexShaderStageCreateInfo, fragmentShaderStageCreateInfo };

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
	vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateCreateInfo.pNext = nullptr;
	vertexInputStateCreateInfo.flags = 0;
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexBindingDescriptions = nullptr;
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = nullptr;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {};
	inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyStateCreateInfo.pNext = nullptr;
	inputAssemblyStateCreateInfo.flags = 0;
	inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.pNext = nullptr;
	viewportStateCreateInfo.flags = 0;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &m_viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &m_scissor;

	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {};
	rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateCreateInfo.pNext = nullptr;
	rasterizationStateCreateInfo.flags = 0;
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
	rasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
	rasterizationStateCreateInfo.depthBiasClamp = 0.0f;
	rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;
	rasterizationStateCreateInfo.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {};
	multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleStateCreateInfo.pNext = nullptr;
	multisampleStateCreateInfo.flags = 0;
	multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
	multisampleStateCreateInfo.minSampleShading = 0.0f;
	multisampleStateCreateInfo.pSampleMask = nullptr;
	multisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
	multisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;

	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {};
	depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilStateCreateInfo.pNext = nullptr;
	depthStencilStateCreateInfo.flags = 0;
	depthStencilStateCreateInfo.depthTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.depthWriteEnable = VK_FALSE;
	depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_NEVER;
	depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.front = {};
	depthStencilStateCreateInfo.back = {};
	depthStencilStateCreateInfo.minDepthBounds = 0.0f;
	depthStencilStateCreateInfo.maxDepthBounds = 1.0f;

	VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
	colorBlendAttachmentState.blendEnable = VK_TRUE;
	colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {};
	colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendStateCreateInfo.pNext = nullptr;
	colorBlendStateCreateInfo.flags = 0;
	colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
	colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	colorBlendStateCreateInfo.attachmentCount = 1;
	colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;

	std::array<VkDynamicState, 2> dynamicStates = { VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_VIEWPORT };
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.pNext = nullptr;
	dynamicStateCreateInfo.flags = 0;
	dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

	VkPushConstantRange vertexPushConstantRange = {};
	vertexPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	vertexPushConstantRange.offset = 0;
	vertexPushConstantRange.size = sizeof(Math::vec2) + sizeof(Math::vec2);

	VkPushConstantRange fragmentPushConstantRange = {};
	fragmentPushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentPushConstantRange.offset = sizeof(Math::vec4);
	fragmentPushConstantRange.size = sizeof(Math::vec4);

	std::array<VkPushConstantRange, 2> pushConstantRanges = { vertexPushConstantRange, fragmentPushConstantRange };
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.pNext = nullptr;
	pipelineLayoutCreateInfo.flags = 0;
	pipelineLayoutCreateInfo.setLayoutCount = 0;
	pipelineLayoutCreateInfo.pSetLayouts = nullptr;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 2;
	pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges.data();
	NTSHENGN_VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_uiLineGraphicsPipelineLayout));

	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
	graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPipelineCreateInfo.pNext = &pipelineRenderingCreateInfo;
	graphicsPipelineCreateInfo.flags = 0;
	graphicsPipelineCreateInfo.stageCount = 2;
	graphicsPipelineCreateInfo.pStages = shaderStageCreateInfos.data();
	graphicsPipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
	graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
	graphicsPipelineCreateInfo.pTessellationState = nullptr;
	graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	graphicsPipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
	graphicsPipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
	graphicsPipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	graphicsPipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	graphicsPipelineCreateInfo.layout = m_uiLineGraphicsPipelineLayout;
	graphicsPipelineCreateInfo.renderPass = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.subpass = 0;
	graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.basePipelineIndex = 0;
	NTSHENGN_VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &m_uiLineGraphicsPipeline));

	vkDestroyShaderModule(m_device, vertexShaderModule, nullptr);
	vkDestroyShaderModule(m_device, fragmentShaderModule, nullptr);
}

void NtshEngn::GraphicsModule::createUIRectangleResources() {
	// Create graphics pipeline
	VkFormat pipelineRenderingColorFormat = m_drawImageFormat;

	VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {};
	pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	pipelineRenderingCreateInfo.pNext = nullptr;
	pipelineRenderingCreateInfo.viewMask = 0;
	pipelineRenderingCreateInfo.colorAttachmentCount = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &pipelineRenderingColorFormat;
	pipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
	pipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

	const std::string vertexShaderCode = R"GLSL(
		#version 460

		layout(push_constant) uniform RectanglePositionInfo {
			vec2 topLeft;
			vec2 bottomRight;
		} rPI;

		void main() {
			if ((gl_VertexIndex == 0) || (gl_VertexIndex == 3)) {
				gl_Position = vec4(rPI.bottomRight.x, rPI.topLeft.y, 0.0, 1.0);
			}
			else if (gl_VertexIndex == 1) {
				gl_Position = vec4(rPI.topLeft, 0.0, 1.0);
			}
			else if ((gl_VertexIndex == 2) || (gl_VertexIndex == 4)) {
				gl_Position = vec4(rPI.topLeft.x, rPI.bottomRight.y, 0.0, 1.0);
			}
			else if (gl_VertexIndex == 5) {
				gl_Position = vec4(rPI.bottomRight, 0.0, 1.0);
			}
		}
	)GLSL";
	const std::vector<uint32_t> vertexShaderSpv = compileShader(vertexShaderCode, ShaderType::Vertex);

	VkShaderModule vertexShaderModule;
	VkShaderModuleCreateInfo vertexShaderModuleCreateInfo = {};
	vertexShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertexShaderModuleCreateInfo.pNext = nullptr;
	vertexShaderModuleCreateInfo.flags = 0;
	vertexShaderModuleCreateInfo.codeSize = vertexShaderSpv.size() * sizeof(uint32_t);
	vertexShaderModuleCreateInfo.pCode = vertexShaderSpv.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &vertexShaderModuleCreateInfo, nullptr, &vertexShaderModule));

	VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo = {};
	vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderStageCreateInfo.pNext = nullptr;
	vertexShaderStageCreateInfo.flags = 0;
	vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexShaderStageCreateInfo.module = vertexShaderModule;
	vertexShaderStageCreateInfo.pName = "main";
	vertexShaderStageCreateInfo.pSpecializationInfo = nullptr;

	const std::string fragmentShaderCode = R"GLSL(
		#version 460

		layout(push_constant) uniform RectangleColorInfo {
			layout(offset = 16) vec4 color;
		} rCI;

		layout(location = 0) out vec4 outColor;

		void main() {
			outColor = rCI.color;
		}
	)GLSL";
	const std::vector<uint32_t> fragmentShaderSpv = compileShader(fragmentShaderCode, ShaderType::Fragment);

	VkShaderModule fragmentShaderModule;
	VkShaderModuleCreateInfo fragmentShaderModuleCreateInfo = {};
	fragmentShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	fragmentShaderModuleCreateInfo.pNext = nullptr;
	fragmentShaderModuleCreateInfo.flags = 0;
	fragmentShaderModuleCreateInfo.codeSize = fragmentShaderSpv.size() * sizeof(uint32_t);
	fragmentShaderModuleCreateInfo.pCode = fragmentShaderSpv.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &fragmentShaderModuleCreateInfo, nullptr, &fragmentShaderModule));

	VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo = {};
	fragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderStageCreateInfo.pNext = nullptr;
	fragmentShaderStageCreateInfo.flags = 0;
	fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentShaderStageCreateInfo.module = fragmentShaderModule;
	fragmentShaderStageCreateInfo.pName = "main";
	fragmentShaderStageCreateInfo.pSpecializationInfo = nullptr;

	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStageCreateInfos = { vertexShaderStageCreateInfo, fragmentShaderStageCreateInfo };

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
	vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateCreateInfo.pNext = nullptr;
	vertexInputStateCreateInfo.flags = 0;
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexBindingDescriptions = nullptr;
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = nullptr;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {};
	inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyStateCreateInfo.pNext = nullptr;
	inputAssemblyStateCreateInfo.flags = 0;
	inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.pNext = nullptr;
	viewportStateCreateInfo.flags = 0;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &m_viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &m_scissor;

	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {};
	rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateCreateInfo.pNext = nullptr;
	rasterizationStateCreateInfo.flags = 0;
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
	rasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
	rasterizationStateCreateInfo.depthBiasClamp = 0.0f;
	rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;
	rasterizationStateCreateInfo.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {};
	multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleStateCreateInfo.pNext = nullptr;
	multisampleStateCreateInfo.flags = 0;
	multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
	multisampleStateCreateInfo.minSampleShading = 0.0f;
	multisampleStateCreateInfo.pSampleMask = nullptr;
	multisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
	multisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;

	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {};
	depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilStateCreateInfo.pNext = nullptr;
	depthStencilStateCreateInfo.flags = 0;
	depthStencilStateCreateInfo.depthTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.depthWriteEnable = VK_FALSE;
	depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_NEVER;
	depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.front = {};
	depthStencilStateCreateInfo.back = {};
	depthStencilStateCreateInfo.minDepthBounds = 0.0f;
	depthStencilStateCreateInfo.maxDepthBounds = 1.0f;

	VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
	colorBlendAttachmentState.blendEnable = VK_TRUE;
	colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {};
	colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendStateCreateInfo.pNext = nullptr;
	colorBlendStateCreateInfo.flags = 0;
	colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
	colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	colorBlendStateCreateInfo.attachmentCount = 1;
	colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;

	std::array<VkDynamicState, 2> dynamicStates = { VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_VIEWPORT };
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.pNext = nullptr;
	dynamicStateCreateInfo.flags = 0;
	dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

	VkPushConstantRange vertexPushConstantRange = {};
	vertexPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	vertexPushConstantRange.offset = 0;
	vertexPushConstantRange.size = sizeof(Math::vec2) + sizeof(Math::vec2);

	VkPushConstantRange fragmentPushConstantRange = {};
	fragmentPushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentPushConstantRange.offset = sizeof(Math::vec4);
	fragmentPushConstantRange.size = sizeof(Math::vec4);

	std::array<VkPushConstantRange, 2> pushConstantRanges = { vertexPushConstantRange, fragmentPushConstantRange };
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.pNext = nullptr;
	pipelineLayoutCreateInfo.flags = 0;
	pipelineLayoutCreateInfo.setLayoutCount = 0;
	pipelineLayoutCreateInfo.pSetLayouts = nullptr;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 2;
	pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges.data();
	NTSHENGN_VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_uiRectangleGraphicsPipelineLayout));

	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
	graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPipelineCreateInfo.pNext = &pipelineRenderingCreateInfo;
	graphicsPipelineCreateInfo.flags = 0;
	graphicsPipelineCreateInfo.stageCount = 2;
	graphicsPipelineCreateInfo.pStages = shaderStageCreateInfos.data();
	graphicsPipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
	graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
	graphicsPipelineCreateInfo.pTessellationState = nullptr;
	graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	graphicsPipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
	graphicsPipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
	graphicsPipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	graphicsPipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	graphicsPipelineCreateInfo.layout = m_uiRectangleGraphicsPipelineLayout;
	graphicsPipelineCreateInfo.renderPass = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.subpass = 0;
	graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.basePipelineIndex = 0;
	NTSHENGN_VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &m_uiRectangleGraphicsPipeline));

	vkDestroyShaderModule(m_device, vertexShaderModule, nullptr);
	vkDestroyShaderModule(m_device, fragmentShaderModule, nullptr);
}

void NtshEngn::GraphicsModule::createUIImageResources() {
	// Create descriptor set layout
	VkDescriptorSetLayoutBinding uiTexturesDescriptorSetLayoutBinding = {};
	uiTexturesDescriptorSetLayoutBinding.binding = 0;
	uiTexturesDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	uiTexturesDescriptorSetLayoutBinding.descriptorCount = 131072;
	uiTexturesDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	uiTexturesDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorBindingFlags descriptorBindingFlag = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
	VkDescriptorSetLayoutBindingFlagsCreateInfo descriptorSetLayoutBindingFlagsCreateInfo = {};
	descriptorSetLayoutBindingFlagsCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	descriptorSetLayoutBindingFlagsCreateInfo.pNext = nullptr;
	descriptorSetLayoutBindingFlagsCreateInfo.bindingCount = 1;
	descriptorSetLayoutBindingFlagsCreateInfo.pBindingFlags = &descriptorBindingFlag;

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.pNext = &descriptorSetLayoutBindingFlagsCreateInfo;
	descriptorSetLayoutCreateInfo.flags = 0;
	descriptorSetLayoutCreateInfo.bindingCount = 1;
	descriptorSetLayoutCreateInfo.pBindings = &uiTexturesDescriptorSetLayoutBinding;
	NTSHENGN_VK_CHECK(vkCreateDescriptorSetLayout(m_device, &descriptorSetLayoutCreateInfo, nullptr, &m_uiImageDescriptorSetLayout));

	// Create graphics pipeline
	VkFormat pipelineRenderingColorFormat = m_drawImageFormat;

	VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {};
	pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	pipelineRenderingCreateInfo.pNext = nullptr;
	pipelineRenderingCreateInfo.viewMask = 0;
	pipelineRenderingCreateInfo.colorAttachmentCount = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &pipelineRenderingColorFormat;
	pipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
	pipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

	const std::string vertexShaderCode = R"GLSL(
		#version 460

		layout(push_constant) uniform UITexturePositionInfo {
			vec2 v0;
			vec2 v1;
			vec2 v2;
			vec2 v3;
			vec2 reverseUV;
		} uTPI;

		layout(location = 0) out vec2 outUv;

		void main() {
			if ((gl_VertexIndex == 0) || (gl_VertexIndex == 3)) {
				gl_Position = vec4(uTPI.v0, 0.0, 1.0);
				outUv = vec2(1.0 - uTPI.reverseUV.x, 0.0 + uTPI.reverseUV.y);
			}
			else if (gl_VertexIndex == 1) {
				gl_Position = vec4(uTPI.v1, 0.0, 1.0);
				outUv = vec2(0.0 + uTPI.reverseUV.x, 0.0 + uTPI.reverseUV.y);
			}
			else if ((gl_VertexIndex == 2) || (gl_VertexIndex == 4)) {
				gl_Position = vec4(uTPI.v2, 0.0, 1.0);
				outUv = vec2(0.0 + uTPI.reverseUV.x, 1.0 - uTPI.reverseUV.y);
			}
			else if (gl_VertexIndex == 5) {
				gl_Position = vec4(uTPI.v3, 0.0, 1.0);
				outUv = vec2(1.0 - uTPI.reverseUV.x, 1.0 - uTPI.reverseUV.y);
			}
		}
	)GLSL";
	const std::vector<uint32_t> vertexShaderSpv = compileShader(vertexShaderCode, ShaderType::Vertex);

	VkShaderModule vertexShaderModule;
	VkShaderModuleCreateInfo vertexShaderModuleCreateInfo = {};
	vertexShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertexShaderModuleCreateInfo.pNext = nullptr;
	vertexShaderModuleCreateInfo.flags = 0;
	vertexShaderModuleCreateInfo.codeSize = vertexShaderSpv.size() * sizeof(uint32_t);
	vertexShaderModuleCreateInfo.pCode = vertexShaderSpv.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &vertexShaderModuleCreateInfo, nullptr, &vertexShaderModule));

	VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo = {};
	vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderStageCreateInfo.pNext = nullptr;
	vertexShaderStageCreateInfo.flags = 0;
	vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexShaderStageCreateInfo.module = vertexShaderModule;
	vertexShaderStageCreateInfo.pName = "main";
	vertexShaderStageCreateInfo.pSpecializationInfo = nullptr;

	const std::string fragmentShaderCode = R"GLSL(
		#version 460
		#extension GL_EXT_nonuniform_qualifier : enable

		layout(push_constant) uniform UITextureInfo {
			layout(offset = 48) vec4 color;
			uint uiTextureIndex;
		} uTI;

		layout(set = 0, binding = 0) uniform sampler2D uiTextures[];

		layout(location = 0) in vec2 uv;

		layout(location = 0) out vec4 outColor;

		void main() {
			outColor = texture(uiTextures[nonuniformEXT(uTI.uiTextureIndex)], uv) * uTI.color;
		}
	)GLSL";
	const std::vector<uint32_t> fragmentShaderSpv = compileShader(fragmentShaderCode, ShaderType::Fragment);

	VkShaderModule fragmentShaderModule;
	VkShaderModuleCreateInfo fragmentShaderModuleCreateInfo = {};
	fragmentShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	fragmentShaderModuleCreateInfo.pNext = nullptr;
	fragmentShaderModuleCreateInfo.flags = 0;
	fragmentShaderModuleCreateInfo.codeSize = fragmentShaderSpv.size() * sizeof(uint32_t);
	fragmentShaderModuleCreateInfo.pCode = fragmentShaderSpv.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &fragmentShaderModuleCreateInfo, nullptr, &fragmentShaderModule));

	VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo = {};
	fragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderStageCreateInfo.pNext = nullptr;
	fragmentShaderStageCreateInfo.flags = 0;
	fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentShaderStageCreateInfo.module = fragmentShaderModule;
	fragmentShaderStageCreateInfo.pName = "main";
	fragmentShaderStageCreateInfo.pSpecializationInfo = nullptr;

	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStageCreateInfos = { vertexShaderStageCreateInfo, fragmentShaderStageCreateInfo };

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
	vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateCreateInfo.pNext = nullptr;
	vertexInputStateCreateInfo.flags = 0;
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexBindingDescriptions = nullptr;
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = nullptr;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {};
	inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyStateCreateInfo.pNext = nullptr;
	inputAssemblyStateCreateInfo.flags = 0;
	inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.pNext = nullptr;
	viewportStateCreateInfo.flags = 0;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &m_viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &m_scissor;

	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {};
	rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateCreateInfo.pNext = nullptr;
	rasterizationStateCreateInfo.flags = 0;
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
	rasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
	rasterizationStateCreateInfo.depthBiasClamp = 0.0f;
	rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;
	rasterizationStateCreateInfo.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {};
	multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleStateCreateInfo.pNext = nullptr;
	multisampleStateCreateInfo.flags = 0;
	multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
	multisampleStateCreateInfo.minSampleShading = 0.0f;
	multisampleStateCreateInfo.pSampleMask = nullptr;
	multisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
	multisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;

	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {};
	depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilStateCreateInfo.pNext = nullptr;
	depthStencilStateCreateInfo.flags = 0;
	depthStencilStateCreateInfo.depthTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.depthWriteEnable = VK_FALSE;
	depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_NEVER;
	depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.front = {};
	depthStencilStateCreateInfo.back = {};
	depthStencilStateCreateInfo.minDepthBounds = 0.0f;
	depthStencilStateCreateInfo.maxDepthBounds = 1.0f;

	VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
	colorBlendAttachmentState.blendEnable = VK_TRUE;
	colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {};
	colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendStateCreateInfo.pNext = nullptr;
	colorBlendStateCreateInfo.flags = 0;
	colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
	colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	colorBlendStateCreateInfo.attachmentCount = 1;
	colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;

	std::array<VkDynamicState, 2> dynamicStates = { VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_VIEWPORT };
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.pNext = nullptr;
	dynamicStateCreateInfo.flags = 0;
	dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

	VkPushConstantRange vertexPushConstantRange = {};
	vertexPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	vertexPushConstantRange.offset = 0;
	vertexPushConstantRange.size = 5 * sizeof(Math::vec2);

	VkPushConstantRange fragmentPushConstantRange = {};
	fragmentPushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentPushConstantRange.offset = 5 * sizeof(Math::vec2) + sizeof(Math::vec2);
	fragmentPushConstantRange.size = sizeof(Math::vec4) + sizeof(uint32_t);

	std::array<VkPushConstantRange, 2> pushConstantRanges = { vertexPushConstantRange, fragmentPushConstantRange };
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.pNext = nullptr;
	pipelineLayoutCreateInfo.flags = 0;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &m_uiImageDescriptorSetLayout;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 2;
	pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges.data();
	NTSHENGN_VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_uiImageGraphicsPipelineLayout));

	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
	graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPipelineCreateInfo.pNext = &pipelineRenderingCreateInfo;
	graphicsPipelineCreateInfo.flags = 0;
	graphicsPipelineCreateInfo.stageCount = 2;
	graphicsPipelineCreateInfo.pStages = shaderStageCreateInfos.data();
	graphicsPipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
	graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
	graphicsPipelineCreateInfo.pTessellationState = nullptr;
	graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	graphicsPipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
	graphicsPipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
	graphicsPipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	graphicsPipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	graphicsPipelineCreateInfo.layout = m_uiImageGraphicsPipelineLayout;
	graphicsPipelineCreateInfo.renderPass = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.subpass = 0;
	graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.basePipelineIndex = 0;
	NTSHENGN_VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &m_uiImageGraphicsPipeline));

	vkDestroyShaderModule(m_device, vertexShaderModule, nullptr);
	vkDestroyShaderModule(m_device, fragmentShaderModule, nullptr);

	// Create descriptor pool
	VkDescriptorPoolSize uiTexturesDescriptorPoolSize = {};
	uiTexturesDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	uiTexturesDescriptorPoolSize.descriptorCount = 131072 * m_framesInFlight;

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext = nullptr;
	descriptorPoolCreateInfo.flags = 0;
	descriptorPoolCreateInfo.maxSets = m_framesInFlight;
	descriptorPoolCreateInfo.poolSizeCount = 1;
	descriptorPoolCreateInfo.pPoolSizes = &uiTexturesDescriptorPoolSize;
	NTSHENGN_VK_CHECK(vkCreateDescriptorPool(m_device, &descriptorPoolCreateInfo, nullptr, &m_uiImageDescriptorPool));

	// Allocate descriptor sets
	m_uiImageDescriptorSets.resize(m_framesInFlight);
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
		descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocateInfo.pNext = nullptr;
		descriptorSetAllocateInfo.descriptorPool = m_uiImageDescriptorPool;
		descriptorSetAllocateInfo.descriptorSetCount = 1;
		descriptorSetAllocateInfo.pSetLayouts = &m_uiImageDescriptorSetLayout;
		NTSHENGN_VK_CHECK(vkAllocateDescriptorSets(m_device, &descriptorSetAllocateInfo, &m_uiImageDescriptorSets[i]));
	}

	m_uiImageDescriptorSetsNeedUpdate.resize(m_framesInFlight);
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		m_uiImageDescriptorSetsNeedUpdate[i] = false;
	}
}

void NtshEngn::GraphicsModule::updateUIImageDescriptorSet(uint32_t frameInFlight) {
	std::vector<VkDescriptorImageInfo> uiTexturesDescriptorImageInfos(m_uiTextures.size());
	for (size_t i = 0; i < m_uiTextures.size(); i++) {
		uiTexturesDescriptorImageInfos[i].sampler = (m_uiTextures[i].second == ImageSamplerFilter::Nearest) ? m_uiNearestSampler : m_uiLinearSampler;
		uiTexturesDescriptorImageInfos[i].imageView = m_textureImageViews[m_uiTextures[i].first];
		uiTexturesDescriptorImageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}

	VkWriteDescriptorSet uiTexturesDescriptorWriteDescriptorSet = {};
	uiTexturesDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	uiTexturesDescriptorWriteDescriptorSet.pNext = nullptr;
	uiTexturesDescriptorWriteDescriptorSet.dstSet = m_uiImageDescriptorSets[frameInFlight];
	uiTexturesDescriptorWriteDescriptorSet.dstBinding = 0;
	uiTexturesDescriptorWriteDescriptorSet.dstArrayElement = 0;
	uiTexturesDescriptorWriteDescriptorSet.descriptorCount = static_cast<uint32_t>(uiTexturesDescriptorImageInfos.size());
	uiTexturesDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	uiTexturesDescriptorWriteDescriptorSet.pImageInfo = uiTexturesDescriptorImageInfos.data();
	uiTexturesDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
	uiTexturesDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

	vkUpdateDescriptorSets(m_device, 1, &uiTexturesDescriptorWriteDescriptorSet, 0, nullptr);
}

void NtshEngn::GraphicsModule::createDefaultResources() {
	// Default mesh
	m_meshes.push_back({ 0, 0, 0, 0 });

	// Create texture sampler
	VkSampler defaultTextureSampler;
	VkSamplerCreateInfo textureSamplerCreateInfo = {};
	textureSamplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	textureSamplerCreateInfo.pNext = nullptr;
	textureSamplerCreateInfo.flags = 0;
	textureSamplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	textureSamplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	textureSamplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	textureSamplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	textureSamplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	textureSamplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	textureSamplerCreateInfo.mipLodBias = 0.0f;
	textureSamplerCreateInfo.anisotropyEnable = VK_TRUE;
	textureSamplerCreateInfo.maxAnisotropy = 16.0f;
	textureSamplerCreateInfo.compareEnable = VK_FALSE;
	textureSamplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
	textureSamplerCreateInfo.minLod = 0.0f;
	textureSamplerCreateInfo.maxLod = VK_LOD_CLAMP_NONE;
	textureSamplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	textureSamplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
	NTSHENGN_VK_CHECK(vkCreateSampler(m_device, &textureSamplerCreateInfo, nullptr, &defaultTextureSampler));

	m_textureSamplers["defaultSampler"] = defaultTextureSampler;

	// Default diffuse texture
	m_defaultDiffuseTexture.width = 16;
	m_defaultDiffuseTexture.height = 16;
	m_defaultDiffuseTexture.format = ImageFormat::R8G8B8A8;
	m_defaultDiffuseTexture.colorSpace = ImageColorSpace::SRGB;
	m_defaultDiffuseTexture.data.resize(m_defaultDiffuseTexture.width * m_defaultDiffuseTexture.height * 4 * 1);
	for (size_t i = 0; i < 256; i++) {
		m_defaultDiffuseTexture.data[i * 4 + 0] = static_cast<uint8_t>(255 - i);
		m_defaultDiffuseTexture.data[i * 4 + 1] = static_cast<uint8_t>(i % 128);
		m_defaultDiffuseTexture.data[i * 4 + 2] = static_cast<uint8_t>(i);
		m_defaultDiffuseTexture.data[i * 4 + 3] = static_cast<uint8_t>(255);
	}

	load(m_defaultDiffuseTexture);
	m_textures.push_back({ 0, "defaultSampler" });

	// Default normal texture
	m_defaultNormalTexture.width = 1;
	m_defaultNormalTexture.height = 1;
	m_defaultNormalTexture.format = ImageFormat::R8G8B8A8;
	m_defaultNormalTexture.colorSpace = ImageColorSpace::Linear;
	m_defaultNormalTexture.data = { 127, 127, 255, 255 };

	load(m_defaultNormalTexture);
	m_textures.push_back({ 1, "defaultSampler" });

	// Default metalness texture
	m_defaultMetalnessTexture.width = 1;
	m_defaultMetalnessTexture.height = 1;
	m_defaultMetalnessTexture.format = ImageFormat::R8G8B8A8;
	m_defaultMetalnessTexture.colorSpace = ImageColorSpace::Linear;
	m_defaultMetalnessTexture.data = { 0, 0, 0, 255 };

	load(m_defaultMetalnessTexture);
	m_textures.push_back({ 2, "defaultSampler" });

	// Default roughness texture
	m_defaultRoughnessTexture.width = 1;
	m_defaultRoughnessTexture.height = 1;
	m_defaultRoughnessTexture.format = ImageFormat::R8G8B8A8;
	m_defaultRoughnessTexture.colorSpace = ImageColorSpace::Linear;
	m_defaultRoughnessTexture.data = { 0, 0, 0, 255 };

	load(m_defaultRoughnessTexture);
	m_textures.push_back({ 3, "defaultSampler" });

	// Default occlusion texture
	m_defaultOcclusionTexture.width = 1;
	m_defaultOcclusionTexture.height = 1;
	m_defaultOcclusionTexture.format = ImageFormat::R8G8B8A8;
	m_defaultOcclusionTexture.colorSpace = ImageColorSpace::Linear;
	m_defaultOcclusionTexture.data = { 255, 255, 255, 255 };

	load(m_defaultOcclusionTexture);
	m_textures.push_back({ 4, "defaultSampler" });

	// Default emissive texture
	m_defaultEmissiveTexture.width = 1;
	m_defaultEmissiveTexture.height = 1;
	m_defaultEmissiveTexture.format = ImageFormat::R8G8B8A8;
	m_defaultEmissiveTexture.colorSpace = ImageColorSpace::SRGB;
	m_defaultEmissiveTexture.data = { 0, 0, 0, 255 };

	load(m_defaultEmissiveTexture);
	m_textures.push_back({ 5, "defaultSampler" });

	m_materials.push_back(InternalMaterial());
}

void NtshEngn::GraphicsModule::resize() {
	if (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) {
		NTSHENGN_VK_CHECK(vkQueueWaitIdle(m_graphicsQueue));

		// Destroy swapchain image views
		for (VkImageView& swapchainImageView : m_swapchainImageViews) {
			vkDestroyImageView(m_device, swapchainImageView, nullptr);
		}

		// Recreate the swapchain
		createSwapchain(m_swapchain);
		
		// Destroy color image and image view
		vkDestroyImageView(m_device, m_colorImageView, nullptr);
		vmaDestroyImage(m_allocator, m_colorImage, m_colorImageAllocation);

		// Recreate color image
		createColorImage();

		// Update descriptor sets
		VkDescriptorImageInfo colorImageDescriptorImageInfo;
		colorImageDescriptorImageInfo.sampler = VK_NULL_HANDLE;
		colorImageDescriptorImageInfo.imageView = m_colorImageView;
		colorImageDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

		for (uint32_t i = 0; i < m_framesInFlight; i++) {
			VkWriteDescriptorSet colorImageDescriptorWriteDescriptorSet = {};
			colorImageDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			colorImageDescriptorWriteDescriptorSet.pNext = nullptr;
			colorImageDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
			colorImageDescriptorWriteDescriptorSet.dstBinding = 0;
			colorImageDescriptorWriteDescriptorSet.dstArrayElement = 0;
			colorImageDescriptorWriteDescriptorSet.descriptorCount = 1;
			colorImageDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			colorImageDescriptorWriteDescriptorSet.pImageInfo = &colorImageDescriptorImageInfo;
			colorImageDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
			colorImageDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

			vkUpdateDescriptorSets(m_device, 1, &colorImageDescriptorWriteDescriptorSet, 0, nullptr);
		}

		// Update tone mapping descriptor set
		colorImageDescriptorImageInfo.sampler = m_toneMappingSampler;
		colorImageDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet colorImageDescriptorWriteDescriptorSet = {};
		colorImageDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		colorImageDescriptorWriteDescriptorSet.pNext = nullptr;
		colorImageDescriptorWriteDescriptorSet.dstSet = m_toneMappingDescriptorSet;
		colorImageDescriptorWriteDescriptorSet.dstBinding = 0;
		colorImageDescriptorWriteDescriptorSet.dstArrayElement = 0;
		colorImageDescriptorWriteDescriptorSet.descriptorCount = 1;
		colorImageDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		colorImageDescriptorWriteDescriptorSet.pImageInfo = &colorImageDescriptorImageInfo;
		colorImageDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
		colorImageDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

		vkUpdateDescriptorSets(m_device, 1, &colorImageDescriptorWriteDescriptorSet, 0, nullptr);

		m_sampleBatch = 0;
	}
}

bool NtshEngn::GraphicsModule::loadRenderableForEntity(Entity entity) {
	const Renderable& renderable = ecs->getComponent<Renderable>(entity);
	InternalObject& object = m_objects[entity];

	bool meshChanged = false;

	if (renderable.mesh && !renderable.mesh->vertices.empty()) {
		const std::unordered_map<const Mesh*, MeshID>::const_iterator newMesh = m_meshAddresses.find(renderable.mesh);
		if ((newMesh == m_meshAddresses.end()) || (newMesh->second != object.meshID)) {
			object.meshID = load(*renderable.mesh);

			meshChanged = true;
		}
	}
	else {
		if (object.meshID != 0) {
			object.meshID = 0;

			meshChanged = true;
		}
	}

	if (renderable.material == m_lastKnownMaterial[entity]) {
		return meshChanged;
	}

	InternalMaterial material = m_materials[object.materialIndex];
	if (renderable.material.diffuseTexture.image) {
		const std::unordered_map<const Image*, ImageID>::const_iterator newImage = m_imageAddresses.find(renderable.material.diffuseTexture.image);
		ImageID imageID = m_textures[material.diffuseTextureIndex].imageID;

		bool textureChanged = false;
		if ((newImage == m_imageAddresses.end()) || (newImage->second != imageID)) {
			imageID = load(*renderable.material.diffuseTexture.image);
			textureChanged = true;
		}

		const std::string samplerKey = createSampler(renderable.material.diffuseTexture.imageSampler);
		if (samplerKey != m_textures[material.diffuseTextureIndex].samplerKey) {
			textureChanged = true;
		}

		if (textureChanged) {
			material.diffuseTextureIndex = addToTextures({ imageID, samplerKey });
		}
	}
	if (renderable.material.normalTexture.image) {
		const std::unordered_map<const Image*, ImageID>::const_iterator newImage = m_imageAddresses.find(renderable.material.normalTexture.image);
		ImageID imageID = m_textures[material.normalTextureIndex].imageID;

		bool textureChanged = false;
		if ((newImage == m_imageAddresses.end()) || (newImage->second != imageID)) {
			imageID = load(*renderable.material.normalTexture.image);
			textureChanged = true;
		}

		const std::string samplerKey = createSampler(renderable.material.normalTexture.imageSampler);
		if (samplerKey != m_textures[material.normalTextureIndex].samplerKey) {
			textureChanged = true;
		}

		if (textureChanged) {
			material.normalTextureIndex = addToTextures({ imageID, samplerKey });
		}
	}
	if (renderable.material.metalnessTexture.image) {
		const std::unordered_map<const Image*, ImageID>::const_iterator newImage = m_imageAddresses.find(renderable.material.metalnessTexture.image);
		ImageID imageID = m_textures[material.metalnessTextureIndex].imageID;

		bool textureChanged = false;
		if ((newImage == m_imageAddresses.end()) || (newImage->second != imageID)) {
			imageID = load(*renderable.material.metalnessTexture.image);
			textureChanged = true;
		}

		const std::string samplerKey = createSampler(renderable.material.metalnessTexture.imageSampler);
		if (samplerKey != m_textures[material.metalnessTextureIndex].samplerKey) {
			textureChanged = true;
		}

		if (textureChanged) {
			material.metalnessTextureIndex = addToTextures({ imageID, samplerKey });
		}
	}
	if (renderable.material.roughnessTexture.image) {
		const std::unordered_map<const Image*, ImageID>::const_iterator newImage = m_imageAddresses.find(renderable.material.roughnessTexture.image);
		ImageID imageID = m_textures[material.roughnessTextureIndex].imageID;

		bool textureChanged = false;
		if ((newImage == m_imageAddresses.end()) || (newImage->second != imageID)) {
			imageID = load(*renderable.material.roughnessTexture.image);
			textureChanged = true;
		}

		const std::string samplerKey = createSampler(renderable.material.roughnessTexture.imageSampler);
		if (samplerKey != m_textures[material.roughnessTextureIndex].samplerKey) {
			textureChanged = true;
		}

		if (textureChanged) {
			material.roughnessTextureIndex = addToTextures({ imageID, samplerKey });
		}
	}
	if (renderable.material.occlusionTexture.image) {
		const std::unordered_map<const Image*, ImageID>::const_iterator newImage = m_imageAddresses.find(renderable.material.occlusionTexture.image);
		ImageID imageID = m_textures[material.occlusionTextureIndex].imageID;

		bool textureChanged = false;
		if ((newImage == m_imageAddresses.end()) || (newImage->second != imageID)) {
			imageID = load(*renderable.material.occlusionTexture.image);
			textureChanged = true;
		}

		const std::string samplerKey = createSampler(renderable.material.occlusionTexture.imageSampler);
		if (samplerKey != m_textures[material.occlusionTextureIndex].samplerKey) {
			textureChanged = true;
		}

		if (textureChanged) {
			material.occlusionTextureIndex = addToTextures({ imageID, samplerKey });
		}
	}
	if (renderable.material.emissiveTexture.image) {
		const std::unordered_map<const Image*, ImageID>::const_iterator newImage = m_imageAddresses.find(renderable.material.emissiveTexture.image);
		ImageID imageID = m_textures[material.emissiveTextureIndex].imageID;

		bool textureChanged = false;
		if ((newImage == m_imageAddresses.end()) || (newImage->second != imageID)) {
			imageID = load(*renderable.material.emissiveTexture.image);
			textureChanged = true;
		}

		const std::string samplerKey = createSampler(renderable.material.emissiveTexture.imageSampler);
		if (samplerKey != m_textures[material.emissiveTextureIndex].samplerKey) {
			textureChanged = true;
		}

		if (textureChanged) {
			material.emissiveTextureIndex = addToTextures({ imageID, samplerKey });
		}
	}
	if ((renderable.material.emissiveFactor != material.emissiveFactor) ||
		(renderable.material.alphaCutoff != material.alphaCutoff)) {
		material.emissiveFactor = renderable.material.emissiveFactor;
		material.alphaCutoff = renderable.material.alphaCutoff;
	}

	uint32_t materialID = addToMaterials(material);
	object.materialIndex = materialID;

	m_lastKnownMaterial[entity] = renderable.material;

	return true;
}

std::string NtshEngn::GraphicsModule::createSampler(const ImageSampler& sampler) {
	const std::string samplerKey = "mag:" + std::to_string(m_filterMap.at(sampler.magFilter)) +
		"/min:" + std::to_string(m_filterMap.at(sampler.minFilter)) +
		"/mip:" + std::to_string(m_mipmapFilterMap.at(sampler.mipmapFilter)) +
		"/aU:" + std::to_string(m_addressModeMap.at(sampler.addressModeU)) +
		"/aV:" + std::to_string(m_addressModeMap.at(sampler.addressModeV)) +
		"/aW:" + std::to_string(m_addressModeMap.at(sampler.addressModeW)) +
		"/mlb:" + std::to_string(0.0f) +
		"/aE:" + std::to_string(sampler.anisotropyLevel > 0.0f ? VK_TRUE : VK_FALSE) +
		"/cE:" + std::to_string(VK_FALSE) +
		"/cO:" + std::to_string(VK_COMPARE_OP_NEVER) +
		"/mL:" + std::to_string(0.0f) +
		"/ML:" + std::to_string(VK_LOD_CLAMP_NONE) +
		"/bC:" + std::to_string(m_borderColorMap.at(sampler.borderColor)) +
		"/unC:" + std::to_string(VK_FALSE);

	if (m_textureSamplers.find(samplerKey) != m_textureSamplers.end()) {
		return samplerKey;
	}

	VkSampler newSampler;

	VkSamplerCreateInfo samplerCreateInfo = {};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.pNext = nullptr;
	samplerCreateInfo.flags = 0;
	samplerCreateInfo.magFilter = m_filterMap.at(sampler.magFilter);
	samplerCreateInfo.minFilter = m_filterMap.at(sampler.minFilter);
	samplerCreateInfo.mipmapMode = m_mipmapFilterMap.at(sampler.mipmapFilter);
	samplerCreateInfo.addressModeU = m_addressModeMap.at(sampler.addressModeU);
	samplerCreateInfo.addressModeV = m_addressModeMap.at(sampler.addressModeV);
	samplerCreateInfo.addressModeW = m_addressModeMap.at(sampler.addressModeW);
	samplerCreateInfo.mipLodBias = 0.0f;
	samplerCreateInfo.anisotropyEnable = sampler.anisotropyLevel > 0.0f ? VK_TRUE : VK_FALSE;
	samplerCreateInfo.maxAnisotropy = sampler.anisotropyLevel;
	samplerCreateInfo.compareEnable = VK_FALSE;
	samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
	samplerCreateInfo.minLod = 0.0f;
	samplerCreateInfo.maxLod = VK_LOD_CLAMP_NONE;
	samplerCreateInfo.borderColor = m_borderColorMap.at(sampler.borderColor);
	samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
	NTSHENGN_VK_CHECK(vkCreateSampler(m_device, &samplerCreateInfo, nullptr, &newSampler));

	m_textureSamplers[samplerKey] = newSampler;

	return samplerKey;
}

uint32_t NtshEngn::GraphicsModule::addToTextures(const InternalTexture& texture) {
	for (size_t i = 0; i < m_textures.size(); i++) {
		const InternalTexture& tex = m_textures[i];
		if ((tex.imageID == texture.imageID) &&
			(tex.samplerKey == texture.samplerKey)) {
			return static_cast<uint32_t>(i);
		}
	}

	m_textures.push_back(texture);

	return static_cast<uint32_t>(m_textures.size()) - 1;
}

uint32_t NtshEngn::GraphicsModule::addToMaterials(const InternalMaterial& material) {
	for (size_t i = 0; i < m_materials.size(); i++) {
		const InternalMaterial& mat = m_materials[i];
		if ((mat.diffuseTextureIndex == material.diffuseTextureIndex) &&
			(mat.normalTextureIndex == material.normalTextureIndex) &&
			(mat.metalnessTextureIndex == material.metalnessTextureIndex) &&
			(mat.roughnessTextureIndex == material.roughnessTextureIndex) &&
			(mat.occlusionTextureIndex == material.occlusionTextureIndex) &&
			(mat.emissiveTextureIndex == material.emissiveTextureIndex) &&
			(mat.emissiveFactor == material.emissiveFactor) &&
			(mat.alphaCutoff == material.alphaCutoff)) {
			return static_cast<uint32_t>(i);
		}
	}

	m_materials.push_back(material);

	return static_cast<uint32_t>(m_materials.size()) - 1;
}

uint32_t NtshEngn::GraphicsModule::attributeObjectIndex() {
	uint32_t objectID = m_freeObjectsIndices[0];
	m_freeObjectsIndices.erase(m_freeObjectsIndices.begin());
	if (m_freeObjectsIndices.empty()) {
		m_freeObjectsIndices.push_back(objectID + 1);
	}

	return objectID;
}

void NtshEngn::GraphicsModule::retrieveObjectIndex(uint32_t objectIndex) {
	m_freeObjectsIndices.insert(m_freeObjectsIndices.begin(), objectIndex);
}

extern "C" NTSHENGN_MODULE_API NtshEngn::GraphicsModuleInterface* createModule() {
	return new NtshEngn::GraphicsModule;
}

extern "C" NTSHENGN_MODULE_API void destroyModule(NtshEngn::GraphicsModuleInterface* m) {
	delete m;
}