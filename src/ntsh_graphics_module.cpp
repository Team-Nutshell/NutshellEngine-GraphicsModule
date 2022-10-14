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
}

void NutshellGraphicsModule::update(double dt) {
	NTSH_UNUSED(dt);
	NTSH_MODULE_WARNING("update() function not implemented.");
}

void NutshellGraphicsModule::destroy() {
	vkDestroySurfaceKHR(m_instance, m_surface, nullptr);

#ifdef NTSH_OS_LINUX
	XCloseDisplay(m_display);
#endif

#ifdef NTSH_DEBUG
	auto destroyDebugUtilsMessengerEXT = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT");
	destroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
#endif

	vkDestroyInstance(m_instance, nullptr);
}

extern "C" NTSH_MODULE_API NutshellGraphicsModuleInterface* createModule() {
	return new NutshellGraphicsModule;
}

extern "C" NTSH_MODULE_API void destroyModule(NutshellGraphicsModuleInterface* m) {
	delete m;
}