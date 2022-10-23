#include "ntsh_graphics_module.h"
#include "../external/Module/utils/ntsh_dynamic_library.h"
#include "../external/Common/module_interfaces/ntsh_window_module_interface.h"
#include <limits>
#include <array>

void NutshellGraphicsModule::init() {
	if (m_windowModule) {
		m_framesInFlight = 2;
	}
	else {
		m_framesInFlight = 1;
	}

	// Create instance
	VkApplicationInfo applicationInfo = {};
	applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	applicationInfo.pNext = nullptr;
	applicationInfo.pApplicationName = "NutshellEngine Vulkan Triangle Graphics Module";
	applicationInfo.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
	applicationInfo.pEngineName = "NutshellEngine";
	applicationInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);
	applicationInfo.apiVersion = VK_API_VERSION_1_1;

	VkInstanceCreateInfo instanceCreateInfo = {};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pNext = nullptr;
	instanceCreateInfo.flags = 0;
	instanceCreateInfo.pApplicationInfo = &applicationInfo;
#ifdef NTSH_DEBUG
	std::array<const char*, 1> explicitLayers = { "VK_LAYER_KHRONOS_validation" };
	bool foundValidationLayer = false;
	uint32_t instanceLayerPropertyCount;
	NTSH_VK_CHECK(vkEnumerateInstanceLayerProperties(&instanceLayerPropertyCount, nullptr));
	std::vector<VkLayerProperties> instanceLayerProperties(instanceLayerPropertyCount);
	NTSH_VK_CHECK(vkEnumerateInstanceLayerProperties(&instanceLayerPropertyCount, instanceLayerProperties.data()));

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
		NTSH_MODULE_WARNING("Could not find validation layer VK_LAYER_KHRONOS_validation.");
		instanceCreateInfo.enabledLayerCount = 0;
		instanceCreateInfo.ppEnabledLayerNames = nullptr;
	}
#else
	instanceCreateInfo.enabledLayerCount = 0;
	instanceCreateInfo.ppEnabledLayerNames = nullptr;
#endif
	std::vector<const char*> instanceExtensions;
#if NTSH_DEBUG
	instanceExtensions.push_back("VK_EXT_debug_utils");
#endif
	if (m_windowModule) {
		instanceExtensions.push_back("VK_KHR_surface");
		instanceExtensions.push_back("VK_KHR_get_surface_capabilities2");
#ifdef NTSH_OS_WINDOWS
		instanceExtensions.push_back("VK_KHR_win32_surface");
#elif NTSH_OS_LINUX
		instanceExtensions.push_back("VK_KHR_xlib_surface");
#endif
		instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
		instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
	}
	else {
		instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(instanceExtensions.size());
		instanceCreateInfo.ppEnabledExtensionNames = instanceExtensions.data();
	}
	NTSH_VK_CHECK(vkCreateInstance(&instanceCreateInfo, nullptr, &m_instance));

#ifdef NTSH_DEBUG
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
	NTSH_VK_CHECK(createDebugUtilsMessengerEXT(m_instance, &debugMessengerCreateInfo, nullptr, &m_debugMessenger));
