#pragma once
#include "../Common/module_interfaces/ntshengn_graphics_module_interface.h"
#include "../Common/resources/ntshengn_resources_graphics.h"
#include "../Common/utils/ntshengn_defines.h"
#include "../Common/utils/ntshengn_enums.h"
#include "../Common/utils/ntshengn_utils_math.h"
#include "../Module/utils/ntshengn_module_defines.h"
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
#include <limits>
#include <unordered_map>

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

enum class ShaderType {
	Vertex,
	TesselationControl,
	TesselationEvaluation,
	Geometry,
	Fragment
};

struct InternalMesh {
	uint32_t indexCount;
	uint32_t firstIndex;
	int32_t vertexOffset;
};

struct InternalTexture {
	NtshEngn::ImageID imageID = 0;
	std::string samplerKey = "defaultSampler";
};

struct InternalFont {
	VkImage image;
	VmaAllocation imageAllocation;
	VkImageView imageView;

	NtshEngn::ImageSamplerFilter filter;

	std::unordered_map<char, NtshEngn::FontGlyph> glyphs;
};

struct InternalObject {
	uint32_t index;

	NtshEngn::MeshID boxMeshIndex = NTSHENGN_MESH_UNKNOWN;
	NtshEngn::MeshID sphereMeshIndex = NTSHENGN_MESH_UNKNOWN;
	NtshEngn::MeshID capsuleMeshIndex = NTSHENGN_MESH_UNKNOWN;
	uint32_t textureIndex = 0;
};

enum class UIElement {
	Text,
	Line,
	Rectangle,
	Image
};

struct InternalUIText {
	NtshEngn::Math::vec4 color = { 0.0f, 0.0f, 0.0f, 0.0f };
	NtshEngn::FontID fontID;

	uint32_t charactersCount = 0;
	uint32_t bufferOffset = 0;
};

struct InternalUILine {
	NtshEngn::Math::vec4 positions = { 0.0f, 0.0f, 0.0f, 0.0f };
	NtshEngn::Math::vec4 color = { 0.0f, 0.0f, 0.0f, 0.0f };
};

struct InternalUIRectangle {
	NtshEngn::Math::vec4 positions = { 0.0f, 0.0f, 0.0f, 0.0f };
	NtshEngn::Math::vec4 color = { 0.0f, 0.0f, 0.0f, 0.0f };
};

struct InternalUIImage {
	NtshEngn::Math::vec4 color = { 0.0f, 0.0f, 0.0f, 0.0f };
	uint32_t uiTextureIndex;

	NtshEngn::Math::vec2 v0 = { 0.0f, 0.0f };
	NtshEngn::Math::vec2 v1 = { 0.0f, 0.0f };
	NtshEngn::Math::vec2 v2 = { 0.0f, 0.0f };
	NtshEngn::Math::vec2 v3 = { 0.0f, 0.0f };
};

namespace NtshEngn {

	class GraphicsModule : public GraphicsModuleInterface {
	public:
		GraphicsModule() : GraphicsModuleInterface("NutshellEngine Vulkan Collider Graphics Module") {}

		void init();
		void update(double dt);
		void destroy();

		// Loads the mesh described in the mesh parameter in the internal format and returns a unique identifier
		MeshID load(const Mesh& mesh);
		// Loads the image described in the image parameter in the internal format and returns a unique identifier
		ImageID load(const Image& image);
		// Loads the font described in the font parameter in the internal format and returns a unique identifier
		FontID load(const Font& font);

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

	public:
		const ComponentMask getComponentMask() const;

		void onEntityComponentAdded(Entity entity, Component componentID);
		void onEntityComponentRemoved(Entity entity, Component componentID);

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

		// Descriptor set layout creation
		void createDescriptorSetLayout();

		// Shader compilation
		std::vector<uint32_t> compileShader(const std::string& shaderCode, ShaderType type);

		// Graphics pipeline creation
		void createGraphicsPipeline();

		// Descriptor sets creation
		void createDescriptorSets();

		// UI resources
		void createUIResources();
		void createUITextResources();
		void updateUITextDescriptorSet(uint32_t frameInFlight);
		void createUILineResources();
		void createUIRectangleResources();
		void createUIImageResources();
		void updateUIImageDescriptorSet(uint32_t frameInFlight);

		// Default resources
		void createDefaultResources();

		// On window resize
		void resize();

		// Attribute an InternalObject index
		uint32_t attributeObjectIndex();

		// Retrieve an InternalObject index
		void retrieveObjectIndex(uint32_t objectIndex);

