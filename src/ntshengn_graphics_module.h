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
#include <string>
#include <vector>
#include <limits>
#include <unordered_map>
#include <set>
#include <queue>

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

enum class ShaderType {
	Vertex,
	TesselationControl,
	TesselationEvaluation,
	Geometry,
	Fragment,
};

struct InternalMesh {
	uint32_t indexCount;
	uint32_t firstIndex;
	int32_t vertexOffset;
};

struct InternalTexture {
	uint32_t imageIndex = 0;
	std::string samplerKey = "defaultSampler";
};

struct InternalMaterial {
	uint32_t diffuseTextureIndex = 0;
	uint32_t normalTextureIndex = 1;
	uint32_t metalnessTextureIndex = 2;
	uint32_t roughnessTextureIndex = 3;
	uint32_t occlusionTextureIndex = 4;
	uint32_t emissiveTextureIndex = 5;
	float emissiveFactor = 1.0f;
	float alphaCutoff = 0.0f;
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

	NtshEngn::MeshID meshID = 0;
	uint32_t materialIndex = 0;
};

struct InternalLight {
	NtshEngn::Math::vec4 position = { 0.0f, 0.0f, 0.0f, 0.0f };
	NtshEngn::Math::vec4 direction = { 0.0f, 0.0f, 0.0f, 0.0f };
	NtshEngn::Math::vec4 color = { 0.0f, 0.0f, 0.0f, 0.0f };
	NtshEngn::Math::vec4 cutoff = { 0.0f, 0.0f, 0.0f, 0.0f };
};

struct InternalLights {
	std::set<NtshEngn::Entity> directionalLights;
	std::set<NtshEngn::Entity> pointLights;
	std::set<NtshEngn::Entity> spotLights;
};

enum class UIElement {
	Text,
	Line,
	Rectangle
};

struct InternalUIText {
	NtshEngn::FontID fontID;
	NtshEngn::Math::vec4 color = { 0.0f, 0.0f, 0.0f, 1.0f };

	uint32_t charactersCount = 0;
	uint32_t bufferOffset = 0;
};

struct InternalUILine {
	NtshEngn::Math::vec4 positions = { 0.0f, 0.0f, 0.0f, 0.0f };
	NtshEngn::Math::vec4 color = { 0.0f, 0.0f, 0.0f, 1.0f };
};

struct InternalUIRectangle {
	NtshEngn::Math::vec4 positions = { 0.0f, 0.0f, 0.0f, 0.0f };
	NtshEngn::Math::vec4 color = { 0.0f, 0.0f, 0.0f, 1.0f };
};

namespace NtshEngn {

	class GraphicsModule : public GraphicsModuleInterface {
	public:
		GraphicsModule() : GraphicsModuleInterface("NutshellEngine Vulkan Renderer Graphics Module") {}

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

		// Color and depth images creation
		void createColorAndDepthImages();

		// Descriptor set layout creation
		void createDescriptorSetLayout();

		// Shader compilation
		std::vector<uint32_t> compileShader(const std::string& shaderCode, ShaderType type);

		// Graphics pipeline creation
		void createGraphicsPipeline();

		// Descriptor sets creation
		void createDescriptorSets();
		void updateDescriptorSet(uint32_t frameInFlight);

		// Tone mapping resources
		void createToneMappingResources();

		// UI resources
		void createUIResources();
		void createUITextResources();
		void updateUITextDescriptorSet(uint32_t frameInFlight);
		void createUILineResources();
		void createUIRectangleResources();

		// Default resources
		void createDefaultResources();

		// On window resize
		void resize();

		// Create sampler
		std::string createSampler(const ImageSampler& sampler);

		// Add to textures
		uint32_t addToTextures(const InternalTexture& texture);

		// Add to materials
		uint32_t addToMaterials(const InternalMaterial& material);

		// Attribute an InternalObject index
		uint32_t attributeObjectIndex();

		// Retrieve an InternalObject index
		void retrieveObjectIndex(uint32_t objectIndex);

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
		std::vector<bool> m_descriptorSetsNeedUpdate;

		VkSampler m_toneMappingSampler;
		VkDescriptorSetLayout m_toneMappingDescriptorSetLayout;
		VkDescriptorPool m_toneMappingDescriptorPool;
		VkDescriptorSet m_toneMappingDescriptorSet;
		VkPipeline m_toneMappingGraphicsPipeline;
		VkPipelineLayout m_toneMappingGraphicsPipelineLayout;

		std::vector<VkBuffer> m_uiTextBuffers;
		std::vector<VmaAllocation> m_uiTextBufferAllocations;
		VkSampler m_uiFontNearestSampler;
		VkSampler m_uiFontLinearSampler;
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

		std::vector<VkBuffer> m_cameraBuffers;
		std::vector<VmaAllocation> m_cameraBufferAllocations;

		std::vector<VkBuffer> m_objectBuffers;
		std::vector<VmaAllocation> m_objectBufferAllocations;

		std::vector<VkBuffer> m_materialBuffers;
		std::vector<VmaAllocation> m_materialBufferAllocations;

		std::vector<VkBuffer> m_lightBuffers;
		std::vector<VmaAllocation> m_lightBufferAllocations;

		Mesh m_defaultMesh;
		Image m_defaultDiffuseTexture;
		Image m_defaultNormalTexture;
		Image m_defaultMetalnessTexture;
		Image m_defaultRoughnessTexture;
		Image m_defaultOcclusionTexture;
		Image m_defaultEmissiveTexture;

		std::vector<InternalMesh> m_meshes;
		int32_t m_currentVertexOffset = 0;
		uint32_t m_currentIndexOffset = 0;
		std::unordered_map<const Mesh*, uint32_t> m_meshAddresses;

		std::vector<VkImage> m_textureImages;
		std::vector<VmaAllocation> m_textureImageAllocations;
		std::vector<VkImageView> m_textureImageViews;
		std::unordered_map<std::string, VkSampler> m_textureSamplers;
		std::unordered_map<const Image*, uint32_t> m_imageAddresses;
		std::vector<InternalTexture> m_textures;

		std::vector<InternalMaterial> m_materials;

		std::vector<InternalFont> m_fonts;
		std::unordered_map<const Font*, uint32_t> m_fontAddresses;

		std::unordered_map<Entity, InternalObject> m_objects;
		std::vector<uint32_t> m_freeObjectsIndices{ 0 };

		Entity m_mainCamera = std::numeric_limits<uint32_t>::max();

		InternalLights m_lights;

		std::queue<UIElement> m_uiElements;

		std::queue<InternalUIText> m_uiTexts;
		uint32_t m_uiTextBufferOffset = 0;

		std::queue<InternalUILine> m_uiLines;

		std::queue<InternalUIRectangle> m_uiRectangles;
	};

}