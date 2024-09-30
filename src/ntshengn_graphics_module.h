#pragma once
#include "../Common/modules/ntshengn_graphics_module_interface.h"
#include "../Common/resources/ntshengn_resources_graphics.h"
#include "../Common/utils/ntshengn_defines.h"
#include "../Common/utils/ntshengn_enums.h"
#include "../Common/utils/ntshengn_utils_math.h"
#include "../Common/utils/ntshengn_utils_block_suballocator.h"
#include "../Module/utils/ntshengn_module_defines.h"
#if defined(NTSHENGN_OS_WINDOWS)
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(NTSHENGN_OS_LINUX) || defined(NTSHENGN_OS_FREEBSD)
#define VK_USE_PLATFORM_XLIB_KHR
#endif
#include "../external/VulkanMemoryAllocator/include/vk_mem_alloc.h"
#if defined(NTSHENGN_OS_LINUX) || defined(NTSHENGN_OS_FREEBSD)
#undef None
#undef Success
#endif
#include <string>
#include <vector>
#include <forward_list>
#include <limits>
#include <unordered_map>
#include <set>
#include <queue>
#include <utility>
#include <tuple>

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

bool operator==(const NtshEngn::ImageSampler& lhs, const NtshEngn::ImageSampler& rhs) {
	return (lhs.magFilter == rhs.magFilter) &&
		(lhs.minFilter == rhs.minFilter) &&
		(lhs.mipmapFilter == rhs.mipmapFilter) &&
		(lhs.addressModeU == rhs.addressModeU) &&
		(lhs.addressModeV == rhs.addressModeV) &&
		(lhs.addressModeW == rhs.addressModeW) &&
		(lhs.borderColor == rhs.borderColor) &&
		(lhs.anisotropyLevel == rhs.anisotropyLevel);
}

bool operator==(const NtshEngn::Material& lhs, const NtshEngn::Material& rhs) {
	return (lhs.diffuseTexture.image == rhs.diffuseTexture.image) &&
		(lhs.diffuseTexture.imageSampler == rhs.diffuseTexture.imageSampler) &&
		(lhs.normalTexture.image == rhs.normalTexture.image) &&
		(lhs.normalTexture.imageSampler == rhs.normalTexture.imageSampler) &&
		(lhs.metalnessTexture.image == rhs.metalnessTexture.image) &&
		(lhs.metalnessTexture.imageSampler == rhs.metalnessTexture.imageSampler) &&
		(lhs.roughnessTexture.image == rhs.roughnessTexture.image) &&
		(lhs.roughnessTexture.imageSampler == rhs.roughnessTexture.imageSampler) &&
		(lhs.occlusionTexture.image == rhs.occlusionTexture.image) &&
		(lhs.occlusionTexture.imageSampler == rhs.occlusionTexture.imageSampler) &&
		(lhs.emissiveTexture.image == rhs.emissiveTexture.image) &&
		(lhs.emissiveTexture.imageSampler == rhs.emissiveTexture.imageSampler) &&
		(lhs.emissiveFactor == rhs.emissiveFactor) &&
		(lhs.alphaCutoff == rhs.alphaCutoff) &&
		(lhs.indexOfRefraction == rhs.indexOfRefraction);
}

struct HostVisibleBuffer {
	VkBuffer handle;
	void* address;
	VmaAllocation allocation;
};

struct DeviceBuffer {
	VkBuffer handle;
	VmaAllocation allocation;
};

enum class ShaderType {
	Vertex,
	TesselationControl,
	TesselationEvaluation,
	Geometry,
	Fragment,
	Compute
};

struct InternalMesh {
	uint32_t indexCount;
	uint32_t firstIndex;
	int32_t vertexOffset;

	uint32_t jointCount;
};

struct InternalTexture {
	NtshEngn::ImageID imageID = 0;
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
	uint32_t jointTransformOffset = 0;
	uint32_t materialIndex = 0;
};