		// Create meshes for colliders
		MeshID createBox(const Math::vec3& center, const Math::vec3& halfExtent, const Math::vec3& rotation);
		MeshID createSphere(const Math::vec3& center, float radius);
		MeshID createCapsule(const Math::vec3& base, const Math::vec3& tip, float radius);

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

		bool m_glslangInitialized = false;

		VkPipeline m_graphicsPipeline;
		VkPipelineLayout m_graphicsPipelineLayout;

		VkDescriptorSetLayout m_descriptorSetLayout;
		VkDescriptorPool m_descriptorPool;
		std::vector<VkDescriptorSet> m_descriptorSets;

		VkSampler m_uiNearestSampler;
		VkSampler m_uiLinearSampler;

		std::vector<VkBuffer> m_uiTextBuffers;
		std::vector<VmaAllocation> m_uiTextBufferAllocations;
		VkDescriptorSetLayout m_uiTextDescriptorSetLayout;
		VkDescriptorPool m_uiTextDescriptorPool;
		std::vector<VkDescriptorSet> m_uiTextDescriptorSets;
		std::vector<bool> m_uiTextDescriptorSetsNeedUpdate;
		VkPipeline m_uiTextGraphicsPipeline;
		VkPipelineLayout m_uiTextGraphicsPipelineLayout;

		VkPipeline m_uiLineGraphicsPipeline;
		VkPipelineLayout m_uiLineGraphicsPipelineLayout;

		VkPipeline m_uiRectangleGraphicsPipeline;
		VkPipelineLayout m_uiRectangleGraphicsPipelineLayout;

		VkDescriptorSetLayout m_uiImageDescriptorSetLayout;
		VkDescriptorPool m_uiImageDescriptorPool;
		std::vector<VkDescriptorSet> m_uiImageDescriptorSets;
		std::vector<bool> m_uiImageDescriptorSetsNeedUpdate;
		VkPipeline m_uiImageGraphicsPipeline;
		VkPipelineLayout m_uiImageGraphicsPipelineLayout;

		std::vector<VkCommandPool> m_renderingCommandPools;
		std::vector<VkCommandBuffer> m_renderingCommandBuffers;

		std::vector<VkFence> m_fences;
		std::vector<VkSemaphore> m_imageAvailableSemaphores;
		std::vector<VkSemaphore> m_renderFinishedSemaphores;

		VkCommandPool m_initializationCommandPool;
		VkCommandBuffer m_initializationCommandBuffer;

		VkFence m_initializationFence;

		PFN_vkCmdBeginRenderingKHR m_vkCmdBeginRenderingKHR;
		PFN_vkCmdEndRenderingKHR m_vkCmdEndRenderingKHR;
		PFN_vkCmdPipelineBarrier2KHR m_vkCmdPipelineBarrier2KHR;

		uint32_t m_imageCount;
		uint32_t m_framesInFlight;
		uint32_t m_currentFrameInFlight;

		std::vector<VkBuffer> m_cameraBuffers;
		std::vector<VmaAllocation> m_cameraBufferAllocations;

		std::vector<VkBuffer> m_objectBuffers;
		std::vector<VmaAllocation> m_objectBufferAllocations;

		Mesh m_defaultMesh;

		std::vector<InternalMesh> m_meshes;
		int32_t m_currentVertexOffset = 0;
		uint32_t m_currentIndexOffset = 0;
		std::unordered_map<const Mesh*, uint32_t> m_meshAddresses;

		std::vector<VkImage> m_textureImages;
		std::vector<VmaAllocation> m_textureImageAllocations;
		std::vector<VkImageView> m_textureImageViews;
		std::vector<Math::vec2> m_textureSizes;
		std::unordered_map<const Image*, ImageID> m_imageAddresses;

		std::vector<InternalFont> m_fonts;
		std::unordered_map<const Font*, FontID> m_fontAddresses;

		std::vector<std::pair<ImageID, ImageSamplerFilter>> m_uiTextures;

		std::unordered_map<Entity, InternalObject> m_objects;
		std::vector<uint32_t> m_freeObjectsIndices{ 0 };

		Entity m_mainCamera = NTSHENGN_ENTITY_UNKNOWN;

		std::queue<UIElement> m_uiElements;

		std::queue<InternalUIText> m_uiTexts;
		uint32_t m_uiTextBufferOffset = 0;

		std::queue<InternalUILine> m_uiLines;

		std::queue<InternalUIRectangle> m_uiRectangles;

		std::queue<InternalUIImage> m_uiImages;
	};

}