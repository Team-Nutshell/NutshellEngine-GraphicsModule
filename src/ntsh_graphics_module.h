#pragma once
#include "../external/Common/module_interfaces/ntsh_graphics_module_interface.h"
#include "../external/Common/resources/ntsh_resources_graphics.h"
#include "../external/Common/utils/ntsh_engine_defines.h"
#include "../external/Common/utils/ntsh_engine_enums.h"
#include "../external/Module/utils/ntsh_module_defines.h"
#include "../external/nml/include/nml.h"
#if defined(NTSH_OS_WINDOWS)
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(NTSH_OS_LINUX)
#define VK_USE_PLATFORM_XLIB_KHR
#endif
#include "../external/VulkanMemoryAllocator/include/vk_mem_alloc.h"
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

const float toRad = 3.1415926535897932384626433832795f / 180.0f;

struct Mesh {
	uint32_t indexCount;
	uint32_t firstIndex;
	int32_t vertexOffset;
};

struct Object {
	uint32_t index;

	nml::vec3 position;
	nml::vec3 rotation;
	nml::vec3 scale;

	size_t meshIndex;

	uint32_t textureID;
};

class NutshellGraphicsModule : public NutshellGraphicsModuleInterface {
public:
	NutshellGraphicsModule() : NutshellGraphicsModuleInterface("Nutshell Graphics Vulkan Model Module") {}

	void init();
	void update(double dt);
	void destroy();

	// Loads the mesh described in the mesh parameter in the internal format and returns a unique identifier
	NtshMeshId load(const NtshMesh mesh);
	// Loads the image described in the image parameter in the internal format and returns a unique identifier
	NtshImageId load(const NtshImage image);

private:
	// Surface-related functions
	VkSurfaceCapabilitiesKHR getSurfaceCapabilities();
	std::vector<VkSurfaceFormatKHR> getSurfaceFormats();
	std::vector<VkPresentModeKHR> getSurfacePresentModes();

	VkPhysicalDeviceMemoryProperties getMemoryProperties();

	uint32_t findMipLevels(uint32_t width, uint32_t height);

	// Swapchain creation
	void createSwapchain(VkSwapchainKHR oldSwapchain);

	// Vertex and index buffers creation
	void createVertexAndIndexBuffers();

	// Depth image creation
	void createDepthImage();

	// Graphics pipeline creation
	void createGraphicsPipeline();

	// Descriptor sets creation
	void createDescriptorSets();
	void updateDescriptorSet(uint32_t frameInFlight);

	// Default resources
	void createDefaultResources();

	// Scene
	void createScene();

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

	VkImage m_depthImage;
	VmaAllocation m_depthImageAllocation;
	VkImageView m_depthImageView;

	VmaAllocator m_allocator;

	VkBuffer m_vertexBuffer;
	VmaAllocation m_vertexBufferAllocation;
	VkBuffer m_indexBuffer;
	VmaAllocation m_indexBufferAllocation;

	VkPipeline m_graphicsPipeline;
	VkPipelineLayout m_graphicsPipelineLayout;

	VkDescriptorSetLayout m_descriptorSetLayout;
	VkDescriptorPool m_descriptorPool;
	std::vector<VkDescriptorSet> m_descriptorSets;
	std::vector<bool> m_descriptorSetsNeedUpdate;

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

	nml::vec3 cameraPosition;
	std::vector<VkBuffer> m_cameraBuffers;
	std::vector<VmaAllocation> m_cameraBufferAllocations;

	std::vector<VkBuffer> m_objectBuffers;
	std::vector<VmaAllocation> m_objectBufferAllocations;

	std::vector<Mesh> m_meshes;
	int32_t m_currentVertexOffset = 0;
	uint32_t m_currentIndexOffset = 0;

	std::vector<VkImage> m_textureImages;
	std::vector<VmaAllocation> m_textureImageAllocations;
	std::vector<VkImageView> m_textureImageViews;
	VkSampler m_textureSampler;

	std::vector<Object> m_objects;
	float m_objectAngle = 0.0f;
	float m_objectRotationSpeed = 0.12f;
};