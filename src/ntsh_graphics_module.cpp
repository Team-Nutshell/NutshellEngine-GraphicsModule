#include "ntsh_graphics_module.h"
#include "../external/Module/ntsh_dynamic_library.h"
#include "../external/Common/module_interfaces/ntsh_window_module_interface.h"

void NutshellGraphicsModule::init() {
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
	uint32_t propertyCount;
	NTSH_VK_CHECK(vkEnumerateInstanceLayerProperties(&propertyCount, nullptr));
	std::vector<VkLayerProperties> properties(propertyCount);
	NTSH_VK_CHECK(vkEnumerateInstanceLayerProperties(&propertyCount, properties.data()));

	for (const VkLayerProperties& availableLayer : properties) {
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
	std::vector<const char*> extensions;
#if NTSH_DEBUG
	extensions.push_back("VK_EXT_debug_utils");
#endif
	if (m_windowModule) {
		extensions.push_back("VK_KHR_surface");
#ifdef NTSH_OS_WINDOWS
		extensions.push_back("VK_KHR_win32_surface");
#elif NTSH_OS_LINUX
		extensions.push_back("VK_KHR_xlib_surface");
#endif
		instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		instanceCreateInfo.ppEnabledExtensionNames = extensions.data();
	}
	else {
		instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		instanceCreateInfo.ppEnabledExtensionNames = extensions.data();
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

	// Create surface
	if (m_windowModule) {
#ifdef NTSH_OS_WINDOWS
		HWND windowHandle = m_windowModule->getWindowHandle();
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
		Window windowHandle = m_windowModule->getWindowHandle();
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
	
	// Create the logical device
	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pNext = nullptr;
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
	std::array<const char*, 1> deviceExtensions = { "VK_KHR_swapchain" };
	if (m_windowModule) {
		deviceCreateInfo.enabledExtensionCount = 1;
		deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();
	}
	else {
		deviceCreateInfo.enabledExtensionCount = 0;
		deviceCreateInfo.ppEnabledExtensionNames = nullptr;
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
		VkFormat swapchainFormat = surfaceFormats[0].format;
		VkColorSpaceKHR swapchainColorSpace = surfaceFormats[0].colorSpace;
		for (const VkSurfaceFormatKHR& surfaceFormat : surfaceFormats) {
			if (surfaceFormat.format == VK_FORMAT_B8G8R8A8_SRGB && surfaceFormat.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR) {
				swapchainFormat = surfaceFormat.format;
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
		swapchainExtent.width = static_cast<uint32_t>(m_windowModule->getWindowWidth());
		swapchainExtent.height = static_cast<uint32_t>(m_windowModule->getWindowHeight());

		VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
		swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapchainCreateInfo.pNext = nullptr;
		swapchainCreateInfo.flags = 0;
		swapchainCreateInfo.surface = m_surface;
		swapchainCreateInfo.minImageCount = minImageCount;
		swapchainCreateInfo.imageFormat = swapchainFormat;
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

		uint32_t swapchainImagesCount;
		NTSH_VK_CHECK(vkGetSwapchainImagesKHR(m_device, m_swapchain, &swapchainImagesCount, nullptr));
		m_swapchainImages.resize(swapchainImagesCount);
		NTSH_VK_CHECK(vkGetSwapchainImagesKHR(m_device, m_swapchain, &swapchainImagesCount, m_swapchainImages.data()));

		m_swapchainImageViews.resize(swapchainImagesCount);
		for (uint32_t i = 0; i < swapchainImagesCount; i++) {
			VkImageViewCreateInfo imageViewCreateInfo = {};
			imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			imageViewCreateInfo.pNext = nullptr;
			imageViewCreateInfo.flags = 0;
			imageViewCreateInfo.image = m_swapchainImages[i];
			imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
			imageViewCreateInfo.format = swapchainFormat;
			imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
			imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
			imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
			imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
			imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
			imageViewCreateInfo.subresourceRange.levelCount = 1;
			imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
			imageViewCreateInfo.subresourceRange.layerCount = 1;
			NTSH_VK_CHECK(vkCreateImageView(m_device, &imageViewCreateInfo, nullptr, &m_swapchainImageViews[i]));
		}
	}
}

void NutshellGraphicsModule::update(double dt) {
	NTSH_UNUSED(dt);
	NTSH_MODULE_WARNING("update() function not implemented.");
}

void NutshellGraphicsModule::destroy() {
	// Destroy swapchain
	if (m_windowModule) {
		for (VkImageView& swapchainImageView : m_swapchainImageViews) {
			vkDestroyImageView(m_device, swapchainImageView, nullptr);
		}
		vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
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
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	NTSH_VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &surfaceCapabilities));

	return surfaceCapabilities;
}

std::vector<VkSurfaceFormatKHR> NutshellGraphicsModule::getSurfaceFormats() {
	uint32_t surfaceFormatsCount;
	NTSH_VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &surfaceFormatsCount, nullptr));
	std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatsCount);
	NTSH_VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &surfaceFormatsCount, surfaceFormats.data()));

	return surfaceFormats;
}

std::vector<VkPresentModeKHR> NutshellGraphicsModule::getSurfacePresentModes() {
	uint32_t presentModesCount;
	NTSH_VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModesCount, nullptr));
	std::vector<VkPresentModeKHR> presentModes(presentModesCount);
	NTSH_VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModesCount, presentModes.data()));

	return presentModes;
}

extern "C" NTSH_MODULE_API NutshellGraphicsModuleInterface* createModule() {
	return new NutshellGraphicsModule;
}

extern "C" NTSH_MODULE_API void destroyModule(NutshellGraphicsModuleInterface* m) {
	delete m;
}