struct PlayingAnimation {
	uint32_t animationIndex;
	float time = 0.0f;
	bool isPlaying = true;
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
	std::set<NtshEngn::Entity> ambientLights;
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
	NtshEngn::Math::vec2 reverseUV = { 0.0f, 0.0f };
};

struct Particle {
	NtshEngn::Math::vec3 position = { 0.0f, 0.0f, 0.0f };
	float size = 0.0f;
	NtshEngn::Math::vec4 color = { 0.0f, 0.0f, 0.0f, 0.0f };
	NtshEngn::Math::vec3 direction = { 0.0f, 0.0f, 0.0f };
	float speed = 0.0f;
	float duration = 0.0f;
	NtshEngn::Math::vec3 padding = { 0.0f, 0.0f, 0.0f };
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

		// Emits particles described by particleEmitter
		void emitParticles(const ParticleEmitter& particleEmitter);

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

		// Particles resources
		void createParticleResources();

		// Tone mapping resources
		void createToneMappingResources();

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

		// Load Renderable for entity
		void loadRenderableForEntity(Entity entity);

		// Create sampler
		std::string createSampler(const ImageSampler& sampler);

		// Add to textures
		uint32_t addToTextures(const InternalTexture& texture);

		// Add to materials
		uint32_t addToMaterials(const InternalMaterial& material);

		// Find previous animation keyframe
		uint32_t findPreviousAnimationKeyframe(float time, const std::vector<AnimationChannelKeyframe>& keyframes);

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
		uint32_t m_graphicsComputeQueueFamilyIndex;
		VkQueue m_graphicsComputeQueue;
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

		std::array<DeviceBuffer, 2> m_particleBuffers;
		std::vector<HostVisibleBuffer> m_particleStagingBuffers;
		DeviceBuffer m_particleDrawIndirectBuffer;
		std::vector<bool> m_particleBuffersNeedUpdate;
		VkDescriptorSetLayout m_particleComputeDescriptorSetLayout;
		VkDescriptorPool m_particleComputeDescriptorPool;
		std::array<VkDescriptorSet, 2> m_particleComputeDescriptorSets;
		VkPipeline m_particleComputePipeline;
		VkPipelineLayout m_particleComputePipelineLayout;
		VkDescriptorSetLayout m_particleGraphicsDescriptorSetLayout;
		VkDescriptorPool m_particleGraphicsDescriptorPool;
		std::vector<VkDescriptorSet> m_particleGraphicsDescriptorSets;
		VkPipeline m_particleGraphicsPipeline;
		VkPipelineLayout m_particleGraphicsPipelineLayout;
		uint32_t m_inParticleBufferCurrentIndex = 0;
		uint32_t m_maxParticlesNumber = 100000;
		size_t m_currentParticleHostSize = 0;

		VkSampler m_toneMappingSampler;
		VkDescriptorSetLayout m_toneMappingDescriptorSetLayout;
		VkDescriptorPool m_toneMappingDescriptorPool;
		VkDescriptorSet m_toneMappingDescriptorSet;
		VkPipeline m_toneMappingGraphicsPipeline;
		VkPipelineLayout m_toneMappingGraphicsPipelineLayout;

		VkSampler m_uiNearestSampler;
		VkSampler m_uiLinearSampler;

		std::vector<HostVisibleBuffer> m_uiTextBuffers;
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

		std::vector<HostVisibleBuffer> m_cameraBuffers;

		std::vector<HostVisibleBuffer> m_objectBuffers;

		VkBuffer m_meshBuffer;
		VmaAllocation m_meshBufferAllocation;

		std::vector<HostVisibleBuffer> m_jointTransformBuffers;

		std::vector<HostVisibleBuffer> m_materialBuffers;

		std::vector<HostVisibleBuffer> m_lightBuffers;

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
		uint32_t m_currentJointOffset = 0;
		std::unordered_map<const Mesh*, MeshID> m_meshAddresses;

