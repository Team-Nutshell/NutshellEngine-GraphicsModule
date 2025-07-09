#pragma once
#include "../Common/modules/ntshengn_graphics_module_interface.h"
#include "../Common/resources/ntshengn_resources_graphics.h"
#include "../Common/utils/ntshengn_defines.h"
#include "../Common/utils/ntshengn_enums.h"
#include "../Common/utils/ntshengn_utils_math.h"
#include "../Module/utils/ntshengn_module_defines.h"
#include "../Common/utils/ntshengn_utils_id_pool.h"
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
#include <limits>
#include <unordered_map>
#include <set>
#include <queue>
#include <utility>

#define NTSHENGN_VK_CHECK(f) \
	do { \
		int64_t check = f; \
		if (check) { \
			NTSHENGN_MODULE_ERROR("Vulkan Error.\nError code: " + std::to_string(check) + "\nFile: " + std::filesystem::path(__FILE__).filename().string() + "\nFunction: " + #f + "\nLine: " + std::to_string(__LINE__)); \
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
		(lhs.indexOfRefraction == rhs.indexOfRefraction) &&
		(lhs.useTriplanarMapping == rhs.useTriplanarMapping) &&
		(lhs.scaleUV == rhs.scaleUV) &&
		(lhs.offsetUV == rhs.offsetUV);
}

struct PreviousCamera {
	NtshEngn::Transform transform;
	NtshEngn::Camera camera;
};

struct PreviousObject {
	NtshEngn::Transform transform;
	NtshEngn::MeshID meshID;
	uint32_t materialIndex;
};

struct PreviousLight {
	NtshEngn::Transform transform;
	NtshEngn::Light light;
};

struct HostVisibleBuffer {
	VkBuffer handle;
	void* address;
	VmaAllocation allocation;
};

enum class ShaderType {
	Vertex,
	TesselationControl,
	TesselationEvaluation,
	Geometry,
	Fragment,
	RayGeneration,
	RayIntersection,
	RayAnyHit,
	RayClosestHit,
	RayMiss,
	RayCallable
};

struct InternalMesh {
	uint32_t indexCount;
	uint32_t firstIndex;
	int32_t vertexOffset;

	VkDeviceAddress vertexDeviceAddress;
	VkDeviceAddress indexDeviceAddress;
	VkDeviceAddress blasDeviceAddress;
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
	NtshEngn::Math::vec2 scaleUV = NtshEngn::Math::vec2(1.0f, 1.0f);
	NtshEngn::Math::vec2 offsetUV = NtshEngn::Math::vec2(0.0f, 0.0f);
	uint32_t useTriplanarMapping = 0;
	float padding = 0.0f;
};

struct InternalFont {
	uint32_t type;

	VkImage image;
	VmaAllocation imageAllocation;
	VkImageView imageView;

	NtshEngn::ImageSamplerFilter filter;

	std::unordered_map<wchar_t, NtshEngn::FontGlyph> glyphs;
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
	uint32_t fontType = 0;

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

namespace NtshEngn {

	class GraphicsModule : public GraphicsModuleInterface {
	public:
		GraphicsModule() : GraphicsModuleInterface("NutshellEngine Vulkan Path Tracing Graphics Module") {}

		void init();
		void update(float dt);
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
		// Sets the current playing time of an animation played by an entity
		void setAnimationCurrentTime(Entity entity, float time);

		// Returns true if the entity is currently playing the animation with index animationIndex, else, returns false
		bool isAnimationPlaying(Entity entity, uint32_t animationIndex);

		// Emits particles described by particleEmitter
		void emitParticles(const ParticleEmitter& particleEmitter);
		// Destroys all particles
		void destroyParticles();

		// Draws a text on the UI with the font in the fontID parameter using the position on screen, scale and color
		void drawUIText(FontID fontID, const std::wstring& text, const Math::vec2& position, const Math::vec2& scale, const Math::vec4& color);
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

		// Vertex, index and acceleration structure buffers creation
		void createVertexIndexAndAccelerationStructureBuffers();

		// Top-Level Acceleration Structure creation
		void createTopLevelAccelerationStructure();

		// Color image creation
		void createColorImage();

		// Descriptor set layout creation
		void createDescriptorSetLayout();

		// Shader compilation
		std::vector<uint32_t> compileShader(const std::string& shaderCode, ShaderType type);

		// Ray tracing pipeline creation
		void createRayTracingPipeline();

		// Ray tracing shader binding table creation
		void createRayTracingShaderBindingTable();

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
		void createUIImageResources();
		void updateUIImageDescriptorSet(uint32_t frameInFlight);

		// Default resources
		void createDefaultResources();

		// On window resize
		void resize();

		// Load Renderable for entity
		bool loadRenderableForEntity(Entity entity);

		// Create sampler
		std::string createSampler(const ImageSampler& sampler);

		// Add to textures
		uint32_t addToTextures(const InternalTexture& texture);

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

		VkImage m_colorImage;
		VmaAllocation m_colorImageAllocation;
		VkImageView m_colorImageView;

		VmaAllocator m_allocator;

		VkBuffer m_vertexBuffer;
		VmaAllocation m_vertexBufferAllocation;
		VkDeviceAddress m_vertexBufferDeviceAddress;
		VkBuffer m_indexBuffer;
		VmaAllocation m_indexBufferAllocation;
		VkDeviceAddress m_indexBufferDeviceAddress;

		VkBuffer m_topLevelAccelerationStructureBuffer;
		VmaAllocation m_topLevelAccelerationStructureBufferAllocation;
		VkDeviceSize m_topLevelAccelerationStructureBufferSize;
		VkBuffer m_topLevelAccelerationStructureScratchBuffer;
		VmaAllocation m_topLevelAccelerationStructureScratchBufferAllocation;
		VkDeviceAddress m_topLevelAccelerationStructureScratchBufferDeviceAddress;
		VkBuffer m_topLevelAccelerationStructureInstancesBuffer;
		VmaAllocation m_topLevelAccelerationStructureInstancesBufferAllocation;
		VkDeviceAddress m_topLevelAccelerationStructureInstancesBufferDeviceAddress;
		std::vector<HostVisibleBuffer> m_topLevelAccelerationStructureInstancesStagingBuffers;

		VkBuffer m_bottomLevelAccelerationStructureBuffer;
		VmaAllocation m_bottomLevelAccelerationStructureBufferAllocation;

		bool m_glslangInitialized = false;

		VkPipeline m_rayTracingPipeline;
		VkPipelineLayout m_rayTracingPipelineLayout;

		uint32_t m_rayTracingPipelineShaderGroupHandleSize;
		uint32_t m_rayTracingPipelineShaderGroupHandleAlignment;
		uint32_t m_rayTracingPipelineShaderGroupBaseAlignment;

		VkStridedDeviceAddressRegionKHR m_rayGenRegion;
		VkStridedDeviceAddressRegionKHR m_rayMissRegion;
		VkStridedDeviceAddressRegionKHR m_rayHitRegion;
		VkStridedDeviceAddressRegionKHR m_rayCallRegion;
		VkBuffer m_rayTracingShaderBindingTableBuffer;
		VmaAllocation m_rayTracingShaderBindingTableBufferAllocation;
		VkDeviceAddress m_rayTracingShaderBindingTableBufferDeviceAddress;

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
		PFN_vkGetBufferDeviceAddressKHR m_vkGetBufferDeviceAddressKHR;
		PFN_vkGetAccelerationStructureBuildSizesKHR m_vkGetAccelerationStructureBuildSizesKHR;
		PFN_vkCreateAccelerationStructureKHR m_vkCreateAccelerationStructureKHR;
		PFN_vkDestroyAccelerationStructureKHR m_vkDestroyAccelerationStructureKHR;
		PFN_vkCmdBuildAccelerationStructuresKHR m_vkCmdBuildAccelerationStructuresKHR;
		PFN_vkGetAccelerationStructureDeviceAddressKHR m_vkGetAccelerationStructureDeviceAddressKHR;
		PFN_vkCreateRayTracingPipelinesKHR m_vkCreateRayTracingPipelinesKHR;
		PFN_vkGetRayTracingShaderGroupHandlesKHR m_vkGetRayTracingShaderGroupHandlesKHR;
		PFN_vkCmdTraceRaysKHR m_vkCmdTraceRaysKHR;

		uint32_t m_imageCount;
		uint32_t m_framesInFlight;
		uint32_t m_currentFrameInFlight;

		std::vector<HostVisibleBuffer> m_cameraBuffers;

		std::vector<HostVisibleBuffer> m_objectBuffers;

		std::vector<HostVisibleBuffer> m_meshBuffers;

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
		VkDeviceSize m_currentBottomLevelAccelerationStructureOffset = 0;
		VkAccelerationStructureKHR m_topLevelAccelerationStructure;
		std::vector<VkAccelerationStructureKHR> m_bottomLevelAccelerationStructures;
		std::unordered_map<const Mesh*, MeshID> m_meshAddresses;

		std::vector<VkImage> m_textureImages;
		std::vector<VmaAllocation> m_textureImageAllocations;
		std::vector<VkImageView> m_textureImageViews;
		std::vector<Math::vec2> m_textureSizes;
		std::unordered_map<std::string, VkSampler> m_textureSamplers;
		std::unordered_map<const Image*, ImageID> m_imageAddresses;
		std::vector<InternalTexture> m_textures;

		std::vector<InternalMaterial> m_materials;
		IDPool m_materialsIDPool;

		std::unordered_map<Entity, InternalObject> m_objects;
		IDPool m_objectsIDPool;
		std::unordered_map<Entity, Material> m_lastKnownMaterial;

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

		uint32_t m_sampleBatch = 0;

		PreviousCamera m_previousCamera;
		std::unordered_map<Entity, PreviousObject> m_previousObjects;
		std::unordered_map<Entity, PreviousLight> m_previousDirectionalLights;
		std::unordered_map<Entity, PreviousLight> m_previousPointLights;
		std::unordered_map<Entity, PreviousLight> m_previousSpotLights;
		std::unordered_map<Entity, PreviousLight> m_previousAmbientLights;

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