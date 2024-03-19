#include "ntshengn_graphics_module.h"
#include "../Module/utils/ntshengn_dynamic_library.h"
#include "../Common/module_interfaces/ntshengn_window_module_interface.h"
#include "../Common/utils/ntshengn_utils_file.h"
#include <limits>
#include <array>
#include <chrono>
#include <algorithm>
#include <cmath>

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
#if defined(NTSHENGN_OS_WINDOWS)
		instanceExtensions.push_back("VK_KHR_win32_surface");
#elif defined(NTSHENGN_OS_LINUX)
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
#elif defined(NTSHENGN_OS_LINUX)
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

	VkPhysicalDeviceProperties2 physicalDeviceProperties2 = {};
	physicalDeviceProperties2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	physicalDeviceProperties2.pNext = nullptr;
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
	VkPhysicalDeviceMaintenance4Features physicalDeviceMaintenance4Features = {};
	physicalDeviceMaintenance4Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MAINTENANCE_4_FEATURES;
	physicalDeviceMaintenance4Features.pNext = nullptr;
	physicalDeviceMaintenance4Features.maintenance4 = VK_TRUE;

	VkPhysicalDeviceDynamicRenderingFeatures physicalDeviceDynamicRenderingFeatures = {};
	physicalDeviceDynamicRenderingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES;
	physicalDeviceDynamicRenderingFeatures.pNext = &physicalDeviceMaintenance4Features;
	physicalDeviceDynamicRenderingFeatures.dynamicRendering = VK_TRUE;

	VkPhysicalDeviceSynchronization2Features physicalDeviceSynchronization2Features = {};
	physicalDeviceSynchronization2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES;
	physicalDeviceSynchronization2Features.pNext = &physicalDeviceDynamicRenderingFeatures;
	physicalDeviceSynchronization2Features.synchronization2 = VK_TRUE;

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
		"VK_KHR_maintenance4" };
	if (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) {
		deviceExtensions.push_back("VK_KHR_swapchain");
	}
	deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
	deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
	deviceCreateInfo.pEnabledFeatures = nullptr;
	NTSHENGN_VK_CHECK(vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_device));

	vkGetDeviceQueue(m_device, m_graphicsQueueFamilyIndex, 0, &m_graphicsQueue);

	// Get functions
	m_vkCmdPipelineBarrier2KHR = (PFN_vkCmdPipelineBarrier2KHR)vkGetDeviceProcAddr(m_device, "vkCmdPipelineBarrier2KHR");
	m_vkCmdBeginRenderingKHR = (PFN_vkCmdBeginRenderingKHR)vkGetDeviceProcAddr(m_device, "vkCmdBeginRenderingKHR");
	m_vkCmdEndRenderingKHR = (PFN_vkCmdEndRenderingKHR)vkGetDeviceProcAddr(m_device, "vkCmdEndRenderingKHR");

	// Initialize VMA
	VmaAllocatorCreateInfo vmaAllocatorCreateInfo = {};
	vmaAllocatorCreateInfo.flags = 0;
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
		imageCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
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
		NTSHENGN_VK_CHECK(vkCreateImage(m_device, &imageCreateInfo, nullptr, &m_drawImage));

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

	// Create initialization fence
	VkFenceCreateInfo initializationFenceCreateInfo = {};
	initializationFenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	initializationFenceCreateInfo.pNext = nullptr;
	initializationFenceCreateInfo.flags = 0;
	NTSHENGN_VK_CHECK(vkCreateFence(m_device, &initializationFenceCreateInfo, nullptr, &m_initializationFence));

	createColorImage();

	createDescriptorSetLayout();

	createToneMappingResources();

	// Create graphics pipeline
	const std::vector<uint32_t> vertexShaderCode = { 0x07230203,0x00010000,0x0008000b,0x0000002c,0x00000000,0x00020011,0x00000001,0x0006000b,
	0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
	0x0008000f,0x00000000,0x00000004,0x6e69616d,0x00000000,0x00000009,0x0000000c,0x0000001d,
	0x00030003,0x00000002,0x000001cc,0x00040005,0x00000004,0x6e69616d,0x00000000,0x00040005,
	0x00000009,0x5574756f,0x00000076,0x00060005,0x0000000c,0x565f6c67,0x65747265,0x646e4978,
	0x00007865,0x00060005,0x0000001b,0x505f6c67,0x65567265,0x78657472,0x00000000,0x00060006,
	0x0000001b,0x00000000,0x505f6c67,0x7469736f,0x006e6f69,0x00070006,0x0000001b,0x00000001,
	0x505f6c67,0x746e696f,0x657a6953,0x00000000,0x00070006,0x0000001b,0x00000002,0x435f6c67,
	0x4470696c,0x61747369,0x0065636e,0x00070006,0x0000001b,0x00000003,0x435f6c67,0x446c6c75,
	0x61747369,0x0065636e,0x00030005,0x0000001d,0x00000000,0x00040047,0x00000009,0x0000001e,
	0x00000000,0x00040047,0x0000000c,0x0000000b,0x0000002a,0x00050048,0x0000001b,0x00000000,
	0x0000000b,0x00000000,0x00050048,0x0000001b,0x00000001,0x0000000b,0x00000001,0x00050048,
	0x0000001b,0x00000002,0x0000000b,0x00000003,0x00050048,0x0000001b,0x00000003,0x0000000b,
	0x00000004,0x00030047,0x0000001b,0x00000002,0x00020013,0x00000002,0x00030021,0x00000003,
	0x00000002,0x00030016,0x00000006,0x00000020,0x00040017,0x00000007,0x00000006,0x00000002,
	0x00040020,0x00000008,0x00000003,0x00000007,0x0004003b,0x00000008,0x00000009,0x00000003,
	0x00040015,0x0000000a,0x00000020,0x00000001,0x00040020,0x0000000b,0x00000001,0x0000000a,
	0x0004003b,0x0000000b,0x0000000c,0x00000001,0x0004002b,0x0000000a,0x0000000e,0x00000001,
	0x0004002b,0x0000000a,0x00000010,0x00000002,0x00040017,0x00000017,0x00000006,0x00000004,
	0x00040015,0x00000018,0x00000020,0x00000000,0x0004002b,0x00000018,0x00000019,0x00000001,
	0x0004001c,0x0000001a,0x00000006,0x00000019,0x0006001e,0x0000001b,0x00000017,0x00000006,
	0x0000001a,0x0000001a,0x00040020,0x0000001c,0x00000003,0x0000001b,0x0004003b,0x0000001c,
	0x0000001d,0x00000003,0x0004002b,0x0000000a,0x0000001e,0x00000000,0x0004002b,0x00000006,
	0x00000020,0x40000000,0x0004002b,0x00000006,0x00000022,0xbf800000,0x0004002b,0x00000006,
	0x00000025,0x00000000,0x0004002b,0x00000006,0x00000026,0x3f800000,0x00040020,0x0000002a,
	0x00000003,0x00000017,0x00050036,0x00000002,0x00000004,0x00000000,0x00000003,0x000200f8,
	0x00000005,0x0004003d,0x0000000a,0x0000000d,0x0000000c,0x000500c4,0x0000000a,0x0000000f,
	0x0000000d,0x0000000e,0x000500c7,0x0000000a,0x00000011,0x0000000f,0x00000010,0x0004006f,
	0x00000006,0x00000012,0x00000011,0x0004003d,0x0000000a,0x00000013,0x0000000c,0x000500c7,
	0x0000000a,0x00000014,0x00000013,0x00000010,0x0004006f,0x00000006,0x00000015,0x00000014,
	0x00050050,0x00000007,0x00000016,0x00000012,0x00000015,0x0003003e,0x00000009,0x00000016,
	0x0004003d,0x00000007,0x0000001f,0x00000009,0x0005008e,0x00000007,0x00000021,0x0000001f,
	0x00000020,0x00050050,0x00000007,0x00000023,0x00000022,0x00000022,0x00050081,0x00000007,
	0x00000024,0x00000021,0x00000023,0x00050051,0x00000006,0x00000027,0x00000024,0x00000000,
	0x00050051,0x00000006,0x00000028,0x00000024,0x00000001,0x00070050,0x00000017,0x00000029,
	0x00000027,0x00000028,0x00000025,0x00000026,0x00050041,0x0000002a,0x0000002b,0x0000001d,
	0x0000001e,0x0003003e,0x0000002b,0x00000029,0x000100fd,0x00010038 };

	m_pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	m_pipelineRenderingCreateInfo.pNext = nullptr;
	m_pipelineRenderingCreateInfo.viewMask = 0;
	m_pipelineRenderingCreateInfo.colorAttachmentCount = 1;
	m_pipelineRenderingCreateInfo.pColorAttachmentFormats = &m_pipelineRenderingColorFormat;
	m_pipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
	m_pipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

	VkShaderModuleCreateInfo vertexShaderModuleCreateInfo = {};
	vertexShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertexShaderModuleCreateInfo.pNext = nullptr;
	vertexShaderModuleCreateInfo.flags = 0;
	vertexShaderModuleCreateInfo.codeSize = vertexShaderCode.size() * sizeof(uint32_t);
	vertexShaderModuleCreateInfo.pCode = vertexShaderCode.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &vertexShaderModuleCreateInfo, nullptr, &m_vertexShaderModule));
	
	m_vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	m_vertexShaderStageCreateInfo.pNext = nullptr;
	m_vertexShaderStageCreateInfo.flags = 0;
	m_vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	m_vertexShaderStageCreateInfo.module = m_vertexShaderModule;
	m_vertexShaderStageCreateInfo.pName = "main";
	m_vertexShaderStageCreateInfo.pSpecializationInfo = nullptr;

	VkPushConstantRange fragmentShaderPushConstantRange = {};
	fragmentShaderPushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentShaderPushConstantRange.offset = 0;
	fragmentShaderPushConstantRange.size = sizeof(PushConstants);

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.pNext = nullptr;
	pipelineLayoutCreateInfo.flags = 0;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &m_descriptorSetLayout;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pPushConstantRanges = &fragmentShaderPushConstantRange;
	NTSHENGN_VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_graphicsPipelineLayout));

	m_vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	m_vertexInputStateCreateInfo.pNext = nullptr;
	m_vertexInputStateCreateInfo.flags = 0;
	m_vertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;
	m_vertexInputStateCreateInfo.pVertexBindingDescriptions = nullptr;
	m_vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;
	m_vertexInputStateCreateInfo.pVertexAttributeDescriptions = nullptr;

	m_inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	m_inputAssemblyStateCreateInfo.pNext = nullptr;
	m_inputAssemblyStateCreateInfo.flags = 0;
	m_inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	m_inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

	m_viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	m_viewportStateCreateInfo.pNext = nullptr;
	m_viewportStateCreateInfo.flags = 0;
	m_viewportStateCreateInfo.viewportCount = 1;
	m_viewportStateCreateInfo.pViewports = &m_viewport;
	m_viewportStateCreateInfo.scissorCount = 1;
	m_viewportStateCreateInfo.pScissors = &m_scissor;

	m_rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	m_rasterizationStateCreateInfo.pNext = nullptr;
	m_rasterizationStateCreateInfo.flags = 0;
	m_rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
	m_rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	m_rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	m_rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	m_rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
	m_rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
	m_rasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
	m_rasterizationStateCreateInfo.depthBiasClamp = 0.0f;
	m_rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;
	m_rasterizationStateCreateInfo.lineWidth = 1.0f;

	m_multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	m_multisampleStateCreateInfo.pNext = nullptr;
	m_multisampleStateCreateInfo.flags = 0;
	m_multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	m_multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
	m_multisampleStateCreateInfo.minSampleShading = 0.0f;
	m_multisampleStateCreateInfo.pSampleMask = nullptr;
	m_multisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
	m_multisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;

	m_depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	m_depthStencilStateCreateInfo.pNext = nullptr;
	m_depthStencilStateCreateInfo.flags = 0;
	m_depthStencilStateCreateInfo.depthTestEnable = VK_FALSE;
	m_depthStencilStateCreateInfo.depthWriteEnable = VK_FALSE;
	m_depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_NEVER;
	m_depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
	m_depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
	m_depthStencilStateCreateInfo.front = {};
	m_depthStencilStateCreateInfo.back = {};
	m_depthStencilStateCreateInfo.minDepthBounds = 0.0f;
	m_depthStencilStateCreateInfo.maxDepthBounds = 1.0f;

	m_colorBlendAttachmentState.blendEnable = VK_FALSE;
	m_colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	m_colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	m_colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	m_colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	m_colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	m_colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
	m_colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	m_colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	m_colorBlendStateCreateInfo.pNext = nullptr;
	m_colorBlendStateCreateInfo.flags = 0;
	m_colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
	m_colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	m_colorBlendStateCreateInfo.attachmentCount = 1;
	m_colorBlendStateCreateInfo.pAttachments = &m_colorBlendAttachmentState;

	m_dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	m_dynamicStateCreateInfo.pNext = nullptr;
	m_dynamicStateCreateInfo.flags = 0;
	m_dynamicStateCreateInfo.dynamicStateCount = 2;
	m_dynamicStateCreateInfo.pDynamicStates = m_dynamicStates.data();

	if (std::filesystem::exists(m_fragmentShaderName)) {
		m_fragmentShaderLastModified = std::filesystem::last_write_time(m_fragmentShaderName);
	}
	else {
		const std::filesystem::path absolutePath = std::filesystem::absolute(std::filesystem::current_path());
		NTSHENGN_MODULE_ERROR("Fragment shader \"shader.frag\" does not exist (\"" + absolutePath.string() + "/" + m_fragmentShaderName + "\").", Result::ModuleError);
	}
	recreateGraphicsPipeline();

	// Create light storage buffer
	m_lightBuffers.resize(m_framesInFlight);
	m_lightBufferAllocations.resize(m_framesInFlight);
	VkBufferCreateInfo lightBufferCreateInfo = {};
	lightBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	lightBufferCreateInfo.pNext = nullptr;
	lightBufferCreateInfo.flags = 0;
	lightBufferCreateInfo.size = 32768;
	lightBufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	lightBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	lightBufferCreateInfo.queueFamilyIndexCount = 1;
	lightBufferCreateInfo.pQueueFamilyIndices = &m_graphicsQueueFamilyIndex;

	VmaAllocationCreateInfo bufferAllocationCreateInfo = {};
	bufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	bufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &lightBufferCreateInfo, &bufferAllocationCreateInfo, &m_lightBuffers[i], &m_lightBufferAllocations[i], nullptr));
	}

	// Create descriptor pool
	VkDescriptorPoolSize lightsDescriptorPoolSize = {};
	lightsDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	lightsDescriptorPoolSize.descriptorCount = m_framesInFlight;

	std::array<VkDescriptorPoolSize, 1> descriptorPoolSizes = { lightsDescriptorPoolSize };
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

		VkDescriptorBufferInfo lightsDescriptorBufferInfo;
		lightsDescriptorBufferInfo.buffer = m_lightBuffers[i];
		lightsDescriptorBufferInfo.offset = 0;
		lightsDescriptorBufferInfo.range = 32768;

		VkWriteDescriptorSet lightsDescriptorWriteDescriptorSet = {};
		lightsDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		lightsDescriptorWriteDescriptorSet.pNext = nullptr;
		lightsDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		lightsDescriptorWriteDescriptorSet.dstBinding = 0;
		lightsDescriptorWriteDescriptorSet.dstArrayElement = 0;
		lightsDescriptorWriteDescriptorSet.descriptorCount = 1;
		lightsDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		lightsDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		lightsDescriptorWriteDescriptorSet.pBufferInfo = &lightsDescriptorBufferInfo;
		lightsDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;
		writeDescriptorSets.push_back(lightsDescriptorWriteDescriptorSet);

		vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}

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

	if (windowModule && !windowModule->isWindowOpen(windowModule->getMainWindowID())) {
		// Do not update if the main window got closed
		return;
	}

	Math::vec4 cameraPosition = { 0.0f, 0.0f, 0.0f, 0.0f };
	Math::vec4 cameraDirection = { 0.0f, 0.0f, -1.0f, 0.0f };
	if (m_mainCamera != NTSHENGN_ENTITY_UNKNOWN) {
		const Camera& camera = ecs->getComponent<Camera>(m_mainCamera);
		const Transform& cameraTransform = ecs->getComponent<Transform>(m_mainCamera);

		const Math::vec3 baseCameraDirection = Math::normalize(camera.forward);
		const float baseDirectionYaw = std::atan2(baseCameraDirection.z, baseCameraDirection.x);
		const float baseDirectionPitch = -std::asin(baseCameraDirection.y);
		const Math::vec3 cameraDirectionAsVec3 = Math::normalize(Math::vec3(
			std::cos(baseDirectionPitch + cameraTransform.rotation.x) * std::cos(baseDirectionYaw + cameraTransform.rotation.y),
			-std::sin(baseDirectionPitch + cameraTransform.rotation.x),
			std::cos(baseDirectionPitch + cameraTransform.rotation.x) * std::sin(baseDirectionYaw + cameraTransform.rotation.y)
		));
		cameraPosition = Math::vec4(cameraTransform.position, 0.0f);
		cameraDirection = Math::vec4(cameraDirectionAsVec3, 0.0f);
	}

	NTSHENGN_VK_CHECK(vkWaitForFences(m_device, 1, &m_fences[m_currentFrameInFlight], VK_TRUE, std::numeric_limits<uint64_t>::max()));

	// Check for any file modification
	bool shaderModified = false;
	const std::filesystem::file_time_type timeNow = std::filesystem::_File_time_clock::now();
	std::filesystem::file_time_type fileLastModified;
	if (std::filesystem::exists(m_fragmentShaderName)) {
		fileLastModified = std::filesystem::last_write_time(m_fragmentShaderName);
		if ((fileLastModified > m_fragmentShaderLastModified) && (timeNow > (fileLastModified + std::chrono::milliseconds(250)))) {
			shaderModified = true;
			m_fragmentShaderLastModified = fileLastModified;
		}
	}
	else {
		const std::filesystem::path absolutePath = std::filesystem::absolute(std::filesystem::current_path());
		NTSHENGN_MODULE_ERROR("Fragment shader \"shader.frag\" does not exist (\"" + absolutePath.string() + "/" + m_fragmentShaderName + "\").", Result::ModuleError);
	}

	if (shaderModified) {
		NTSHENGN_VK_CHECK(vkQueueWaitIdle(m_graphicsQueue));

		if (recreateGraphicsPipeline()) {
			NTSHENGN_MODULE_INFO("Fragment shader reloaded.");
		}
		else {
			NTSHENGN_MODULE_WARNING("Fragment shader could not be reloaded.");
		}
	}

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

	// Update lights buffer
	void* data;
	NTSHENGN_VK_CHECK(vmaMapMemory(m_allocator, m_lightBufferAllocations[m_currentFrameInFlight], &data));
	std::array<uint32_t, 4> lightsCount = { static_cast<uint32_t>(m_lights.directionalLights.size()), static_cast<uint32_t>(m_lights.pointLights.size()), static_cast<uint32_t>(m_lights.spotLights.size()), 0 };
	memcpy(data, lightsCount.data(), 4 * sizeof(uint32_t));

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
		internalLight.color = Math::vec4(lightLight.color, 0.0f);

		memcpy(reinterpret_cast<char*>(data) + offset, &internalLight, sizeof(InternalLight));
		offset += sizeof(InternalLight);
	}
	for (Entity light : m_lights.pointLights) {
		const Light& lightLight = ecs->getComponent<Light>(light);
		const Transform& lightTransform = ecs->getComponent<Transform>(light);

		InternalLight internalLight;
		internalLight.position = Math::vec4(lightTransform.position, 0.0f);
		internalLight.color = Math::vec4(lightLight.color, 0.0f);

		memcpy(reinterpret_cast<char*>(data) + offset, &internalLight, sizeof(InternalLight));
		offset += sizeof(InternalLight);
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
		internalLight.color = Math::vec4(lightLight.color, 0.0f);
		internalLight.cutoff = Math::vec4(lightLight.cutoff, 0.0f, 0.0f);

		memcpy(reinterpret_cast<char*>(data) + offset, &internalLight, sizeof(InternalLight));
		offset += sizeof(InternalLight);
	}
	vmaUnmapMemory(m_allocator, m_lightBufferAllocations[m_currentFrameInFlight]);

	// Record rendering commands
	NTSHENGN_VK_CHECK(vkResetCommandPool(m_device, m_renderingCommandPools[m_currentFrameInFlight], 0));

	// Begin command buffer recording
	VkCommandBufferBeginInfo commandBufferBeginInfo = {};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.pNext = nullptr;
	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	NTSHENGN_VK_CHECK(vkBeginCommandBuffer(m_renderingCommandBuffers[m_currentFrameInFlight], &commandBufferBeginInfo));

	// Layout transition VK_IMAGE_LAYOUT_UNDEFINED -> VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	VkImageMemoryBarrier2 swapchainOrDrawImageMemoryBarrier = {};
	swapchainOrDrawImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	swapchainOrDrawImageMemoryBarrier.pNext = nullptr;
	swapchainOrDrawImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
	swapchainOrDrawImageMemoryBarrier.srcAccessMask = 0;
	swapchainOrDrawImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	swapchainOrDrawImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	swapchainOrDrawImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	swapchainOrDrawImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	swapchainOrDrawImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	swapchainOrDrawImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	swapchainOrDrawImageMemoryBarrier.image = (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) ? m_swapchainImages[imageIndex] : m_drawImage;
	swapchainOrDrawImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	swapchainOrDrawImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	swapchainOrDrawImageMemoryBarrier.subresourceRange.levelCount = 1;
	swapchainOrDrawImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	swapchainOrDrawImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkImageMemoryBarrier2 undefinedToColorAttachmentOptimalImageMemoryBarrier = {};
	undefinedToColorAttachmentOptimalImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.pNext = nullptr;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.image = m_colorImage;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.subresourceRange.levelCount = 1;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.subresourceRange.layerCount = 1;

	std::array<VkImageMemoryBarrier2, 2> imageMemoryBarriers = { swapchainOrDrawImageMemoryBarrier, undefinedToColorAttachmentOptimalImageMemoryBarrier };
	VkDependencyInfo undefinedToColorAttachmentOptimalDependencyInfo = {};
	undefinedToColorAttachmentOptimalDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	undefinedToColorAttachmentOptimalDependencyInfo.pNext = nullptr;
	undefinedToColorAttachmentOptimalDependencyInfo.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
	undefinedToColorAttachmentOptimalDependencyInfo.memoryBarrierCount = 0;
	undefinedToColorAttachmentOptimalDependencyInfo.pMemoryBarriers = nullptr;
	undefinedToColorAttachmentOptimalDependencyInfo.bufferMemoryBarrierCount = 0;
	undefinedToColorAttachmentOptimalDependencyInfo.pBufferMemoryBarriers = nullptr;
	undefinedToColorAttachmentOptimalDependencyInfo.imageMemoryBarrierCount = static_cast<uint32_t>(imageMemoryBarriers.size());
	undefinedToColorAttachmentOptimalDependencyInfo.pImageMemoryBarriers = imageMemoryBarriers.data();
	m_vkCmdPipelineBarrier2KHR(m_renderingCommandBuffers[m_currentFrameInFlight], &undefinedToColorAttachmentOptimalDependencyInfo);

	// Bind descriptor set 0
	vkCmdBindDescriptorSets(m_renderingCommandBuffers[m_currentFrameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipelineLayout, 0, 1, &m_descriptorSets[m_currentFrameInFlight], 0, nullptr);

	// Begin rendering
	VkRenderingAttachmentInfo renderingColorAttachmentInfo = {};
	renderingColorAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	renderingColorAttachmentInfo.pNext = nullptr;
	renderingColorAttachmentInfo.imageView = m_colorImageView;
	renderingColorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	renderingColorAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
	renderingColorAttachmentInfo.resolveImageView = VK_NULL_HANDLE;
	renderingColorAttachmentInfo.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	renderingColorAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	renderingColorAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	renderingColorAttachmentInfo.clearValue.color = { 0.0f, 0.0f, 0.0f, 0.0f };

	VkRenderingInfo renderingInfo = {};
	renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderingInfo.pNext = nullptr;
	renderingInfo.flags = 0;
	renderingInfo.renderArea = m_scissor;
	renderingInfo.layerCount = 1;
	renderingInfo.viewMask = 0;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments = &renderingColorAttachmentInfo;
	renderingInfo.pDepthAttachment = nullptr;
	renderingInfo.pStencilAttachment = nullptr;
	m_vkCmdBeginRenderingKHR(m_renderingCommandBuffers[m_currentFrameInFlight], &renderingInfo);

	if (m_graphicsPipeline != VK_NULL_HANDLE) {
		// Bind graphics pipeline
		vkCmdBindPipeline(m_renderingCommandBuffers[m_currentFrameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
		vkCmdSetViewport(m_renderingCommandBuffers[m_currentFrameInFlight], 0, 1, &m_viewport);
		vkCmdSetScissor(m_renderingCommandBuffers[m_currentFrameInFlight], 0, 1, &m_scissor);

		// Push time constant
		PushConstants pushConstants = { static_cast<float>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count()) / 1000.0f, m_scissor.extent.width, m_scissor.extent.height, 0.0f, { cameraPosition[0], cameraPosition[1], cameraPosition[2], 0.0f }, { cameraDirection[0], cameraDirection[1], cameraDirection[2], 0.0f } };
		vkCmdPushConstants(m_renderingCommandBuffers[m_currentFrameInFlight], m_graphicsPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstants), &pushConstants);

		// Draw
		vkCmdDraw(m_renderingCommandBuffers[m_currentFrameInFlight], 3, 1, 0, 0);
	}

	// End rendering
	m_vkCmdEndRenderingKHR(m_renderingCommandBuffers[m_currentFrameInFlight]);

	// Layout transition VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL -> VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	VkImageMemoryBarrier2 colorAttachmentOptimalToShaderReadOnlyImageMemoryBarrier = {};
	colorAttachmentOptimalToShaderReadOnlyImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	colorAttachmentOptimalToShaderReadOnlyImageMemoryBarrier.pNext = nullptr;
	colorAttachmentOptimalToShaderReadOnlyImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	colorAttachmentOptimalToShaderReadOnlyImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	colorAttachmentOptimalToShaderReadOnlyImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	colorAttachmentOptimalToShaderReadOnlyImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	colorAttachmentOptimalToShaderReadOnlyImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachmentOptimalToShaderReadOnlyImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	colorAttachmentOptimalToShaderReadOnlyImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	colorAttachmentOptimalToShaderReadOnlyImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	colorAttachmentOptimalToShaderReadOnlyImageMemoryBarrier.image = m_colorImage;
	colorAttachmentOptimalToShaderReadOnlyImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	colorAttachmentOptimalToShaderReadOnlyImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	colorAttachmentOptimalToShaderReadOnlyImageMemoryBarrier.subresourceRange.levelCount = 1;
	colorAttachmentOptimalToShaderReadOnlyImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	colorAttachmentOptimalToShaderReadOnlyImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkDependencyInfo colorAttachmentToShaderReadOnlyOptimalDependencyInfo = {};
	colorAttachmentToShaderReadOnlyOptimalDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	colorAttachmentToShaderReadOnlyOptimalDependencyInfo.pNext = nullptr;
	colorAttachmentToShaderReadOnlyOptimalDependencyInfo.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
	colorAttachmentToShaderReadOnlyOptimalDependencyInfo.memoryBarrierCount = 0;
	colorAttachmentToShaderReadOnlyOptimalDependencyInfo.pMemoryBarriers = nullptr;
	colorAttachmentToShaderReadOnlyOptimalDependencyInfo.bufferMemoryBarrierCount = 0;
	colorAttachmentToShaderReadOnlyOptimalDependencyInfo.pBufferMemoryBarriers = nullptr;
	colorAttachmentToShaderReadOnlyOptimalDependencyInfo.imageMemoryBarrierCount = 1;
	colorAttachmentToShaderReadOnlyOptimalDependencyInfo.pImageMemoryBarriers = &colorAttachmentOptimalToShaderReadOnlyImageMemoryBarrier;
	m_vkCmdPipelineBarrier2KHR(m_renderingCommandBuffers[m_currentFrameInFlight], &colorAttachmentToShaderReadOnlyOptimalDependencyInfo);

	// Tonemapping
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

	// Layout transition VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL -> VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	if (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) {
		VkImageMemoryBarrier2 colorAttachmentOptimalToPresentSrcImageMemoryBarrier = {};
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.pNext = nullptr;
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.dstAccessMask = 0;
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.image = m_swapchainImages[imageIndex];
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.subresourceRange.levelCount = 1;
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.subresourceRange.layerCount = 1;

		VkDependencyInfo colorAttachmentOptimalToPresentSrcDependencyInfo = {};
		colorAttachmentOptimalToPresentSrcDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		colorAttachmentOptimalToPresentSrcDependencyInfo.pNext = nullptr;
		colorAttachmentOptimalToPresentSrcDependencyInfo.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
		colorAttachmentOptimalToPresentSrcDependencyInfo.memoryBarrierCount = 0;
		colorAttachmentOptimalToPresentSrcDependencyInfo.pMemoryBarriers = nullptr;
		colorAttachmentOptimalToPresentSrcDependencyInfo.bufferMemoryBarrierCount = 0;
		colorAttachmentOptimalToPresentSrcDependencyInfo.pBufferMemoryBarriers = nullptr;
		colorAttachmentOptimalToPresentSrcDependencyInfo.imageMemoryBarrierCount = 1;
		colorAttachmentOptimalToPresentSrcDependencyInfo.pImageMemoryBarriers = &colorAttachmentOptimalToPresentSrcImageMemoryBarrier;

		m_vkCmdPipelineBarrier2KHR(m_renderingCommandBuffers[m_currentFrameInFlight], &colorAttachmentOptimalToPresentSrcDependencyInfo);
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

	// Destroy lights buffers
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		vmaDestroyBuffer(m_allocator, m_lightBuffers[i], m_lightBufferAllocations[i]);
	}

	// Destroy tone mapping resources
	vkDestroyDescriptorPool(m_device, m_toneMappingDescriptorPool, nullptr);
	vkDestroyPipeline(m_device, m_toneMappingGraphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_toneMappingGraphicsPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(m_device, m_toneMappingDescriptorSetLayout, nullptr);
	vkDestroySampler(m_device, m_toneMappingSampler, nullptr);

	// Destroy descriptor pool
	vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);

	// Destroy graphics pipeline
	vkDestroyShaderModule(m_device, m_vertexShaderModule, nullptr);
	if (m_fragmentShaderModule != VK_NULL_HANDLE) {
		vkDestroyShaderModule(m_device, m_fragmentShaderModule, nullptr);
	}
	vkDestroyPipelineLayout(m_device, m_graphicsPipelineLayout, nullptr);
	if (m_graphicsPipeline != VK_NULL_HANDLE) {
		vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
	}

	// Destroy descriptor set layout
	vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);

	// Destroy color image and image view
	vkDestroyImageView(m_device, m_colorImageView, nullptr);
	vmaDestroyImage(m_allocator, m_colorImage, m_colorImageAllocation);

	// Destroy initialization fence
	vkDestroyFence(m_device, m_initializationFence, nullptr);

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
	NTSHENGN_UNUSED(mesh);
	NTSHENGN_MODULE_FUNCTION_NOT_IMPLEMENTED();

	return NTSHENGN_MESH_UNKNOWN;
}

