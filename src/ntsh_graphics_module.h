#pragma once
#include "../external/Common/module_interfaces/ntsh_graphics_module_interface.h"
#include "../external/Common/utils/ntsh_engine_defines.h"
#include "../external/Common/utils/ntsh_engine_enums.h"
#include "../external/Module/utils/ntsh_module_defines.h"
#include "../external/glslang/glslang/Include/ShHandle.h"
#include "../external/glslang/SPIRV/GlslangToSpv.h"
#include "../external/glslang/StandAlone/DirStackFileIncluder.h"
#if defined(NTSH_OS_WINDOWS)
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(NTSH_OS_LINUX)
#define VK_USE_PLATFORM_XLIB_KHR
#endif
#include "vulkan/vulkan.h"
#include <vector>
#include <filesystem>

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

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData) {
	NTSH_UNUSED(messageSeverity);
	NTSH_UNUSED(messageType);
	NTSH_UNUSED(pUserData);

	NTSH_VK_VALIDATION(pCallbackData->pMessage);

	return VK_FALSE;
}

const float toRad = 3.1415926535897932384626433832795f / 180.0f;

struct PushConstants {
	float time;
	uint32_t width;
	uint32_t height;
	float padding;
	float cameraPosition[4];
	float cameraDirection[4];
};

class NutshellGraphicsModule : public NutshellGraphicsModuleInterface {
public:
	NutshellGraphicsModule() : NutshellGraphicsModuleInterface("Nutshell Graphics Vulkan Raymarching Module") {}

	void init();
	void update(double dt);
	void destroy();

private:
	// Surface-related functions
	VkSurfaceCapabilitiesKHR getSurfaceCapabilities();
	std::vector<VkSurfaceFormatKHR> getSurfaceFormats();
	std::vector<VkPresentModeKHR> getSurfacePresentModes();

	VkPhysicalDeviceMemoryProperties getMemoryProperties();

	std::vector<uint32_t> compileFragmentShader();
	bool recreateGraphicsPipeline();

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
	VkSurfaceKHR m_surface;

	VkPhysicalDevice m_physicalDevice;
	uint32_t m_graphicsQueueIndex;
	VkQueue m_graphicsQueue;
	VkDevice m_device;

	VkViewport m_viewport;
	VkRect2D m_scissor;

	VkSwapchainKHR m_swapchain;
	std::vector<VkImage> m_swapchainImages;
	std::vector<VkImageView> m_swapchainImageViews;
	VkFormat m_swapchainFormat;

	VkImage m_drawImage;
	VkImageView m_drawImageView;
	VkDeviceMemory m_drawImageMemory;

	bool m_glslangInitialized = false;
	const std::string m_fragmentShaderName = "raymarching.frag";
	const std::string m_raymarchingHelperFileName = "raymarching_helper.glsl";
	const std::string m_sceneFileName = "scene.glsl";
	std::filesystem::file_time_type m_fragmentShaderLastModified;
	std::filesystem::file_time_type m_raymarchingHelperLastModified;
	std::filesystem::file_time_type m_sceneLastModified;
	VkFormat m_pipelineRenderingColorFormat = VK_FORMAT_R8G8B8A8_SRGB;
	VkPipelineRenderingCreateInfo m_pipelineRenderingCreateInfo{};
	VkShaderModule m_vertexShaderModule;
	VkPipelineShaderStageCreateInfo m_vertexShaderStageCreateInfo{};
	VkShaderModule m_fragmentShaderModule = VK_NULL_HANDLE;
	VkPipelineVertexInputStateCreateInfo m_vertexInputStateCreateInfo{};
	VkPipelineInputAssemblyStateCreateInfo m_inputAssemblyStateCreateInfo{};
	VkPipelineViewportStateCreateInfo m_viewportStateCreateInfo{};
	VkPipelineRasterizationStateCreateInfo m_rasterizationStateCreateInfo{};
	VkPipelineMultisampleStateCreateInfo m_multisampleStateCreateInfo{};
	VkPipelineDepthStencilStateCreateInfo m_depthStencilStateCreateInfo{};
	VkPipelineColorBlendAttachmentState m_colorBlendAttachmentState{};
	VkPipelineColorBlendStateCreateInfo m_colorBlendStateCreateInfo{};
	std::array<VkDynamicState, 2> m_dynamicStates = { VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_VIEWPORT };
	VkPipelineDynamicStateCreateInfo m_dynamicStateCreateInfo{};
	VkPipeline m_graphicsPipeline = VK_NULL_HANDLE;
	VkPipelineLayout m_graphicsPipelineLayout;

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
	uint32_t currentFrameInFlight;

	bool m_mouseMiddleMode = true;
	const float m_cameraSpeed = 0.0015f;
	const float m_mouseSensitivity = 0.12f;
	int m_prevMouseX = 0;
	int m_prevMouseY = 0;
	float m_yaw = 0.0f;
	float m_pitch = 0.0f;

	float m_cameraPosition[3] = {0.0f, 1.0f, -2.0f};
	float m_cameraDirection[3] = {0.0f, 0.0f, 1.0f};
};