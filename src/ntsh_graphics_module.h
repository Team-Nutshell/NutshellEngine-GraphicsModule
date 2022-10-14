#pragma once
#include "../external/Common/module_interfaces/ntsh_graphics_module_interface.h"
#include "../external/Common/ntsh_engine_defines.h"
#include "../external/Common/ntsh_engine_enums.h"
#include "../external/Module/ntsh_module_defines.h"
#ifdef NTSH_OS_WINDOWS
#define VK_USE_PLATFORM_WIN32_KHR
#elif NTSH_OS_LINUX
#define VK_USE_PLATFORM_XLIB_KHR
#endif
#include "vulkan/vulkan.h"

#define NTSH_VK_CHECK(f) \
	do { \
		int64_t check = f; \
		if (check) { \
			NTSH_MODULE_ERROR("Vulkan Error.\nError code: " + std::to_string(check) + "\nFile: " + std::string(__FILE__) + "\nFunction: " + #f + "\nLine: " + std::to_string(__LINE__), NTSH_RESULT_UNKNOWN_ERROR); \
		} \
	} while(0)

#define NTSH_VK_VALIDATION(m) \
	do { \
		NTSH_MODULE_WARNING("Vulkan Validation Layer: " + std::string(m)); \
	} while(0)

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT * pCallbackData, void* pUserData) {
	NTSH_UNUSED(messageSeverity);
	NTSH_UNUSED(messageType);
	NTSH_UNUSED(pUserData);
	
	NTSH_VK_VALIDATION(pCallbackData->pMessage);

	return VK_FALSE;
}

class NutshellGraphicsModule : public NutshellGraphicsModuleInterface {
public:
	NutshellGraphicsModule() : NutshellGraphicsModuleInterface("Nutshell Graphics Test Module") {}

	void init();
	void update(double dt);
	void destroy();

private:
	VkInstance m_instance;
#ifdef NTSH_OS_LINUX
	Display* m_display = nullptr;
#endif
	VkSurfaceKHR m_surface;
#ifdef NTSH_DEBUG
	VkDebugUtilsMessengerEXT m_debugMessenger;
#endif
};