NtshEngn::ImageID NtshEngn::GraphicsModule::load(const Image& image) {
	NTSHENGN_UNUSED(image);
	NTSHENGN_MODULE_FUNCTION_NOT_IMPLEMENTED();

	return NTSHENGN_IMAGE_UNKNOWN;
}

NtshEngn::FontID NtshEngn::GraphicsModule::load(const Font& font) {
	NTSHENGN_UNUSED(font);
	NTSHENGN_MODULE_FUNCTION_NOT_IMPLEMENTED();

	return NTSHENGN_FONT_UNKNOWN;
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
	NTSHENGN_UNUSED(fontID);
	NTSHENGN_UNUSED(text);
	NTSHENGN_UNUSED(position);
	NTSHENGN_UNUSED(color);
	NTSHENGN_MODULE_FUNCTION_NOT_IMPLEMENTED();
}

void NtshEngn::GraphicsModule::drawUILine(const Math::vec2& start, const Math::vec2& end, const Math::vec4& color) {
	NTSHENGN_UNUSED(start);
	NTSHENGN_UNUSED(end);
	NTSHENGN_UNUSED(color);
	NTSHENGN_MODULE_FUNCTION_NOT_IMPLEMENTED();
}

void NtshEngn::GraphicsModule::drawUIRectangle(const Math::vec2& position, const Math::vec2& size, const Math::vec4& color) {
	NTSHENGN_UNUSED(position);
	NTSHENGN_UNUSED(size);
	NTSHENGN_UNUSED(color);
	NTSHENGN_MODULE_FUNCTION_NOT_IMPLEMENTED();
}