		std::vector<VkImage> m_textureImages;
		std::vector<VmaAllocation> m_textureImageAllocations;
		std::vector<VkImageView> m_textureImageViews;
		std::vector<Math::vec2> m_textureSizes;
		std::unordered_map<std::string, VkSampler> m_textureSamplers;
		std::unordered_map<const Image*, ImageID> m_imageAddresses;
		std::vector<InternalTexture> m_textures;

		std::vector<InternalMaterial> m_materials;

		std::unordered_map<Entity, InternalObject> m_objects;
		std::vector<uint32_t> m_freeObjectsIndices{ 0 };
		BlockSuballocator m_freeJointTransformOffsets{ 4096 };
		std::unordered_map<Entity, Material> m_lastKnownMaterial;

		std::unordered_map<InternalObject*, PlayingAnimation> m_playingAnimations;

		Entity m_mainCamera = NTSHENGN_ENTITY_UNKNOWN;

		InternalLights m_lights;

		Math::vec4 m_backgroundColor = Math::vec4(0.0f, 0.0f, 0.0f, 0.0f);

		std::vector<InternalFont> m_fonts;
		std::unordered_map<const Font*, FontID> m_fontAddresses;

		std::vector<std::pair<ImageID, ImageSamplerFilter>> m_uiTextures;

		std::queue<UIElement> m_uiElements;

		std::queue<InternalUIText> m_uiTexts;
		uint32_t m_uiTextBufferOffset = 0;

		std::queue<InternalUILine> m_uiLines;

		std::queue<InternalUIRectangle> m_uiRectangles;

		std::queue<InternalUIImage> m_uiImages;

		const std::unordered_map<ImageSamplerFilter, VkFilter> m_filterMap{ { ImageSamplerFilter::Linear, VK_FILTER_LINEAR },
			{ ImageSamplerFilter::Nearest, VK_FILTER_NEAREST },
			{ ImageSamplerFilter::Unknown, VK_FILTER_LINEAR }
		};
		const std::unordered_map<ImageSamplerFilter, VkSamplerMipmapMode> m_mipmapFilterMap{ { ImageSamplerFilter::Linear, VK_SAMPLER_MIPMAP_MODE_LINEAR },
			{ ImageSamplerFilter::Nearest, VK_SAMPLER_MIPMAP_MODE_NEAREST },
			{ ImageSamplerFilter::Unknown, VK_SAMPLER_MIPMAP_MODE_LINEAR }
		};
		const std::unordered_map<ImageSamplerAddressMode, VkSamplerAddressMode> m_addressModeMap{ { ImageSamplerAddressMode::Repeat, VK_SAMPLER_ADDRESS_MODE_REPEAT },
			{ ImageSamplerAddressMode::MirroredRepeat, VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT },
			{ ImageSamplerAddressMode::ClampToEdge, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE },
			{ ImageSamplerAddressMode::ClampToBorder, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER },
			{ ImageSamplerAddressMode::Unknown, VK_SAMPLER_ADDRESS_MODE_REPEAT }
		};
		const std::unordered_map<ImageSamplerBorderColor, VkBorderColor> m_borderColorMap{ { ImageSamplerBorderColor::FloatTransparentBlack, VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK },
			{ ImageSamplerBorderColor::IntTransparentBlack, VK_BORDER_COLOR_INT_TRANSPARENT_BLACK },
			{ ImageSamplerBorderColor::FloatOpaqueBlack, VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK },
			{ ImageSamplerBorderColor::IntOpaqueBlack, VK_BORDER_COLOR_INT_OPAQUE_BLACK },
			{ ImageSamplerBorderColor::FloatOpaqueWhite, VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE },
			{ ImageSamplerBorderColor::IntOpaqueWhite, VK_BORDER_COLOR_INT_OPAQUE_WHITE },
			{ ImageSamplerBorderColor::Unknown, VK_BORDER_COLOR_INT_OPAQUE_BLACK }
		};
	};

}