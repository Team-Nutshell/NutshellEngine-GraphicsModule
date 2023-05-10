#pragma once
#include "../external/Common/module_interfaces/ntshengn_graphics_module_interface.h"
#include "../external/Common/utils/ntshengn_defines.h"
#include "../external/Common/utils/ntshengn_enums.h"
#include "../external/Module/utils/ntshengn_module_defines.h"
#include "../external/glslang/glslang/Include/ShHandle.h"
#include "../external/glslang/SPIRV/GlslangToSpv.h"
#include "../external/glslang/StandAlone/DirStackFileIncluder.h"
#include "../external/nml/include/nml.h"
#if defined(NTSHENGN_OS_WINDOWS)
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(NTSHENGN_OS_LINUX)
#define VK_USE_PLATFORM_XLIB_KHR
#endif
#include "../external/VulkanMemoryAllocator/include/vk_mem_alloc.h"
#if defined(NTSHENGN_OS_LINUX)
#undef None
#undef Success
#endif
#include <vector>
#include <set>
#include <filesystem>

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

const float toRad = 3.1415926535897932384626433832795f / 180.0f;

struct PushConstants {
	float time;
	uint32_t width;
	uint32_t height;
	float padding;
	float cameraPosition[4];
	float cameraDirection[4];
};

struct InternalLight {
	nml::vec4 position = { 0.0f, 0.0f, 0.0f, 0.0f };
	nml::vec4 direction = { 0.0f, 0.0f, 0.0f, 0.0f };
	nml::vec4 color = { 0.0f, 0.0f, 0.0f, 0.0f };
	nml::vec4 cutoffs = { 0.0f, 0.0f, 0.0f, 0.0f };
};

struct InternalLights {
	std::set<NtshEngn::Entity> directionalLights;
	std::set<NtshEngn::Entity> pointLights;
	std::set<NtshEngn::Entity> spotLights;
};


namespace NtshEngn {

	class GraphicsModule : public GraphicsModuleInterface {
	public:
		GraphicsModule() : GraphicsModuleInterface("NutshellEngine Graphics Vulkan Ray Marching Module") {}

		void init();
		void update(double dt);
		void destroy();

		// Loads the mesh described in the mesh parameter in the internal format and returns a unique identifier
		NtshEngn::MeshId load(const NtshEngn::Mesh& mesh);
		// Loads the image described in the image parameter in the internal format and returns a unique identifier
		NtshEngn::ImageId load(const NtshEngn::Image& image);

	private:
		// Surface-related functions
		VkSurfaceCapabilitiesKHR getSurfaceCapabilities();
		std::vector<VkSurfaceFormatKHR> getSurfaceFormats();
		std::vector<VkPresentModeKHR> getSurfacePresentModes();

		VkPhysicalDeviceMemoryProperties getMemoryProperties();

		// Color image creation
		void createColorImage();

		// Descriptor set layout creation
		void createDescriptorSetLayout();

		std::vector<uint32_t> compileFragmentShader();
		bool recreateGraphicsPipeline();

		// Swapchain creation
		void createSwapchain(VkSwapchainKHR oldSwapchain);

		// Tone mapping resources
		void createToneMappingResources();

		// On window resize
		void resize();

	public:
		const ComponentMask getComponentMask() const;

		void onEntityComponentAdded(Entity entity, Component componentID);
		void onEntityComponentRemoved(Entity entity, Component componentID);

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
		VkFormat m_swapchainFormat;

		VkImage m_drawImage;
		VmaAllocation m_drawImageAllocation;
		VkImageView m_drawImageView;

		VkImage m_colorImage;
		VmaAllocation m_colorImageAllocation;
		VkImageView m_colorImageView;

		VmaAllocator m_allocator;

		bool m_glslangInitialized = false;
		const std::string m_fragmentShaderName = "raymarching.frag";
		const std::string m_raymarchingHelperFileName = "raymarching_helper.glsl";
		const std::string m_sceneFileName = "scene.glsl";
		std::filesystem::file_time_type m_fragmentShaderLastModified;
		std::filesystem::file_time_type m_raymarchingHelperLastModified;
		std::filesystem::file_time_type m_sceneLastModified;
		VkFormat m_pipelineRenderingColorFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
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

		VkDescriptorSetLayout m_descriptorSetLayout;
		VkDescriptorPool m_descriptorPool;
		std::vector<VkDescriptorSet> m_descriptorSets;

		VkSampler m_toneMappingSampler;
		VkDescriptorSetLayout m_toneMappingDescriptorSetLayout;
		VkDescriptorPool m_toneMappingDescriptorPool;
		VkDescriptorSet m_toneMappingDescriptorSet;
		VkPipeline m_toneMappingGraphicsPipeline;
		VkPipelineLayout m_toneMappingGraphicsPipelineLayout;

		std::vector<VkCommandPool> m_renderingCommandPools;
		std::vector<VkCommandBuffer> m_renderingCommandBuffers;

		std::vector<VkFence> m_fences;
		std::vector<VkSemaphore> m_imageAvailableSemaphores;
		std::vector<VkSemaphore> m_renderFinishedSemaphores;

		VkFence m_initializationFence;

		PFN_vkCmdBeginRenderingKHR m_vkCmdBeginRenderingKHR;
		PFN_vkCmdEndRenderingKHR m_vkCmdEndRenderingKHR;
		PFN_vkCmdPipelineBarrier2KHR m_vkCmdPipelineBarrier2KHR;

		uint32_t m_imageCount;
		uint32_t m_framesInFlight;
		uint32_t m_currentFrameInFlight;

		std::vector<VkBuffer> m_lightBuffers;
		std::vector<VmaAllocation> m_lightBufferAllocations;

		InternalLights m_lights;

		Entity m_mainCamera = std::numeric_limits<uint32_t>::max();
	};

}