void NtshEngn::GraphicsModule::drawUIImage(ImageID imageID, ImageSamplerFilter imageSamplerFilter, const Math::vec2& position, float rotation, const Math::vec2& scale, const Math::vec4& color) {
	NTSHENGN_UNUSED(imageID);
	NTSHENGN_UNUSED(imageSamplerFilter);
	NTSHENGN_UNUSED(position);
	NTSHENGN_UNUSED(rotation);
	NTSHENGN_UNUSED(scale);
	NTSHENGN_UNUSED(color);
	NTSHENGN_MODULE_FUNCTION_NOT_IMPLEMENTED();
}

const NtshEngn::ComponentMask NtshEngn::GraphicsModule::getComponentMask() const {
	ComponentMask componentMask;
	componentMask.set(ecs->getComponentID<Camera>());
	componentMask.set(ecs->getComponentID<Light>());

	return componentMask;
}

void NtshEngn::GraphicsModule::onEntityComponentAdded(Entity entity, Component componentID) {
	if (componentID == ecs->getComponentID<Camera>()) {
		if (m_mainCamera == NTSHENGN_ENTITY_UNKNOWN) {
			m_mainCamera = entity;
		}
	}
	else if (componentID == ecs->getComponentID<Light>()) {
		const Light& light = ecs->getComponent<Light>(entity);

		switch (light.type) {
		case LightType::Directional:
			m_lights.directionalLights.insert(entity);
			break;

		case LightType::Point:
			m_lights.pointLights.insert(entity);
			break;

		case LightType::Spot:
			m_lights.spotLights.insert(entity);
			break;

		default: // Arbitrarily consider it a directional light
			m_lights.directionalLights.insert(entity);
			break;
		}
	}
}