#endif

	// Get functions
	m_vkCmdPipelineBarrier2KHR = (PFN_vkCmdPipelineBarrier2KHR)vkGetInstanceProcAddr(m_instance, "vkCmdPipelineBarrier2KHR");
	m_vkCmdBeginRenderingKHR = (PFN_vkCmdBeginRenderingKHR)vkGetInstanceProcAddr(m_instance, "vkCmdBeginRenderingKHR");
	m_vkCmdEndRenderingKHR = (PFN_vkCmdEndRenderingKHR)vkGetInstanceProcAddr(m_instance, "vkCmdEndRenderingKHR");

	// Create surface
	if (m_windowModule) {
#ifdef NTSH_OS_WINDOWS
		HWND windowHandle = m_windowModule->getNativeHandle();
		VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {};
		surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		surfaceCreateInfo.pNext = nullptr;
		surfaceCreateInfo.flags = 0;
		surfaceCreateInfo.hinstance = reinterpret_cast<HINSTANCE>(GetWindowLongPtr(windowHandle, GWLP_HINSTANCE));
		surfaceCreateInfo.hwnd = windowHandle;
		auto createWin32SurfaceKHR = (PFN_vkCreateWin32SurfaceKHR)vkGetInstanceProcAddr(m_instance, "vkCreateWin32SurfaceKHR");
		NTSH_VK_CHECK(createWin32SurfaceKHR(m_instance, &surfaceCreateInfo, nullptr, &m_surface));
#elif NTSH_OS_LINUX
		m_display = XOpenDisplay(NULL);
		Window windowHandle = m_windowModule->getNativeHandle();
		VkXlibSurfaceCreateInfoKHR surfaceCreateInfo = {};
		surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
		surfaceCreateInfo.pNext = nullptr;
		surfaceCreateInfo.flags = 0;
		surfaceCreateInfo.dpy = m_display;
		surfaceCreateInfo.window = windowHandle;
		auto createXlibSurfaceKHR = (PFN_vkCreateXlibSurfaceKHR)vkGetInstanceProcAddr(m_instance, "vkCreateXlibSurfaceKHR");
		NTSH_VK_CHECK(createXlibSurfaceKHR(m_instance, &surfaceCreateInfo, nullptr, &m_surface));
#endif
	}

	// Pick a physical device
	uint32_t deviceCount;
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
	if (deviceCount == 0) {
		NTSH_MODULE_ERROR("Vulkan: Found no suitable GPU.", NTSH_RESULT_UNKNOWN_ERROR);
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
#ifdef NTSH_OS_WINDOWS
	else if (physicalDeviceProperties2.properties.vendorID == 0x8086) { // Intel
		uint32_t major = (physicalDeviceProperties2.properties.driverVersion >> 14);
		uint32_t minor = (physicalDeviceProperties2.properties.driverVersion) & 0x3fff;
		driverVersion = std::to_string(major) + "." + std::to_string(minor);
	}
#endif

	NTSH_MODULE_INFO("Physical Device Name: " + std::string(physicalDeviceProperties2.properties.deviceName));
	NTSH_MODULE_INFO("Physical Device Type: " + physicalDeviceType);
	NTSH_MODULE_INFO("Physical Device Driver Version: " + driverVersion);
	NTSH_MODULE_INFO("Physical Device Vulkan API Version: " + std::to_string(VK_API_VERSION_MAJOR(physicalDeviceProperties2.properties.apiVersion)) + "."
		+ std::to_string(VK_API_VERSION_MINOR(physicalDeviceProperties2.properties.apiVersion)) + "."
		+ std::to_string(VK_API_VERSION_PATCH(physicalDeviceProperties2.properties.apiVersion)));

	// Find a queue family supporting graphics
	uint32_t queueFamilyPropertyCount;
	vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyPropertyCount, nullptr);
	std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyPropertyCount);
	vkGetPhysicalDeviceQueueFamilyProperties(m_physicalDevice, &queueFamilyPropertyCount, queueFamilyProperties.data());
	m_graphicsQueueIndex = 0;
	for (const VkQueueFamilyProperties& queueFamilyProperty : queueFamilyProperties) {
		if (queueFamilyProperty.queueCount > 0 && queueFamilyProperty.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			if (m_windowModule) {
				VkBool32 presentSupport;
				vkGetPhysicalDeviceSurfaceSupportKHR(m_physicalDevice, m_graphicsQueueIndex, m_surface, &presentSupport);
				if (presentSupport) {
					break;
				}
			}
			else {
				break;
			}
		}
		m_graphicsQueueIndex++;
	}

	// Create a queue supporting graphics
	float queuePriority = 1.0f;
	VkDeviceQueueCreateInfo deviceQueueCreateInfo = {};
	deviceQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	deviceQueueCreateInfo.pNext = nullptr;
	deviceQueueCreateInfo.flags = 0;
	deviceQueueCreateInfo.queueFamilyIndex = m_graphicsQueueIndex;
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
#ifdef NTSH_DEBUG
	if (foundValidationLayer) {
		deviceCreateInfo.enabledLayerCount = 1;
		deviceCreateInfo.ppEnabledLayerNames = explicitLayers.data();
	}
	else {
		deviceCreateInfo.enabledLayerCount = 0;
		deviceCreateInfo.ppEnabledLayerNames = nullptr;
	}
#else
	deviceCreateInfo.enabledLayerCount = 0;
	deviceCreateInfo.ppEnabledLayerNames = nullptr;
