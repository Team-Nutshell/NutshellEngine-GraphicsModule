#pragma once
#include "../Common/module_interfaces/ntshengn_graphics_module_interface.h"
#include "../Common/utils/ntshengn_defines.h"
#include "../Common/utils/ntshengn_enums.h"
#include "../Module/utils/ntshengn_module_defines.h"
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

namespace NtshEngn {

	class GraphicsModule : public GraphicsModuleInterface {
	public:
		GraphicsModule() : GraphicsModuleInterface("NutshellEngine Vulkan Triangle Graphics Module") {}

		void init();
		void update(double dt);
		void destroy();

		// Loads the mesh described in the mesh parameter in the internal format and returns a unique identifier
		MeshID load(const Mesh& mesh);
		// Loads the image described in the image parameter in the internal format and returns a unique identifier
		ImageID load(const Image& image);
		// Loads the font described in the font parameter in the internal format and returns a unique identifier
		FontID load(const Font& font);

		// Draws a text on the UI with the font in the fontID parameter using the position on screen and color
		void drawUIText(FontID fontID, const std::string& text, const Math::vec2& position, const Math::vec4& color);
		// Draws a line on the UI according to its start and end points and its color
		void drawUILine(const Math::vec2& start, const Math::vec2& end, const Math::vec4& color);
		// Draws a rectangle on the UI according to its position, its size (width and height) and its color
		void drawUIRectangle(const Math::vec2& position, const Math::vec2& size, const Math::vec4& color);
		// Draws an image on the UI according to its sampler filter, position, rotation, scale and color to multiply the image with
		void drawUIImage(ImageID imageID, ImageSamplerFilter imageSamplerFilter, const Math::vec2& position, float rotation, const Math::vec2& scale, const Math::vec4& color);

	private:
		// Surface-related functions
		VkSurfaceCapabilitiesKHR getSurfaceCapabilities();
		std::vector<VkSurfaceFormatKHR> getSurfaceFormats();
		std::vector<VkPresentModeKHR> getSurfacePresentModes();

		VkPhysicalDeviceMemoryProperties getMemoryProperties();

		// Create swapchain
		void createSwapchain(VkSwapchainKHR oldSwapchain);

		// On window resize
		void resize();

	private:
		VkInstance m_instance;
#if defined(NTSHENGN_DEBUG)
		VkDebugUtilsMessengerEXT m_debugMessenger;
#endif

		VkSurfaceKHR m_surface = VK_NULL_HANDLE;

		VkPhysicalDevice m_physicalDevice;
		uint32_t m_graphicsQueueFamilyIndex;
		VkQueue m_graphicsQueue;
		VkDevice m_device;

		VkViewport m_viewport;
		VkRect2D m_scissor;

		VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
		std::vector<VkImage> m_swapchainImages;
		std::vector<VkImageView> m_swapchainImageViews;

		VkFormat m_drawImageFormat;

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

}