void NtshEngn::GraphicsModule::onEntityComponentRemoved(Entity entity, Component componentID) {
	if (componentID == ecs->getComponentID<Camera>()) {
		if (m_mainCamera == entity) {
			m_mainCamera = NTSHENGN_ENTITY_UNKNOWN;
		}
	}
	else if (componentID == ecs->getComponentID<Light>()) {
		const Light& light = ecs->getComponent<Light>(entity);

		switch (light.type) {
		case LightType::Directional:
			m_lights.directionalLights.erase(entity);
			break;

		case LightType::Point:
			m_lights.pointLights.erase(entity);
			break;

		case LightType::Spot:
			m_lights.spotLights.erase(entity);
			break;

		default: // Arbitrarily consider it a directional light
			m_lights.directionalLights.erase(entity);
			break;
		}
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
	colorImageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
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

	// Layout transition VK_IMAGE_LAYOUT_UNDEFINED -> VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	VkCommandPool colorImageTransitionCommandPool;

	VkCommandPoolCreateInfo colorImageTransitionCommandPoolCreateInfo = {};
	colorImageTransitionCommandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	colorImageTransitionCommandPoolCreateInfo.pNext = nullptr;
	colorImageTransitionCommandPoolCreateInfo.flags = 0;
	colorImageTransitionCommandPoolCreateInfo.queueFamilyIndex = m_graphicsQueueFamilyIndex;
	NTSHENGN_VK_CHECK(vkCreateCommandPool(m_device, &colorImageTransitionCommandPoolCreateInfo, nullptr, &colorImageTransitionCommandPool));

	VkCommandBuffer colorImageTransitionCommandBuffer;

	VkCommandBufferAllocateInfo colorImageTransitionCommandBufferAllocateInfo = {};
	colorImageTransitionCommandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	colorImageTransitionCommandBufferAllocateInfo.pNext = nullptr;
	colorImageTransitionCommandBufferAllocateInfo.commandPool = colorImageTransitionCommandPool;
	colorImageTransitionCommandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	colorImageTransitionCommandBufferAllocateInfo.commandBufferCount = 1;
	NTSHENGN_VK_CHECK(vkAllocateCommandBuffers(m_device, &colorImageTransitionCommandBufferAllocateInfo, &colorImageTransitionCommandBuffer));

	VkCommandBufferBeginInfo colorImageTransitionCommandBufferBeginInfo = {};
	colorImageTransitionCommandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	colorImageTransitionCommandBufferBeginInfo.pNext = nullptr;
	colorImageTransitionCommandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	colorImageTransitionCommandBufferBeginInfo.pInheritanceInfo = nullptr;
	NTSHENGN_VK_CHECK(vkBeginCommandBuffer(colorImageTransitionCommandBuffer, &colorImageTransitionCommandBufferBeginInfo));

	VkImageMemoryBarrier2 undefinedToColorAttachmentOptimalImageMemoryBarrier = {};
	undefinedToColorAttachmentOptimalImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.pNext = nullptr;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.srcAccessMask = 0;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.image = m_colorImage;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.subresourceRange.levelCount = 1;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkDependencyInfo undefinedToColorAttachmentOptimalDependencyInfo = {};
	undefinedToColorAttachmentOptimalDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	undefinedToColorAttachmentOptimalDependencyInfo.pNext = nullptr;
	undefinedToColorAttachmentOptimalDependencyInfo.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
	undefinedToColorAttachmentOptimalDependencyInfo.memoryBarrierCount = 0;
	undefinedToColorAttachmentOptimalDependencyInfo.pMemoryBarriers = nullptr;
	undefinedToColorAttachmentOptimalDependencyInfo.bufferMemoryBarrierCount = 0;
	undefinedToColorAttachmentOptimalDependencyInfo.pBufferMemoryBarriers = nullptr;
	undefinedToColorAttachmentOptimalDependencyInfo.imageMemoryBarrierCount = 1;
	undefinedToColorAttachmentOptimalDependencyInfo.pImageMemoryBarriers = &undefinedToColorAttachmentOptimalImageMemoryBarrier;
	m_vkCmdPipelineBarrier2KHR(colorImageTransitionCommandBuffer, &undefinedToColorAttachmentOptimalDependencyInfo);

	NTSHENGN_VK_CHECK(vkEndCommandBuffer(colorImageTransitionCommandBuffer));

	VkSubmitInfo colorImageTransitionSubmitInfo = {};
	colorImageTransitionSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	colorImageTransitionSubmitInfo.pNext = nullptr;
	colorImageTransitionSubmitInfo.waitSemaphoreCount = 0;
	colorImageTransitionSubmitInfo.pWaitSemaphores = nullptr;
	colorImageTransitionSubmitInfo.pWaitDstStageMask = nullptr;
	colorImageTransitionSubmitInfo.commandBufferCount = 1;
	colorImageTransitionSubmitInfo.pCommandBuffers = &colorImageTransitionCommandBuffer;
	colorImageTransitionSubmitInfo.signalSemaphoreCount = 0;
	colorImageTransitionSubmitInfo.pSignalSemaphores = nullptr;
	NTSHENGN_VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &colorImageTransitionSubmitInfo, m_initializationFence));
	NTSHENGN_VK_CHECK(vkWaitForFences(m_device, 1, &m_initializationFence, VK_TRUE, std::numeric_limits<uint64_t>::max()));
	NTSHENGN_VK_CHECK(vkResetFences(m_device, 1, &m_initializationFence));

	vkDestroyCommandPool(m_device, colorImageTransitionCommandPool, nullptr);
}