#endif
	std::vector<const char*> deviceExtensions = { "VK_KHR_synchronization2",
		"VK_KHR_create_renderpass2",
		"VK_KHR_depth_stencil_resolve", 
		"VK_KHR_dynamic_rendering",
		"VK_KHR_maintenance4" };
	if (m_windowModule) {
		deviceExtensions.push_back("VK_KHR_swapchain");
		deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
		deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
	}
	else {
		deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
		deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
	}
	deviceCreateInfo.pEnabledFeatures = nullptr;
	NTSH_VK_CHECK(vkCreateDevice(m_physicalDevice, &deviceCreateInfo, nullptr, &m_device));

	vkGetDeviceQueue(m_device, m_graphicsQueueIndex, 0, &m_graphicsQueue);

	// Create the swapchain
	if (m_windowModule) {
		VkSurfaceCapabilitiesKHR surfaceCapabilities = getSurfaceCapabilities();
		uint32_t minImageCount = surfaceCapabilities.minImageCount + 1;
		if (surfaceCapabilities.maxImageCount > 0 && minImageCount > surfaceCapabilities.maxImageCount) {
			minImageCount = surfaceCapabilities.maxImageCount;
		}

		std::vector<VkSurfaceFormatKHR> surfaceFormats = getSurfaceFormats();
		m_swapchainFormat = surfaceFormats[0].format;
		VkColorSpaceKHR swapchainColorSpace = surfaceFormats[0].colorSpace;
		for (const VkSurfaceFormatKHR& surfaceFormat : surfaceFormats) {
			if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB && surfaceFormat.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR) {
				m_swapchainFormat = surfaceFormat.format;
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
		swapchainExtent.width = static_cast<uint32_t>(m_windowModule->getWidth());
		swapchainExtent.height = static_cast<uint32_t>(m_windowModule->getHeight());

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
		swapchainCreateInfo.imageFormat = m_swapchainFormat;
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
		swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;
		NTSH_VK_CHECK(vkCreateSwapchainKHR(m_device, &swapchainCreateInfo, nullptr, &m_swapchain));

		NTSH_VK_CHECK(vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_imageCount, nullptr));
		m_swapchainImages.resize(m_imageCount);
		NTSH_VK_CHECK(vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_imageCount, m_swapchainImages.data()));

		// Create the swapchain image views
		m_swapchainImageViews.resize(m_imageCount);
		for (uint32_t i = 0; i < m_imageCount; i++) {
			VkImageViewCreateInfo swapchainImageViewCreateInfo = {};
			swapchainImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			swapchainImageViewCreateInfo.pNext = nullptr;
			swapchainImageViewCreateInfo.flags = 0;
			swapchainImageViewCreateInfo.image = m_swapchainImages[i];
			swapchainImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			swapchainImageViewCreateInfo.format = m_swapchainFormat;
			swapchainImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
			swapchainImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
			swapchainImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
			swapchainImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
			swapchainImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			swapchainImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
			swapchainImageViewCreateInfo.subresourceRange.levelCount = 1;
			swapchainImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
			swapchainImageViewCreateInfo.subresourceRange.layerCount = 1;
			NTSH_VK_CHECK(vkCreateImageView(m_device, &swapchainImageViewCreateInfo, nullptr, &m_swapchainImageViews[i]));
		}
	}
	// Or create an image to draw on
	else {
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
		NTSH_VK_CHECK(vkCreateImage(m_device, &imageCreateInfo, nullptr, &m_drawImage));

		// Allocate memory for the image
		VkMemoryRequirements memoryRequirements;
		vkGetImageMemoryRequirements(m_device, m_drawImage, &memoryRequirements);

		VkPhysicalDeviceMemoryProperties memoryProperties = getMemoryProperties();
		uint32_t memoryTypeIndex = memoryProperties.memoryTypeCount;
		for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
			uint32_t memoryTypeBits = (1 << i);
			bool isRequiredMemoryType = memoryRequirements.memoryTypeBits & memoryTypeBits;

			VkMemoryPropertyFlags properties = memoryProperties.memoryTypes[i].propertyFlags;
			bool hasRequiredProperties = (properties & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) == VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

			if (isRequiredMemoryType && hasRequiredProperties) {
				memoryTypeIndex = i;
			}
		}
		NTSH_ASSERT(memoryTypeIndex < memoryProperties.memoryTypeCount);

		VkMemoryAllocateInfo memoryAllocateInfo = {};
		memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		memoryAllocateInfo.pNext = nullptr;
		memoryAllocateInfo.allocationSize = 268435456;
		memoryAllocateInfo.memoryTypeIndex = memoryTypeIndex;
		NTSH_VK_CHECK(vkAllocateMemory(m_device, &memoryAllocateInfo, nullptr, &m_drawImageMemory));

		NTSH_VK_CHECK(vkBindImageMemory(m_device, m_drawImage, m_drawImageMemory, 0));

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
		NTSH_VK_CHECK(vkCreateImageView(m_device, &imageViewCreateInfo, nullptr, &m_drawImageView));
	}

	// Create graphics pipeline
	VkFormat pipelineRenderingColorFormat = VK_FORMAT_R8G8B8A8_SRGB;
	if (m_windowModule) {
		pipelineRenderingColorFormat = m_swapchainFormat;
	}

	VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {};
	pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	pipelineRenderingCreateInfo.pNext = nullptr;
	pipelineRenderingCreateInfo.viewMask = 0;
	pipelineRenderingCreateInfo.colorAttachmentCount = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &pipelineRenderingColorFormat;
	pipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
	pipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

	const std::vector<uint32_t> vertexShaderCode = { 0x07230203,0x00010000,0x0008000a,0x00000036,0x00000000,0x00020011,0x00000001,0x0006000b,
	0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
	0x0008000f,0x00000000,0x00000004,0x6e69616d,0x00000000,0x0000001e,0x00000021,0x0000002b,
	0x00030003,0x00000002,0x000001cc,0x00040005,0x00000004,0x6e69616d,0x00000000,0x00050005,
	0x0000000c,0x69736f70,0x6e6f6974,0x00000073,0x00040005,0x00000017,0x6f6c6f63,0x00007372,
	0x00050005,0x0000001e,0x4374756f,0x726f6c6f,0x00000000,0x00060005,0x00000021,0x565f6c67,
	0x65747265,0x646e4978,0x00007865,0x00060005,0x00000029,0x505f6c67,0x65567265,0x78657472,
	0x00000000,0x00060006,0x00000029,0x00000000,0x505f6c67,0x7469736f,0x006e6f69,0x00070006,
	0x00000029,0x00000001,0x505f6c67,0x746e696f,0x657a6953,0x00000000,0x00070006,0x00000029,
	0x00000002,0x435f6c67,0x4470696c,0x61747369,0x0065636e,0x00070006,0x00000029,0x00000003,
	0x435f6c67,0x446c6c75,0x61747369,0x0065636e,0x00030005,0x0000002b,0x00000000,0x00040047,
	0x0000001e,0x0000001e,0x00000000,0x00040047,0x00000021,0x0000000b,0x0000002a,0x00050048,
	0x00000029,0x00000000,0x0000000b,0x00000000,0x00050048,0x00000029,0x00000001,0x0000000b,
	0x00000001,0x00050048,0x00000029,0x00000002,0x0000000b,0x00000003,0x00050048,0x00000029,
	0x00000003,0x0000000b,0x00000004,0x00030047,0x00000029,0x00000002,0x00020013,0x00000002,
	0x00030021,0x00000003,0x00000002,0x00030016,0x00000006,0x00000020,0x00040017,0x00000007,
	0x00000006,0x00000002,0x00040015,0x00000008,0x00000020,0x00000000,0x0004002b,0x00000008,
	0x00000009,0x00000003,0x0004001c,0x0000000a,0x00000007,0x00000009,0x00040020,0x0000000b,
	0x00000006,0x0000000a,0x0004003b,0x0000000b,0x0000000c,0x00000006,0x0004002b,0x00000006,
	0x0000000d,0x00000000,0x0004002b,0x00000006,0x0000000e,0xbf000000,0x0005002c,0x00000007,
	0x0000000f,0x0000000d,0x0000000e,0x0004002b,0x00000006,0x00000010,0x3f000000,0x0005002c,
	0x00000007,0x00000011,0x0000000e,0x00000010,0x0005002c,0x00000007,0x00000012,0x00000010,
	0x00000010,0x0006002c,0x0000000a,0x00000013,0x0000000f,0x00000011,0x00000012,0x00040017,
	0x00000014,0x00000006,0x00000003,0x0004001c,0x00000015,0x00000014,0x00000009,0x00040020,
	0x00000016,0x00000006,0x00000015,0x0004003b,0x00000016,0x00000017,0x00000006,0x0004002b,
	0x00000006,0x00000018,0x3f800000,0x0006002c,0x00000014,0x00000019,0x00000018,0x0000000d,
	0x0000000d,0x0006002c,0x00000014,0x0000001a,0x0000000d,0x0000000d,0x00000018,0x0006002c,
	0x00000014,0x0000001b,0x0000000d,0x00000018,0x0000000d,0x0006002c,0x00000015,0x0000001c,
	0x00000019,0x0000001a,0x0000001b,0x00040020,0x0000001d,0x00000003,0x00000014,0x0004003b,
	0x0000001d,0x0000001e,0x00000003,0x00040015,0x0000001f,0x00000020,0x00000001,0x00040020,
	0x00000020,0x00000001,0x0000001f,0x0004003b,0x00000020,0x00000021,0x00000001,0x00040020,
	0x00000023,0x00000006,0x00000014,0x00040017,0x00000026,0x00000006,0x00000004,0x0004002b,
	0x00000008,0x00000027,0x00000001,0x0004001c,0x00000028,0x00000006,0x00000027,0x0006001e,
	0x00000029,0x00000026,0x00000006,0x00000028,0x00000028,0x00040020,0x0000002a,0x00000003,
	0x00000029,0x0004003b,0x0000002a,0x0000002b,0x00000003,0x0004002b,0x0000001f,0x0000002c,
	0x00000000,0x00040020,0x0000002e,0x00000006,0x00000007,0x00040020,0x00000034,0x00000003,
	0x00000026,0x00050036,0x00000002,0x00000004,0x00000000,0x00000003,0x000200f8,0x00000005,
	0x0003003e,0x0000000c,0x00000013,0x0003003e,0x00000017,0x0000001c,0x0004003d,0x0000001f,
	0x00000022,0x00000021,0x00050041,0x00000023,0x00000024,0x00000017,0x00000022,0x0004003d,
	0x00000014,0x00000025,0x00000024,0x0003003e,0x0000001e,0x00000025,0x0004003d,0x0000001f,
	0x0000002d,0x00000021,0x00050041,0x0000002e,0x0000002f,0x0000000c,0x0000002d,0x0004003d,
	0x00000007,0x00000030,0x0000002f,0x00050051,0x00000006,0x00000031,0x00000030,0x00000000,
	0x00050051,0x00000006,0x00000032,0x00000030,0x00000001,0x00070050,0x00000026,0x00000033,
	0x00000031,0x00000032,0x0000000d,0x00000018,0x00050041,0x00000034,0x00000035,0x0000002b,
	0x0000002c,0x0003003e,0x00000035,0x00000033,0x000100fd,0x00010038 };

	VkShaderModule vertexShaderModule;
	VkShaderModuleCreateInfo vertexShaderModuleCreateInfo = {};
	vertexShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertexShaderModuleCreateInfo.pNext = nullptr;
	vertexShaderModuleCreateInfo.flags = 0;
	vertexShaderModuleCreateInfo.codeSize = vertexShaderCode.size() * sizeof(uint32_t);
	vertexShaderModuleCreateInfo.pCode = vertexShaderCode.data();
	NTSH_VK_CHECK(vkCreateShaderModule(m_device, &vertexShaderModuleCreateInfo, nullptr, &vertexShaderModule));

	VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo = {};
	vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderStageCreateInfo.pNext = nullptr;
	vertexShaderStageCreateInfo.flags = 0;
	vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexShaderStageCreateInfo.module = vertexShaderModule;
	vertexShaderStageCreateInfo.pName = "main";
	vertexShaderStageCreateInfo.pSpecializationInfo = nullptr;

	const std::vector<uint32_t> fragmentShaderCode = { 0x07230203,0x00010000,0x0008000a,0x00000013,0x00000000,0x00020011,0x00000001,0x0006000b,
	0x00000001,0x4c534c47,0x6474732e,0x3035342e,0x00000000,0x0003000e,0x00000000,0x00000001,
	0x0007000f,0x00000004,0x00000004,0x6e69616d,0x00000000,0x00000009,0x0000000c,0x00030010,
	0x00000004,0x00000007,0x00030003,0x00000002,0x000001cc,0x00040005,0x00000004,0x6e69616d,
	0x00000000,0x00050005,0x00000009,0x4374756f,0x726f6c6f,0x00000000,0x00040005,0x0000000c,
	0x6f6c6f63,0x00000072,0x00040047,0x00000009,0x0000001e,0x00000000,0x00040047,0x0000000c,
	0x0000001e,0x00000000,0x00020013,0x00000002,0x00030021,0x00000003,0x00000002,0x00030016,
	0x00000006,0x00000020,0x00040017,0x00000007,0x00000006,0x00000004,0x00040020,0x00000008,
	0x00000003,0x00000007,0x0004003b,0x00000008,0x00000009,0x00000003,0x00040017,0x0000000a,
	0x00000006,0x00000003,0x00040020,0x0000000b,0x00000001,0x0000000a,0x0004003b,0x0000000b,
	0x0000000c,0x00000001,0x0004002b,0x00000006,0x0000000e,0x3f800000,0x00050036,0x00000002,
	0x00000004,0x00000000,0x00000003,0x000200f8,0x00000005,0x0004003d,0x0000000a,0x0000000d,
	0x0000000c,0x00050051,0x00000006,0x0000000f,0x0000000d,0x00000000,0x00050051,0x00000006,
	0x00000010,0x0000000d,0x00000001,0x00050051,0x00000006,0x00000011,0x0000000d,0x00000002,
	0x00070050,0x00000007,0x00000012,0x0000000f,0x00000010,0x00000011,0x0000000e,0x0003003e,
	0x00000009,0x00000012,0x000100fd,0x00010038 };

	VkShaderModule fragmentShaderModule;
	VkShaderModuleCreateInfo fragmentShaderModuleCreateInfo = {};
	fragmentShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	fragmentShaderModuleCreateInfo.pNext = nullptr;
	fragmentShaderModuleCreateInfo.flags = 0;
	fragmentShaderModuleCreateInfo.codeSize = fragmentShaderCode.size() * sizeof(uint32_t);
	fragmentShaderModuleCreateInfo.pCode = fragmentShaderCode.data();
	NTSH_VK_CHECK(vkCreateShaderModule(m_device, &fragmentShaderModuleCreateInfo, nullptr, &fragmentShaderModule));

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
	colorBlendAttachmentState.blendEnable = VK_FALSE;
	colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.colorWriteMask = { VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT };

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
	dynamicStateCreateInfo.dynamicStateCount = 2;
	dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

	VkPipelineLayout pipelineLayout;
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.pNext = nullptr;
	pipelineLayoutCreateInfo.flags = 0;
	pipelineLayoutCreateInfo.setLayoutCount = 0;
	pipelineLayoutCreateInfo.pSetLayouts = nullptr;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;
	NTSH_VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

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
	graphicsPipelineCreateInfo.layout = pipelineLayout;
	graphicsPipelineCreateInfo.renderPass = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.subpass = 0;
	graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.basePipelineIndex = 0;
	NTSH_VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &m_graphicsPipeline));

	vkDestroyPipelineLayout(m_device, pipelineLayout, nullptr);
	vkDestroyShaderModule(m_device, vertexShaderModule, nullptr);
	vkDestroyShaderModule(m_device, fragmentShaderModule, nullptr);

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
	commandPoolCreateInfo.queueFamilyIndex = m_graphicsQueueIndex;

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
		NTSH_VK_CHECK(vkCreateCommandPool(m_device, &commandPoolCreateInfo, nullptr, &m_renderingCommandPools[i]));

		commandBufferAllocateInfo.commandPool = m_renderingCommandPools[i];
		NTSH_VK_CHECK(vkAllocateCommandBuffers(m_device, &commandBufferAllocateInfo, &m_renderingCommandBuffers[i]));

		// Create sync objects
		NTSH_VK_CHECK(vkCreateFence(m_device, &fenceCreateInfo, nullptr, &m_fences[i]));

		NTSH_VK_CHECK(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_imageAvailableSemaphores[i]));
	}
	for (uint32_t i = 0; i < m_imageCount; i++) {
		NTSH_VK_CHECK(vkCreateSemaphore(m_device, &semaphoreCreateInfo, nullptr, &m_renderFinishedSemaphores[i]));
	}

	// Set current frame-in-flight to 0
	currentFrameInFlight = 0;
}

