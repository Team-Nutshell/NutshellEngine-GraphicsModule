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
		// Loads the font described in the font parameter in the internal format and returns a unique identifier
		FontID load(const Font& font);

		// Sets the background color
		void setBackgroundColor(const Math::vec4& backgroundColor);

		// Plays an animation for an entity, indexed in the entity's model animation list
		void playAnimation(Entity entity, uint32_t animationIndex);
		// Pauses an animation played by an entity
		void pauseAnimation(Entity entity);
		// Stops an animation played by an entity
		void stopAnimation(Entity entity);

		// Returns true if the entity is currently playing the animation with index animationIndex, else, returns false
		bool isAnimationPlaying(Entity entity, uint32_t animationIndex);

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

		Math::vec4 m_backgroundColor = Math::vec4(0.0f, 0.0f, 0.0f, 0.0f);
	};

}