void NtshEngn::GraphicsModule::createDescriptorSetLayout() {
	VkDescriptorSetLayoutBinding lightsDescriptorSetLayoutBinding = {};
	lightsDescriptorSetLayoutBinding.binding = 0;
	lightsDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	lightsDescriptorSetLayoutBinding.descriptorCount = 1;
	lightsDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	lightsDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	std::array<VkDescriptorSetLayoutBinding, 1> descriptorSetLayoutBindings = { lightsDescriptorSetLayoutBinding };
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.pNext = nullptr;
	descriptorSetLayoutCreateInfo.flags = 0;
	descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(descriptorSetLayoutBindings.size());
	descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings.data();
	NTSHENGN_VK_CHECK(vkCreateDescriptorSetLayout(m_device, &descriptorSetLayoutCreateInfo, nullptr, &m_descriptorSetLayout));
}

std::vector<uint32_t> NtshEngn::GraphicsModule::compileFragmentShader() {
	if (!m_glslangInitialized) {
		glslang::InitializeProcess();
		m_glslangInitialized = true;
	}

	std::vector<uint32_t> spvCode;

	if (!std::filesystem::exists(m_fragmentShaderName)) {
		const std::filesystem::path absolutePath = std::filesystem::absolute(std::filesystem::current_path());
		NTSHENGN_MODULE_ERROR("Fragment shader shader.frag does not exist (\"" + absolutePath.string() + "/" + m_fragmentShaderName + "\").", Result::ModuleError);
	}
	std::string shaderCode = m_fragmentShaderPrefix + File::readAscii(m_fragmentShaderName);
	const char* shaderCodeCharPtr = shaderCode.c_str();

	EShLanguage shaderType = EShLangFragment;

	glslang::TShader shader(shaderType);
	shader.setStrings(&shaderCodeCharPtr, 1);
	int clientInputSemanticsVersion = 110;
	glslang::EshTargetClientVersion vulkanClientVersion = glslang::EShTargetVulkan_1_1;
	glslang::EShTargetLanguageVersion spvLanguageVersion = glslang::EShTargetSpv_1_2;
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
	includer.pushExternalLocalDirectory(m_fragmentShaderName);
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

bool NtshEngn::GraphicsModule::recreateGraphicsPipeline() {
	const std::vector<uint32_t> fragmentShaderCode = compileFragmentShader();
	if (fragmentShaderCode.size() != 0) {
		// Destroy fragment shader module and graphics pipeline if the fragment shader compilation succeeded
		if (m_fragmentShaderModule != VK_NULL_HANDLE) {
			vkDestroyShaderModule(m_device, m_fragmentShaderModule, nullptr);
		}
		if (m_graphicsPipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
		}
	}
	else {
		// Don't recreate the pipeline if the fragment shader compilation failed
		return false;
	}

	VkShaderModuleCreateInfo fragmentShaderModuleCreateInfo = {};
	fragmentShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	fragmentShaderModuleCreateInfo.pNext = nullptr;
	fragmentShaderModuleCreateInfo.flags = 0;
	fragmentShaderModuleCreateInfo.codeSize = fragmentShaderCode.size() * sizeof(uint32_t);
	fragmentShaderModuleCreateInfo.pCode = fragmentShaderCode.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &fragmentShaderModuleCreateInfo, nullptr, &m_fragmentShaderModule));

	VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo = {};
	fragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderStageCreateInfo.pNext = nullptr;
	fragmentShaderStageCreateInfo.flags = 0;
	fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentShaderStageCreateInfo.module = m_fragmentShaderModule;
	fragmentShaderStageCreateInfo.pName = "main";
	fragmentShaderStageCreateInfo.pSpecializationInfo = nullptr;

	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStageCreateInfos = { m_vertexShaderStageCreateInfo, fragmentShaderStageCreateInfo };

	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
	graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPipelineCreateInfo.pNext = &m_pipelineRenderingCreateInfo;
	graphicsPipelineCreateInfo.flags = 0;
	graphicsPipelineCreateInfo.stageCount = 2;
	graphicsPipelineCreateInfo.pStages = shaderStageCreateInfos.data();
	graphicsPipelineCreateInfo.pVertexInputState = &m_vertexInputStateCreateInfo;
	graphicsPipelineCreateInfo.pInputAssemblyState = &m_inputAssemblyStateCreateInfo;
	graphicsPipelineCreateInfo.pTessellationState = nullptr;
	graphicsPipelineCreateInfo.pViewportState = &m_viewportStateCreateInfo;
	graphicsPipelineCreateInfo.pRasterizationState = &m_rasterizationStateCreateInfo;
	graphicsPipelineCreateInfo.pMultisampleState = &m_multisampleStateCreateInfo;
	graphicsPipelineCreateInfo.pDepthStencilState = &m_depthStencilStateCreateInfo;
	graphicsPipelineCreateInfo.pColorBlendState = &m_colorBlendStateCreateInfo;
	graphicsPipelineCreateInfo.pDynamicState = &m_dynamicStateCreateInfo;
	graphicsPipelineCreateInfo.layout = m_graphicsPipelineLayout;
	graphicsPipelineCreateInfo.renderPass = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.subpass = 0;
	graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.basePipelineIndex = 0;
	NTSHENGN_VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &m_graphicsPipeline));

	return true;
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

	const std::vector<uint32_t> vertexShaderSpv = { 0x07230203,0x00010000,0x0008000b,0x0000002c,0x00000000,0x00020011,0x00000001,0x0006000b,
	0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
	0x0008000f,0x00000000,0x00000004,0x6e69616d,0x00000000,0x00000009,0x0000000c,0x0000001d,
	0x00030003,0x00000002,0x000001cc,0x00040005,0x00000004,0x6e69616d,0x00000000,0x00040005,
	0x00000009,0x5574756f,0x00000076,0x00060005,0x0000000c,0x565f6c67,0x65747265,0x646e4978,
	0x00007865,0x00060005,0x0000001b,0x505f6c67,0x65567265,0x78657472,0x00000000,0x00060006,
	0x0000001b,0x00000000,0x505f6c67,0x7469736f,0x006e6f69,0x00070006,0x0000001b,0x00000001,
	0x505f6c67,0x746e696f,0x657a6953,0x00000000,0x00070006,0x0000001b,0x00000002,0x435f6c67,
	0x4470696c,0x61747369,0x0065636e,0x00070006,0x0000001b,0x00000003,0x435f6c67,0x446c6c75,
	0x61747369,0x0065636e,0x00030005,0x0000001d,0x00000000,0x00040047,0x00000009,0x0000001e,
	0x00000000,0x00040047,0x0000000c,0x0000000b,0x0000002a,0x00050048,0x0000001b,0x00000000,
	0x0000000b,0x00000000,0x00050048,0x0000001b,0x00000001,0x0000000b,0x00000001,0x00050048,
	0x0000001b,0x00000002,0x0000000b,0x00000003,0x00050048,0x0000001b,0x00000003,0x0000000b,
	0x00000004,0x00030047,0x0000001b,0x00000002,0x00020013,0x00000002,0x00030021,0x00000003,
	0x00000002,0x00030016,0x00000006,0x00000020,0x00040017,0x00000007,0x00000006,0x00000002,
	0x00040020,0x00000008,0x00000003,0x00000007,0x0004003b,0x00000008,0x00000009,0x00000003,
	0x00040015,0x0000000a,0x00000020,0x00000001,0x00040020,0x0000000b,0x00000001,0x0000000a,
	0x0004003b,0x0000000b,0x0000000c,0x00000001,0x0004002b,0x0000000a,0x0000000e,0x00000001,
	0x0004002b,0x0000000a,0x00000010,0x00000002,0x00040017,0x00000017,0x00000006,0x00000004,
	0x00040015,0x00000018,0x00000020,0x00000000,0x0004002b,0x00000018,0x00000019,0x00000001,
	0x0004001c,0x0000001a,0x00000006,0x00000019,0x0006001e,0x0000001b,0x00000017,0x00000006,
	0x0000001a,0x0000001a,0x00040020,0x0000001c,0x00000003,0x0000001b,0x0004003b,0x0000001c,
	0x0000001d,0x00000003,0x0004002b,0x0000000a,0x0000001e,0x00000000,0x0004002b,0x00000006,
	0x00000020,0x40000000,0x0004002b,0x00000006,0x00000022,0xbf800000,0x0004002b,0x00000006,
	0x00000025,0x00000000,0x0004002b,0x00000006,0x00000026,0x3f800000,0x00040020,0x0000002a,
	0x00000003,0x00000017,0x00050036,0x00000002,0x00000004,0x00000000,0x00000003,0x000200f8,
	0x00000005,0x0004003d,0x0000000a,0x0000000d,0x0000000c,0x000500c4,0x0000000a,0x0000000f,
	0x0000000d,0x0000000e,0x000500c7,0x0000000a,0x00000011,0x0000000f,0x00000010,0x0004006f,
	0x00000006,0x00000012,0x00000011,0x0004003d,0x0000000a,0x00000013,0x0000000c,0x000500c7,
	0x0000000a,0x00000014,0x00000013,0x00000010,0x0004006f,0x00000006,0x00000015,0x00000014,
	0x00050050,0x00000007,0x00000016,0x00000012,0x00000015,0x0003003e,0x00000009,0x00000016,
	0x0004003d,0x00000007,0x0000001f,0x00000009,0x0005008e,0x00000007,0x00000021,0x0000001f,
	0x00000020,0x00050050,0x00000007,0x00000023,0x00000022,0x00000022,0x00050081,0x00000007,
	0x00000024,0x00000021,0x00000023,0x00050051,0x00000006,0x00000027,0x00000024,0x00000000,
	0x00050051,0x00000006,0x00000028,0x00000024,0x00000001,0x00070050,0x00000017,0x00000029,
	0x00000027,0x00000028,0x00000025,0x00000026,0x00050041,0x0000002a,0x0000002b,0x0000001d,
	0x0000001e,0x0003003e,0x0000002b,0x00000029,0x000100fd,0x00010038 };

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

	const std::vector<uint32_t> fragmentShaderSpv = { 0x07230203,0x00010000,0x0008000b,0x0000002b,0x00000000,0x00020011,0x00000001,0x0006000b,
	0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
	0x0007000f,0x00000004,0x00000004,0x6e69616d,0x00000000,0x00000011,0x00000029,0x00030010,
	0x00000004,0x00000007,0x00030003,0x00000002,0x000001cc,0x00040005,0x00000004,0x6e69616d,
	0x00000000,0x00040005,0x00000009,0x6f6c6f63,0x00000072,0x00060005,0x0000000d,0x67616d69,
	0x6d615365,0x72656c70,0x00000000,0x00030005,0x00000011,0x00007675,0x00050005,0x00000029,
	0x4374756f,0x726f6c6f,0x00000000,0x00040047,0x0000000d,0x00000022,0x00000000,0x00040047,
	0x0000000d,0x00000021,0x00000000,0x00040047,0x00000011,0x0000001e,0x00000000,0x00040047,
	0x00000029,0x0000001e,0x00000000,0x00020013,0x00000002,0x00030021,0x00000003,0x00000002,
	0x00030016,0x00000006,0x00000020,0x00040017,0x00000007,0x00000006,0x00000004,0x00040020,
	0x00000008,0x00000007,0x00000007,0x00090019,0x0000000a,0x00000006,0x00000001,0x00000000,
	0x00000000,0x00000000,0x00000001,0x00000000,0x0003001b,0x0000000b,0x0000000a,0x00040020,
	0x0000000c,0x00000000,0x0000000b,0x0004003b,0x0000000c,0x0000000d,0x00000000,0x00040017,
	0x0000000f,0x00000006,0x00000002,0x00040020,0x00000010,0x00000001,0x0000000f,0x0004003b,
	0x00000010,0x00000011,0x00000001,0x00040017,0x00000014,0x00000006,0x00000003,0x0004002b,
	0x00000006,0x00000017,0x3f800000,0x0006002c,0x00000014,0x00000018,0x00000017,0x00000017,
	0x00000017,0x00040015,0x0000001d,0x00000020,0x00000000,0x0004002b,0x0000001d,0x0000001e,
	0x00000000,0x00040020,0x0000001f,0x00000007,0x00000006,0x0004002b,0x0000001d,0x00000022,
	0x00000001,0x0004002b,0x0000001d,0x00000025,0x00000002,0x00040020,0x00000028,0x00000003,
	0x00000007,0x0004003b,0x00000028,0x00000029,0x00000003,0x00050036,0x00000002,0x00000004,
	0x00000000,0x00000003,0x000200f8,0x00000005,0x0004003b,0x00000008,0x00000009,0x00000007,
	0x0004003d,0x0000000b,0x0000000e,0x0000000d,0x0004003d,0x0000000f,0x00000012,0x00000011,
	0x00050057,0x00000007,0x00000013,0x0000000e,0x00000012,0x0003003e,0x00000009,0x00000013,
	0x0004003d,0x00000007,0x00000015,0x00000009,0x0008004f,0x00000014,0x00000016,0x00000015,
	0x00000015,0x00000000,0x00000001,0x00000002,0x00050081,0x00000014,0x00000019,0x00000016,
	0x00000018,0x0004003d,0x00000007,0x0000001a,0x00000009,0x0008004f,0x00000014,0x0000001b,
	0x0000001a,0x0000001a,0x00000000,0x00000001,0x00000002,0x00050088,0x00000014,0x0000001c,
	0x0000001b,0x00000019,0x00050041,0x0000001f,0x00000020,0x00000009,0x0000001e,0x00050051,
	0x00000006,0x00000021,0x0000001c,0x00000000,0x0003003e,0x00000020,0x00000021,0x00050041,
	0x0000001f,0x00000023,0x00000009,0x00000022,0x00050051,0x00000006,0x00000024,0x0000001c,
	0x00000001,0x0003003e,0x00000023,0x00000024,0x00050041,0x0000001f,0x00000026,0x00000009,
	0x00000025,0x00050051,0x00000006,0x00000027,0x0000001c,0x00000002,0x0003003e,0x00000026,
	0x00000027,0x0004003d,0x00000007,0x0000002a,0x00000009,0x0003003e,0x00000029,0x0000002a,
	0x000100fd,0x00010038 };

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

void NtshEngn::GraphicsModule::resize() {
	if (windowModule && windowModule->isWindowOpen(windowModule->getMainWindowID())) {
		while ((windowModule->getWindowWidth(windowModule->getMainWindowID()) == 0) || (windowModule->getWindowHeight(windowModule->getMainWindowID()) == 0)) {
			windowModule->pollEvents();
		}

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

		// Update tone mapping descriptor set
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
}

extern "C" NTSHENGN_MODULE_API NtshEngn::GraphicsModuleInterface* createModule() {
	return new NtshEngn::GraphicsModule;
}

extern "C" NTSHENGN_MODULE_API void destroyModule(NtshEngn::GraphicsModuleInterface* m) {
	delete m;
}