void NutshellGraphicsModule::update(double dt) {
	NTSH_UNUSED(dt);

	NTSH_VK_CHECK(vkWaitForFences(m_device, 1, &m_fences[currentFrameInFlight], VK_TRUE, std::numeric_limits<uint64_t>::max()));

	uint32_t imageIndex = m_imageCount - 1;
	if (m_windowModule) {
		VkResult acquireNextImageResult = vkAcquireNextImageKHR(m_device, m_swapchain, std::numeric_limits<uint64_t>::max(), m_imageAvailableSemaphores[currentFrameInFlight], VK_NULL_HANDLE, &imageIndex);
		if (acquireNextImageResult == VK_ERROR_OUT_OF_DATE_KHR) {
			resize();
		}
		else if (acquireNextImageResult != VK_SUCCESS && acquireNextImageResult != VK_SUBOPTIMAL_KHR) {
			NTSH_MODULE_ERROR("Next swapchain image acquire failed.", NTSH_RESULT_MODULE_ERROR);
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
		emptySignalSubmitInfo.pSignalSemaphores = &m_imageAvailableSemaphores[currentFrameInFlight];
		NTSH_VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &emptySignalSubmitInfo, VK_NULL_HANDLE));
	}

	// Record rendering commands
	NTSH_VK_CHECK(vkResetCommandPool(m_device, m_renderingCommandPools[currentFrameInFlight], 0));

	// Begin command buffer recording
	VkCommandBufferBeginInfo commandBufferBeginInfo = {};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.pNext = nullptr;
	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	NTSH_VK_CHECK(vkBeginCommandBuffer(m_renderingCommandBuffers[currentFrameInFlight], &commandBufferBeginInfo));

	// Layout transition VK_IMAGE_LAYOUT_UNDEFINED -> VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	VkImageMemoryBarrier2 undefinedToColorAttachmentOptimalImageMemoryBarrier = {};
	undefinedToColorAttachmentOptimalImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.pNext = nullptr;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.srcAccessMask = 0;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueIndex;
	undefinedToColorAttachmentOptimalImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueIndex;
	if (m_windowModule) {
		undefinedToColorAttachmentOptimalImageMemoryBarrier.image = m_swapchainImages[imageIndex];
	}
	else {
		undefinedToColorAttachmentOptimalImageMemoryBarrier.image = m_drawImage;
	}
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

	m_vkCmdPipelineBarrier2KHR(m_renderingCommandBuffers[currentFrameInFlight], &undefinedToColorAttachmentOptimalDependencyInfo);

	// Begin rendering
	VkRenderingAttachmentInfo renderingAttachmentInfo = {};
	renderingAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	renderingAttachmentInfo.pNext = nullptr;
	if (m_windowModule) {
		renderingAttachmentInfo.imageView = m_swapchainImageViews[imageIndex];
	}
	else {
		renderingAttachmentInfo.imageView = m_drawImageView;
	}
	renderingAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	renderingAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
	renderingAttachmentInfo.resolveImageView = VK_NULL_HANDLE;
	renderingAttachmentInfo.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	renderingAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	renderingAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	renderingAttachmentInfo.clearValue.color = { 0.0f, 0.0f, 0.0f, 0.0f };
	renderingAttachmentInfo.clearValue.depthStencil = { 0.0f, 0 };

	VkRenderingInfo renderingInfo = {};
	renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderingInfo.pNext = nullptr;
	renderingInfo.flags = 0;
	renderingInfo.renderArea = m_scissor;
	renderingInfo.layerCount = 1;
	renderingInfo.viewMask = 0;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments = &renderingAttachmentInfo;
	renderingInfo.pDepthAttachment = nullptr;
	renderingInfo.pStencilAttachment = nullptr;
	m_vkCmdBeginRenderingKHR(m_renderingCommandBuffers[currentFrameInFlight], &renderingInfo);

	// Bind graphics pipeline
	vkCmdBindPipeline(m_renderingCommandBuffers[currentFrameInFlight], VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
	vkCmdSetViewport(m_renderingCommandBuffers[currentFrameInFlight], 0, 1, &m_viewport);
	vkCmdSetScissor(m_renderingCommandBuffers[currentFrameInFlight], 0, 1, &m_scissor);

	// Draw
	vkCmdDraw(m_renderingCommandBuffers[currentFrameInFlight], 3, 1, 0, 0);

	// End rendering
	m_vkCmdEndRenderingKHR(m_renderingCommandBuffers[currentFrameInFlight]);

	// Layout transition VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL -> VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
	if (m_windowModule) {
		VkImageMemoryBarrier2 colorAttachmentOptimalToPresentSrcImageMemoryBarrier = {};
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.pNext = nullptr;
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.dstAccessMask = 0;
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueIndex;
		colorAttachmentOptimalToPresentSrcImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueIndex;
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

		m_vkCmdPipelineBarrier2KHR(m_renderingCommandBuffers[currentFrameInFlight], &colorAttachmentOptimalToPresentSrcDependencyInfo);
	}

	// End command buffer recording
	NTSH_VK_CHECK(vkEndCommandBuffer(m_renderingCommandBuffers[currentFrameInFlight]));

	NTSH_VK_CHECK(vkResetFences(m_device, 1, &m_fences[currentFrameInFlight]));

	VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &m_imageAvailableSemaphores[currentFrameInFlight];
	submitInfo.pWaitDstStageMask = &waitDstStageMask;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_renderingCommandBuffers[currentFrameInFlight];
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &m_renderFinishedSemaphores[imageIndex];
	NTSH_VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_fences[currentFrameInFlight]));

	if (m_windowModule) {
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
			NTSH_MODULE_ERROR("Queue present swapchain image failed.", NTSH_RESULT_MODULE_ERROR);
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
		NTSH_VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &emptyWaitSubmitInfo, VK_NULL_HANDLE));
	}

	currentFrameInFlight = (currentFrameInFlight + 1) % m_framesInFlight;
}

