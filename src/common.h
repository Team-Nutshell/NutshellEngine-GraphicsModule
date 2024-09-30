#pragma once
#include "../Common/resources/ntshengn_resources_graphics.h"
#include "../Common/utils/ntshengn_defines.h"
#include "../Common/utils/ntshengn_enums.h"
#include "../Common/ecs/ntshengn_ecs_interface.h"
#include "../Common/utils/ntshengn_utils_math.h"
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
#include "../external/glslang/glslang/Include/ShHandle.h"
#include "../external/glslang/SPIRV/GlslangToSpv.h"
#include "../external/glslang/StandAlone/DirStackFileIncluder.h"
#include <string>
#include <set>
#include <vector>
#include <unordered_map>

#define FRUSTUM_CULLING_DISABLED 0
#define FRUSTUM_CULLING_CPU_SINGLETHREADED 1
#define FRUSTUM_CULLING_CPU_MULTITHREADED 2
#define FRUSTUM_CULLING_GPU 3
#define FRUSTUM_CULLING_TYPE FRUSTUM_CULLING_GPU

#define SHADOW_MAPPING_RESOLUTION 2048
#define SHADOW_MAPPING_CASCADE_COUNT 3
#define SHADOW_MAPPING_CASCADE_SPLIT_LAMBDA 0.7f

#define SSAO_SAMPLE_COUNT 64

#define BLOOM_ENABLE 1
#define BLOOM_DOWNSCALE 4

#define FXAA_ENABLE 1

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

	NtshEngn::Math::vec3 aabbMin = { 0.0f, 0.0f, 0.0f };
	NtshEngn::Math::vec3 aabbMax = { 0.0f, 0.0f, 0.0f };
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

struct VulkanImage {
	VkImage handle = VK_NULL_HANDLE;
	VmaAllocation allocation;
	VkImageView view = VK_NULL_HANDLE;
	std::vector<VkImageView> layerMipViews;

	void destroy(VkDevice device, VmaAllocator allocator) {
		for (size_t i = 0; i < layerMipViews.size(); i++) {
			vkDestroyImageView(device, layerMipViews[i], nullptr);
		}
		if (view != VK_NULL_HANDLE) {
			vkDestroyImageView(device, view, nullptr);
		}
		if (handle != VK_NULL_HANDLE) {
			vmaDestroyImage(allocator, handle, allocation);
		}
	}
};

struct VulkanBuffer {
	VkBuffer handle;
	VmaAllocation allocation;

	void destroy(VmaAllocator allocator) {
		vmaDestroyBuffer(allocator, handle, allocation);
	}
};

