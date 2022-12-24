#pragma once
#include "../external/Common/module_interfaces/ntsh_graphics_module_interface.h"
#include "../external/Common/utils/ntsh_engine_defines.h"
#include "../external/Common/utils/ntsh_engine_enums.h"
#include "../external/Module/utils/ntsh_module_defines.h"
#if defined(NTSH_OS_WINDOWS)
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(NTSH_OS_LINUX)
#define VK_USE_PLATFORM_XLIB_KHR
#endif
#include "vulkan/vulkan.h"
#include <vector>

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
	NutshellGraphicsModule() : NutshellGraphicsModuleInterface("Nutshell Graphics Vulkan Triangle Module") {}

	void init();
	void update(double dt);
	void destroy();

private:
	// Surface-related functions
	VkSurfaceCapabilitiesKHR getSurfaceCapabilities();
	std::vector<VkSurfaceFormatKHR> getSurfaceFormats();
	std::vector<VkPresentModeKHR> getSurfacePresentModes();

	VkPhysicalDeviceMemoryProperties getMemoryProperties();

	// On window resize
	void resize();

private:
	VkInstance m_instance;
#if defined(NTSH_DEBUG)
	VkDebugUtilsMessengerEXT m_debugMessenger;
#endif

#if defined(NTSH_OS_LINUX)
	Display* m_display = nullptr;
#endif
	VkSurfaceKHR m_surface = VK_NULL_HANDLE;

	VkPhysicalDevice m_physicalDevice;
	uint32_t m_graphicsQueueIndex;
	VkQueue m_graphicsQueue;
	VkDevice m_device;

	VkViewport m_viewport;
	VkRect2D m_scissor;

	VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
	std::vector<VkImage> m_swapchainImages;
	std::vector<VkImageView> m_swapchainImageViews;
	VkFormat m_swapchainFormat;

	VkImage m_drawImage;
	VkImageView m_drawImageView;
	VkDeviceMemory m_drawImageMemory;

	VkPipeline m_graphicsPipeline;

	std::vector<VkCommandPool> m_renderingCommandPools;
	std::vector<VkCommandBuffer> m_renderingCommandBuffers;

	std::vector<VkFence> m_fences;
	std::vector<VkSemaphore> m_imageAvailableSemaphores;
	std::vector<VkSemaphore> m_renderFinishedSemaphores;

	PFN_vkCmdBeginRenderingKHR m_vkCmdBeginRenderingKHR;
	PFN_vkCmdEndRenderingKHR m_vkCmdEndRenderingKHR;
	PFN_vkCmdPipelineBarrier2KHR m_vkCmdPipelineBarrier2KHR;

	uint32_t m_imageCount;
	uint32_t m_framesInFlight;
	uint32_t m_currentFrameInFlight;
};