void NutshellGraphicsModule::destroy() {
	NTSH_VK_CHECK(vkQueueWaitIdle(m_graphicsQueue));

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

	// Destroy graphics pipeline
	vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);

	// Destroy swapchain
	if (m_windowModule) {
		for (VkImageView& swapchainImageView : m_swapchainImageViews) {
			vkDestroyImageView(m_device, swapchainImageView, nullptr);
		}
		vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
	}
	// Or destroy the image
	else {
		vkDestroyImageView(m_device, m_drawImageView, nullptr);
		vkDestroyImage(m_device, m_drawImage, nullptr);
		vkFreeMemory(m_device, m_drawImageMemory, nullptr);
	}

	// Destroy device
	vkDestroyDevice(m_device, nullptr);

	// Destroy surface
	if (m_windowModule) {
		vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
	}

#ifdef NTSH_OS_LINUX
	// Close X display
	XCloseDisplay(m_display);
#endif

#ifdef NTSH_DEBUG
	// Destroy debug messenger
	auto destroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT");
	destroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
#endif

	// Destroy instance
	vkDestroyInstance(m_instance, nullptr);
}

VkSurfaceCapabilitiesKHR NutshellGraphicsModule::getSurfaceCapabilities() {
	VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo = {};
	surfaceInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR;
	surfaceInfo.pNext = nullptr;
	surfaceInfo.surface = m_surface;

	VkSurfaceCapabilities2KHR surfaceCapabilities;
	surfaceCapabilities.sType = VK_STRUCTURE_TYPE_SURFACE_CAPABILITIES_2_KHR;
	surfaceCapabilities.pNext = nullptr;
	NTSH_VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilities2KHR(m_physicalDevice, &surfaceInfo, &surfaceCapabilities));

	return surfaceCapabilities.surfaceCapabilities;
}

