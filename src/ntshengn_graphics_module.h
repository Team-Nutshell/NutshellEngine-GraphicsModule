#pragma once
#include "../Common/module_interfaces/ntshengn_graphics_module_interface.h"
#include "../Common/utils/ntshengn_defines.h"
#include "../Common/utils/ntshengn_enums.h"
#include "../Module/utils/ntshengn_module_defines.h"
#include "../Common/module_interfaces/ntshengn_window_module_interface.h"
#if defined(NTSHENGN_OS_WINDOWS)
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(NTSHENGN_OS_LINUX)
#define VK_USE_PLATFORM_XLIB_KHR
#endif
#include "vulkan/vulkan.h"
#if defined(NTSHENGN_OS_LINUX)
#undef None
#undef Success
#endif
#include <vector>

#define NTSHENGN_VK_CHECK(f) \
	do { \
		int64_t check = f; \
		if (check) { \
			NTSHENGN_MODULE_ERROR("Vulkan Error.\nError code: " + std::to_string(check) + "\nFile: " + std::string(__FILE__) + "\nFunction: " + #f + "\nLine: " + std::to_string(__LINE__), NtshEngn::Result::UnknownError); \
		} \
	} while(0)

#define NTSHENGN_VK_VALIDATION(m) \
	do { \
		NTSHENGN_MODULE_WARNING("Vulkan Validation Layer: " + std::string(m)); \
	} while(0)

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
	NTSHENGN_UNUSED(messageSeverity);
	NTSHENGN_UNUSED(messageType);
	NTSHENGN_UNUSED(pCallbackData);
	NTSHENGN_UNUSED(pUserData);

	NTSHENGN_VK_VALIDATION(pCallbackData->pMessage);

	return VK_FALSE;
}

struct PerWindowResources {
	NtshEngn::WindowID windowID;
	VkSurfaceKHR surface = VK_NULL_HANDLE;
	VkSwapchainKHR swapchain = VK_NULL_HANDLE;
	uint32_t swapchainImageCount;
	std::vector<VkImage> swapchainImages;
	std::vector<VkImageView> swapchainImageViews;
	std::vector<VkSemaphore> renderFinishedSemaphores;
	std::vector<VkSemaphore> imageAvailableSemaphores;
	VkViewport viewport;
	VkRect2D scissor;
};

namespace NtshEngn {

	class GraphicsModule : public GraphicsModuleInterface {
	public:
		GraphicsModule() : GraphicsModuleInterface("NutshellEngine Vulkan Multi-Window Graphics Module") {}

		void init();
		void update(double dt);
		void destroy();

		// Loads the mesh described in the mesh parameter in the internal format and returns a unique identifier
		MeshID load(const Mesh& mesh);
		// Loads the image described in the image parameter in the internal format and returns a unique identifier
		ImageID load(const Image& image);

	private:
		// Surface-related functions
		VkSurfaceCapabilitiesKHR getSurfaceCapabilities(size_t index);
		std::vector<VkSurfaceFormatKHR> getSurfaceFormats(size_t);
		std::vector<VkPresentModeKHR> getSurfacePresentModes(size_t);

		VkPhysicalDeviceMemoryProperties getMemoryProperties();

		// Per window functions
		void createSwapchain(size_t index);
		void createWindowResources(WindowID windowID);
		std::vector<PerWindowResources>::iterator destroyWindowResources(const std::vector<PerWindowResources>::iterator perWindowResources);
		void resize(size_t index);

	private:
		VkInstance m_instance;
#if defined(NTSHENGN_DEBUG)
		VkDebugUtilsMessengerEXT m_debugMessenger;
#endif

		VkPhysicalDevice m_physicalDevice;
		uint32_t m_graphicsQueueIndex;
		VkQueue m_graphicsQueue;
		VkDevice m_device;

		std::vector<PerWindowResources> m_perWindowResources;
		VkFormat m_swapchainFormat;

		VkImage m_drawImage;
		VkImageView m_drawImageView;
		VkDeviceMemory m_drawImageMemory;

		VkPipeline m_graphicsPipeline;

		std::vector<VkCommandPool> m_renderingCommandPools;
		std::vector<VkCommandBuffer> m_renderingCommandBuffers;

		std::vector<VkFence> m_fences;

		PFN_vkCmdBeginRenderingKHR m_vkCmdBeginRenderingKHR;
		PFN_vkCmdEndRenderingKHR m_vkCmdEndRenderingKHR;
		PFN_vkCmdPipelineBarrier2KHR m_vkCmdPipelineBarrier2KHR;

		uint32_t m_framesInFlight;
		uint32_t m_currentFrameInFlight;
	};

}