struct HostVisibleVulkanBuffer : public VulkanBuffer {
	void* address;
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

inline std::vector<uint32_t> compileShader(const std::string& shaderCode, ShaderType type) {
	std::vector<uint32_t> spvCode;

	const char* shaderCodeCharPtr = shaderCode.c_str();

	EShLanguage shaderType = EShLangVertex;
	switch (type) {
	case ShaderType::Vertex:
		shaderType = EShLangVertex;
		break;

	case ShaderType::TesselationControl:
		shaderType = EShLangTessControl;
		break;

	case ShaderType::TesselationEvaluation:
		shaderType = EShLangTessEvaluation;
		break;

	case ShaderType::Geometry:
		shaderType = EShLangGeometry;
		break;

	case ShaderType::Fragment:
		shaderType = EShLangFragment;
		break;

	case ShaderType::Compute:
		shaderType = EShLangCompute;
		break;
	}

	glslang::TShader shader(shaderType);
	shader.setStrings(&shaderCodeCharPtr, 1);
	int clientInputSemanticsVersion = 110;
	glslang::EshTargetClientVersion vulkanClientVersion = glslang::EShTargetVulkan_1_1;
	glslang::EShTargetLanguageVersion spvLanguageVersion = glslang::EShTargetSpv_1_2;
	shader.setEnvInput(glslang::EShSourceGlsl, shaderType, glslang::EShClientVulkan, clientInputSemanticsVersion);
	shader.setEnvClient(glslang::EShClientVulkan, vulkanClientVersion);
	shader.setEnvTarget(glslang::EshTargetSpv, spvLanguageVersion);
	EShMessages messages = (EShMessages)(EShMsgSpvRules | EShMsgVulkanRules);
	int defaultVersion = 460;

	// Preprocess
	const TBuiltInResource defaultTBuiltInResource = {
		/* .MaxLights = */ 32,
		/* .MaxClipPlanes = */ 6,
		/* .MaxTextureUnits = */ 32,
		/* .MaxTextureCoords = */ 32,
		/* .MaxVertexAttribs = */ 64,
		/* .MaxVertexUniformComponents = */ 4096,
		/* .MaxVaryingFloats = */ 64,
		/* .MaxVertexTextureImageUnits = */ 32,
		/* .MaxCombinedTextureImageUnits = */ 80,
		/* .MaxTextureImageUnits = */ 32,
		/* .MaxFragmentUniformComponents = */ 4096,
		/* .MaxDrawBuffers = */ 32,
		/* .MaxVertexUniformVectors = */ 128,
		/* .MaxVaryingVectors = */ 8,
		/* .MaxFragmentUniformVectors = */ 16,
		/* .MaxVertexOutputVectors = */ 16,
		/* .MaxFragmentInputVectors = */ 15,
		/* .MinProgramTexelOffset = */ -8,
		/* .MaxProgramTexelOffset = */ 7,
		/* .MaxClipDistances = */ 8,
		/* .MaxComputeWorkGroupCountX = */ 65535,
		/* .MaxComputeWorkGroupCountY = */ 65535,
		/* .MaxComputeWorkGroupCountZ = */ 65535,
		/* .MaxComputeWorkGroupSizeX = */ 1024,
		/* .MaxComputeWorkGroupSizeY = */ 1024,
		/* .MaxComputeWorkGroupSizeZ = */ 64,
		/* .MaxComputeUniformComponents = */ 1024,
		/* .MaxComputeTextureImageUnits = */ 16,
		/* .MaxComputeImageUniforms = */ 8,
		/* .MaxComputeAtomicCounters = */ 8,
		/* .MaxComputeAtomicCounterBuffers = */ 1,
		/* .MaxVaryingComponents = */ 60,
		/* .MaxVertexOutputComponents = */ 64,
		/* .MaxGeometryInputComponents = */ 64,
		/* .MaxGeometryOutputComponents = */ 128,
		/* .MaxFragmentInputComponents = */ 128,
		/* .MaxImageUnits = */ 8,
		/* .MaxCombinedImageUnitsAndFragmentOutputs = */ 8,
		/* .MaxCombinedShaderOutputResources = */ 8,
		/* .MaxImageSamples = */ 0,
		/* .MaxVertexImageUniforms = */ 0,
		/* .MaxTessControlImageUniforms = */ 0,
		/* .MaxTessEvaluationImageUniforms = */ 0,
		/* .MaxGeometryImageUniforms = */ 0,
		/* .MaxFragmentImageUniforms = */ 8,
		/* .MaxCombinedImageUniforms = */ 8,
		/* .MaxGeometryTextureImageUnits = */ 16,
		/* .MaxGeometryOutputVertices = */ 256,
		/* .MaxGeometryTotalOutputComponents = */ 1024,
		/* .MaxGeometryUniformComponents = */ 1024,
		/* .MaxGeometryVaryingComponents = */ 64,
		/* .MaxTessControlInputComponents = */ 128,
		/* .MaxTessControlOutputComponents = */ 128,
		/* .MaxTessControlTextureImageUnits = */ 16,
		/* .MaxTessControlUniformComponents = */ 1024,
		/* .MaxTessControlTotalOutputComponents = */ 4096,
		/* .MaxTessEvaluationInputComponents = */ 128,
		/* .MaxTessEvaluationOutputComponents = */ 128,
		/* .MaxTessEvaluationTextureImageUnits = */ 16,
		/* .MaxTessEvaluationUniformComponents = */ 1024,
		/* .MaxTessPatchComponents = */ 120,
		/* .MaxPatchVertices = */ 32,
		/* .MaxTessGenLevel = */ 64,
		/* .MaxViewports = */ 16,
		/* .MaxVertexAtomicCounters = */ 0,
		/* .MaxTessControlAtomicCounters = */ 0,
		/* .MaxTessEvaluationAtomicCounters = */ 0,
		/* .MaxGeometryAtomicCounters = */ 0,
		/* .MaxFragmentAtomicCounters = */ 8,
		/* .MaxCombinedAtomicCounters = */ 8,
		/* .MaxAtomicCounterBindings = */ 1,
		/* .MaxVertexAtomicCounterBuffers = */ 0,
		/* .MaxTessControlAtomicCounterBuffers = */ 0,
		/* .MaxTessEvaluationAtomicCounterBuffers = */ 0,
		/* .MaxGeometryAtomicCounterBuffers = */ 0,
		/* .MaxFragmentAtomicCounterBuffers = */ 1,
		/* .MaxCombinedAtomicCounterBuffers = */ 1,
		/* .MaxAtomicCounterBufferSize = */ 16384,
		/* .MaxTransformFeedbackBuffers = */ 4,
		/* .MaxTransformFeedbackInterleavedComponents = */ 64,
		/* .MaxCullDistances = */ 8,
		/* .MaxCombinedClipAndCullDistances = */ 8,
		/* .MaxSamples = */ 4,
		/* .maxMeshOutputVerticesNV = */ 256,
		/* .maxMeshOutputPrimitivesNV = */ 512,
		/* .maxMeshWorkGroupSizeX_NV = */ 32,
		/* .maxMeshWorkGroupSizeY_NV = */ 1,
		/* .maxMeshWorkGroupSizeZ_NV = */ 1,
		/* .maxTaskWorkGroupSizeX_NV = */ 32,
		/* .maxTaskWorkGroupSizeY_NV = */ 1,
		/* .maxTaskWorkGroupSizeZ_NV = */ 1,
		/* .maxMeshViewCountNV = */ 4,
		/* .maxMeshOutputVerticesEXT = */ 256,
		/* .maxMeshOutputPrimitivesEXT = */ 256,
		/* .maxMeshWorkGroupSizeX_EXT = */ 128,
		/* .maxMeshWorkGroupSizeY_EXT = */ 128,
		/* .maxMeshWorkGroupSizeZ_EXT = */ 128,
		/* .maxTaskWorkGroupSizeX_EXT = */ 128,
		/* .maxTaskWorkGroupSizeY_EXT = */ 128,
		/* .maxTaskWorkGroupSizeZ_EXT = */ 128,
		/* .maxMeshViewCountEXT = */ 4,
		/* .maxDualSourceDrawBuffersEXT = */ 1,

		/* .limits = */ {
			/* .nonInductiveForLoops = */ 1,
			/* .whileLoops = */ 1,
			/* .doWhileLoops = */ 1,
			/* .generalUniformIndexing = */ 1,
			/* .generalAttributeMatrixVectorIndexing = */ 1,
			/* .generalVaryingIndexing = */ 1,
			/* .generalSamplerIndexing = */ 1,
			/* .generalVariableIndexing = */ 1,
			/* .generalConstantMatrixVectorIndexing = */ 1,
		} };
	DirStackFileIncluder includer;
	std::string preprocess;
	if (!shader.preprocess(&defaultTBuiltInResource, defaultVersion, ENoProfile, false, false, messages, &preprocess, includer)) {
		NTSHENGN_MODULE_WARNING("Shader preprocessing failed.\n" + std::string(shader.getInfoLog()));
		return spvCode;
	}

	// Parse
	const char* preprocessCharPtr = preprocess.c_str();
	shader.setStrings(&preprocessCharPtr, 1);
	if (!shader.parse(&defaultTBuiltInResource, defaultVersion, false, messages)) {
		NTSHENGN_MODULE_WARNING("Shader parsing failed.\n" + std::string(shader.getInfoLog()));
		return spvCode;
	}

	// Link
	glslang::TProgram program;
	program.addShader(&shader);
	if (!program.link(messages)) {
		NTSHENGN_MODULE_WARNING("Shader linking failed.");
		return spvCode;
	}

	// Compile
	spv::SpvBuildLogger buildLogger;
	glslang::SpvOptions spvOptions;
	glslang::GlslangToSpv(*program.getIntermediate(shaderType), spvCode, &buildLogger, &spvOptions);

	return spvCode;
}