std::vector<VkSurfaceFormatKHR> NutshellGraphicsModule::getSurfaceFormats() {
	VkPhysicalDeviceSurfaceInfo2KHR surfaceInfo = {};
	surfaceInfo.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SURFACE_INFO_2_KHR;
	surfaceInfo.pNext = nullptr;
	surfaceInfo.surface = m_surface;

	uint32_t surfaceFormatsCount;
	NTSH_VK_CHECK(vkGetPhysicalDeviceSurfaceFormats2KHR(m_physicalDevice, &surfaceInfo, &surfaceFormatsCount, nullptr));
	std::vector<VkSurfaceFormat2KHR> surfaceFormats2(surfaceFormatsCount);
	for (VkSurfaceFormat2KHR& surfaceFormat2 : surfaceFormats2) {
		surfaceFormat2.sType = VK_STRUCTURE_TYPE_SURFACE_FORMAT_2_KHR;
		surfaceFormat2.pNext = nullptr;
	}
	NTSH_VK_CHECK(vkGetPhysicalDeviceSurfaceFormats2KHR(m_physicalDevice, &surfaceInfo, &surfaceFormatsCount, surfaceFormats2.data()));
	
	std::vector<VkSurfaceFormatKHR> surfaceFormats;
	for (const VkSurfaceFormat2KHR surfaceFormat2 : surfaceFormats2) {
		surfaceFormats.push_back(surfaceFormat2.surfaceFormat);
	}

	return surfaceFormats;
}

std::vector<VkPresentModeKHR> NutshellGraphicsModule::getSurfacePresentModes() {
	uint32_t presentModesCount;
	NTSH_VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModesCount, nullptr));
	std::vector<VkPresentModeKHR> presentModes(presentModesCount);
	NTSH_VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModesCount, presentModes.data()));

	return presentModes;
}

VkPhysicalDeviceMemoryProperties NutshellGraphicsModule::getMemoryProperties() {
	VkPhysicalDeviceMemoryProperties2 memoryProperties = {};
	memoryProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MEMORY_PROPERTIES_2;
	memoryProperties.pNext = nullptr;
	vkGetPhysicalDeviceMemoryProperties2(m_physicalDevice, &memoryProperties);

	return memoryProperties.memoryProperties;
}

void NutshellGraphicsModule::resize() {
	if (m_windowModule) {
		while (m_windowModule->getWidth() == 0 || m_windowModule->getHeight() == 0) {
			m_windowModule->pollEvents();
		}

		NTSH_VK_CHECK(vkQueueWaitIdle(m_graphicsQueue));

		// Destroy swapchain image views
		for (VkImageView& swapchainImageView : m_swapchainImageViews) {
			vkDestroyImageView(m_device, swapchainImageView, nullptr);
		}

		// Recreate the swapchain
		VkSurfaceCapabilitiesKHR surfaceCapabilities = getSurfaceCapabilities();
		uint32_t minImageCount = surfaceCapabilities.minImageCount + 1;
		if (surfaceCapabilities.maxImageCount > 0 && minImageCount > surfaceCapabilities.maxImageCount) {
			minImageCount = surfaceCapabilities.maxImageCount;
		}

		std::vector<VkSurfaceFormatKHR> surfaceFormats = getSurfaceFormats();
		m_swapchainFormat = surfaceFormats[0].format;
		VkColorSpaceKHR swapchainColorSpace = surfaceFormats[0].colorSpace;
		for (const VkSurfaceFormatKHR& surfaceFormat : surfaceFormats) {
			if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB && surfaceFormat.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR) {
				m_swapchainFormat = surfaceFormat.format;
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
		swapchainExtent.width = static_cast<uint32_t>(m_windowModule->getWidth());
		swapchainExtent.height = static_cast<uint32_t>(m_windowModule->getHeight());

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
		swapchainCreateInfo.imageFormat = m_swapchainFormat;
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
		VkSwapchainKHR oldSwapchain = m_swapchain;
		swapchainCreateInfo.oldSwapchain = oldSwapchain;
		NTSH_VK_CHECK(vkCreateSwapchainKHR(m_device, &swapchainCreateInfo, nullptr, &m_swapchain));

		NTSH_VK_CHECK(vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_imageCount, nullptr));
		m_swapchainImages.resize(m_imageCount);
		NTSH_VK_CHECK(vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_imageCount, m_swapchainImages.data()));

		// Create the swapchain image views
		m_swapchainImageViews.resize(m_imageCount);
		for (uint32_t i = 0; i < m_imageCount; i++) {
			VkImageViewCreateInfo swapchainImageViewCreateInfo = {};
			swapchainImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			swapchainImageViewCreateInfo.pNext = nullptr;
			swapchainImageViewCreateInfo.flags = 0;
			swapchainImageViewCreateInfo.image = m_swapchainImages[i];
			swapchainImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			swapchainImageViewCreateInfo.format = m_swapchainFormat;
			swapchainImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
			swapchainImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
			swapchainImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
			swapchainImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
			swapchainImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			swapchainImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
			swapchainImageViewCreateInfo.subresourceRange.levelCount = 1;
			swapchainImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
			swapchainImageViewCreateInfo.subresourceRange.layerCount = 1;
			NTSH_VK_CHECK(vkCreateImageView(m_device, &swapchainImageViewCreateInfo, nullptr, &m_swapchainImageViews[i]));
		}
	}
}

extern "C" NTSH_MODULE_API NutshellGraphicsModuleInterface* createModule() {
	return new NutshellGraphicsModule;
}

extern "C" NTSH_MODULE_API void destroyModule(NutshellGraphicsModuleInterface* m) {
	delete m;
}