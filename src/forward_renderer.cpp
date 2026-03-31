#include "forward_renderer.h"

void ForwardRenderer::init(VkDevice device, VkQueue graphicsQueue, uint32_t graphicsQueueFamilyIndex, VkViewport viewport, VkRect2D scissor, uint32_t framesInFlight, const std::vector<HostVisibleVulkanBuffer>& cameraBuffers, const std::vector<HostVisibleVulkanBuffer>& lightBuffers, const std::vector<HostVisibleVulkanBuffer>& objectBuffers, VulkanBuffer meshBuffer, const std::vector<HostVisibleVulkanBuffer>& jointTransformBuffers, const std::vector<HostVisibleVulkanBuffer>& materialBuffers, const std::vector<HostVisibleVulkanBuffer>& shadowSceneBuffers, PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR, PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR, PFN_vkCmdPipelineBarrier2KHR vkCmdPipelineBarrier2KHR) {
	m_device = device;
	m_graphicsQueue = graphicsQueue;
	m_graphicsQueueFamilyIndex = graphicsQueueFamilyIndex;
	m_viewport = viewport;
	m_scissor = scissor;
	m_framesInFlight = framesInFlight;
	m_vkCmdBeginRenderingKHR = vkCmdBeginRenderingKHR;
	m_vkCmdEndRenderingKHR = vkCmdEndRenderingKHR;
	m_vkCmdPipelineBarrier2KHR = vkCmdPipelineBarrier2KHR;

	createDescriptorSetLayout();
	createDescriptorSets(cameraBuffers, objectBuffers, lightBuffers, meshBuffer, jointTransformBuffers, materialBuffers, shadowSceneBuffers);
}

void ForwardRenderer::destroy() {
	for (const std::pair<std::string, VkPipeline>& customGraphicsPipeline : m_customGraphicsPipelines) {
		vkDestroyPipeline(m_device, customGraphicsPipeline.second, nullptr);
	}
	if (m_customGraphicsPipelineLayout != VK_NULL_HANDLE) {
		vkDestroyPipelineLayout(m_device, m_customGraphicsPipelineLayout, nullptr);
	}
	if (m_customVertexShaderModule != VK_NULL_HANDLE) {
		vkDestroyShaderModule(m_device, m_customVertexShaderModule, nullptr);
	}

	vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
	vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
}

void ForwardRenderer::draw(float dt, VkCommandBuffer commandBuffer, uint32_t currentFrameInFlight, const std::vector<InternalObject>& objects, const std::vector<InternalMesh>& meshes, const NtshEngn::Math::vec3& cameraPosition, const VulkanImage& colorImage, const VulkanImage& depthImage) {
	m_time += dt;

	NtshEngn::Math::vec4 cameraPositionAndTime = NtshEngn::Math::vec4(cameraPosition, m_time);

	VkRenderingAttachmentInfo forwardRendererColorAttachmentInfo = {};
	forwardRendererColorAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	forwardRendererColorAttachmentInfo.pNext = nullptr;
	forwardRendererColorAttachmentInfo.imageView = colorImage.view;
	forwardRendererColorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	forwardRendererColorAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
	forwardRendererColorAttachmentInfo.resolveImageView = VK_NULL_HANDLE;
	forwardRendererColorAttachmentInfo.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	forwardRendererColorAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	forwardRendererColorAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	forwardRendererColorAttachmentInfo.clearValue.color = { 0.0f, 0.0f, 0.0f, 0.0f };

	VkRenderingAttachmentInfo forwardRendererDepthAttachmentInfo = {};
	forwardRendererDepthAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	forwardRendererDepthAttachmentInfo.pNext = nullptr;
	forwardRendererDepthAttachmentInfo.imageView = depthImage.view;
	forwardRendererDepthAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	forwardRendererDepthAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
	forwardRendererDepthAttachmentInfo.resolveImageView = VK_NULL_HANDLE;
	forwardRendererDepthAttachmentInfo.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	forwardRendererDepthAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	forwardRendererDepthAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	forwardRendererDepthAttachmentInfo.clearValue.depthStencil = { 0.0f, 0 };

	VkRenderingInfo forwardRendererRenderingInfo = {};
	forwardRendererRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	forwardRendererRenderingInfo.pNext = nullptr;
	forwardRendererRenderingInfo.flags = 0;
	forwardRendererRenderingInfo.renderArea = m_scissor;
	forwardRendererRenderingInfo.layerCount = 1;
	forwardRendererRenderingInfo.viewMask = 0;
	forwardRendererRenderingInfo.colorAttachmentCount = 1;
	forwardRendererRenderingInfo.pColorAttachments = &forwardRendererColorAttachmentInfo;
	forwardRendererRenderingInfo.pDepthAttachment = &forwardRendererDepthAttachmentInfo;
	forwardRendererRenderingInfo.pStencilAttachment = nullptr;
	m_vkCmdBeginRenderingKHR(commandBuffer, &forwardRendererRenderingInfo);

	vkCmdSetViewport(commandBuffer, 0, 1, &m_viewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &m_scissor);

	for (const InternalObject& object : objects) {
		if (object.graphicsPipelineKey.empty() || (m_customGraphicsPipelines.find(object.graphicsPipelineKey) == m_customGraphicsPipelines.end())) {
			continue;
		}

		// Bind graphics pipeline
		VkPipeline graphicsPipeline = m_customGraphicsPipelines[object.graphicsPipelineKey];
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

		// Bind descriptor set 0
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_customGraphicsPipelineLayout, 0, 1, &m_descriptorSets[currentFrameInFlight], 0, nullptr);

		// Object index as push constant
		vkCmdPushConstants(commandBuffer, m_customGraphicsPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t), &object.index);

		// Additional data for custom fragment shaders
		vkCmdPushConstants(commandBuffer, m_customGraphicsPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(NtshEngn::Math::vec4), sizeof(NtshEngn::Math::vec4), &cameraPositionAndTime);

		// Draw
		vkCmdDrawIndexed(commandBuffer, meshes[object.meshID].indexCount, 1, meshes[object.meshID].firstIndex, meshes[object.meshID].vertexOffset, 0);
	}

	m_vkCmdEndRenderingKHR(commandBuffer);

	// Compositing synchronization before forward renderer
	VkImageMemoryBarrier2 colorImageMemoryBarrier = {};
	colorImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	colorImageMemoryBarrier.pNext = nullptr;
	colorImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	colorImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	colorImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	colorImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	colorImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	colorImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	colorImageMemoryBarrier.image = colorImage.handle;
	colorImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	colorImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	colorImageMemoryBarrier.subresourceRange.levelCount = 1;
	colorImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	colorImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkImageMemoryBarrier2 depthImageMemoryBarrier = {};
	depthImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	depthImageMemoryBarrier.pNext = nullptr;
	depthImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
	depthImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	depthImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
	depthImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	depthImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depthImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depthImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	depthImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	depthImageMemoryBarrier.image = depthImage.handle;
	depthImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	depthImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	depthImageMemoryBarrier.subresourceRange.levelCount = 1;
	depthImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	depthImageMemoryBarrier.subresourceRange.layerCount = 1;

	std::array<VkImageMemoryBarrier2, 2> imageMemoryBarriers = { colorImageMemoryBarrier, depthImageMemoryBarrier };
	VkDependencyInfo dependencyInfo = {};
	dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	dependencyInfo.pNext = nullptr;
	dependencyInfo.dependencyFlags = 0;
	dependencyInfo.memoryBarrierCount = 0;
	dependencyInfo.pMemoryBarriers = nullptr;
	dependencyInfo.bufferMemoryBarrierCount = 0;
	dependencyInfo.pBufferMemoryBarriers = nullptr;
	dependencyInfo.imageMemoryBarrierCount = static_cast<uint32_t>(imageMemoryBarriers.size());
	dependencyInfo.pImageMemoryBarriers = imageMemoryBarriers.data();
	m_vkCmdPipelineBarrier2KHR(commandBuffer, &dependencyInfo);
}

void ForwardRenderer::onResize(uint32_t width, uint32_t height) {
	m_viewport.width = static_cast<float>(width);
	m_viewport.height = static_cast<float>(height);
	m_scissor.extent.width = width;
	m_scissor.extent.height = height;
}

void ForwardRenderer::descriptorSetNeedsUpdate(uint32_t frameInFlight) {
	m_descriptorSetsNeedUpdate[frameInFlight] = true;
}

void ForwardRenderer::updateDescriptorSets(uint32_t frameInFlight, const std::vector<InternalTexture>& textures, const std::vector<VkImageView>& textureImageViews, const std::unordered_map<std::string, VkSampler>& textureSamplers) {
	if (!m_descriptorSetsNeedUpdate[frameInFlight]) {
		return;
	}

	std::vector<VkDescriptorImageInfo> texturesDescriptorImageInfos(textures.size());
	for (size_t i = 0; i < textures.size(); i++) {
		texturesDescriptorImageInfos[i].sampler = textureSamplers.at(textures[i].samplerKey);
		texturesDescriptorImageInfos[i].imageView = textureImageViews[textures[i].imageID];
		texturesDescriptorImageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}

	VkWriteDescriptorSet texturesDescriptorWriteDescriptorSet = {};
	texturesDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	texturesDescriptorWriteDescriptorSet.pNext = nullptr;
	texturesDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[frameInFlight];
	texturesDescriptorWriteDescriptorSet.dstBinding = 6;
	texturesDescriptorWriteDescriptorSet.dstArrayElement = 0;
	texturesDescriptorWriteDescriptorSet.descriptorCount = static_cast<uint32_t>(texturesDescriptorImageInfos.size());
	texturesDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	texturesDescriptorWriteDescriptorSet.pImageInfo = texturesDescriptorImageInfos.data();
	texturesDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
	texturesDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

	vkUpdateDescriptorSets(m_device, 1, &texturesDescriptorWriteDescriptorSet, 0, nullptr);

	m_descriptorSetsNeedUpdate[frameInFlight] = false;
}

void ForwardRenderer::shadowDescriptorSetNeedsUpdate(uint32_t frameInFlight) {
	m_descriptorSetsShadowNeedUpdate[frameInFlight] = true;
}

void ForwardRenderer::updateShadowDescriptorSets(uint32_t frameInFlight, const std::vector<VulkanImage>& shadowMaps, VkSampler shadowMapSampler) {
	if (!m_descriptorSetsShadowNeedUpdate[frameInFlight]) {
		return;
	}

	std::vector<VkDescriptorImageInfo> shadowMapImageDescriptorImageInfos(shadowMaps.size());
	for (uint32_t i = 0; i < shadowMaps.size(); i++) {
		shadowMapImageDescriptorImageInfos[i].sampler = shadowMapSampler;
		shadowMapImageDescriptorImageInfos[i].imageView = shadowMaps[i].view;
		shadowMapImageDescriptorImageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}

	VkWriteDescriptorSet shadowMapImageDescriptorWriteDescriptorSet = {};
	shadowMapImageDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	shadowMapImageDescriptorWriteDescriptorSet.pNext = nullptr;
	shadowMapImageDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[frameInFlight];
	shadowMapImageDescriptorWriteDescriptorSet.dstBinding = 8;
	shadowMapImageDescriptorWriteDescriptorSet.dstArrayElement = 0;
	shadowMapImageDescriptorWriteDescriptorSet.descriptorCount = static_cast<uint32_t>(shadowMapImageDescriptorImageInfos.size());
	shadowMapImageDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	shadowMapImageDescriptorWriteDescriptorSet.pImageInfo = shadowMapImageDescriptorImageInfos.data();
	shadowMapImageDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
	shadowMapImageDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

	vkUpdateDescriptorSets(m_device, 1, &shadowMapImageDescriptorWriteDescriptorSet, 0, nullptr);
}

bool ForwardRenderer::createGraphicsPipelineFromFragmentShader(const std::string& fragmentShader) {
	if (fragmentShader.empty()) {
		return false;
	}

	if (m_customGraphicsPipelines.find(fragmentShader) != m_customGraphicsPipelines.end()) {
		return true;
	}

	VkFormat pipelineRenderingColorFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
	VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {};
	pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	pipelineRenderingCreateInfo.pNext = nullptr;
	pipelineRenderingCreateInfo.viewMask = 0;
	pipelineRenderingCreateInfo.colorAttachmentCount = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &pipelineRenderingColorFormat;
	pipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;
	pipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

	if (m_customVertexShaderModule == VK_NULL_HANDLE) {
		const std::string vertexShaderCode = R"GLSL(
		#version 460

		struct ObjectInfo {
			mat4 model;
			mat4 transposeInverseModel;
			uint meshID;
			uint jointTransformOffset;
			uint materialID;
		};

		struct MeshInfo {
			uint hasSkin;
		};

		layout(set = 0, binding = 0) uniform Camera {
			mat4 view;
			mat4 projection;
			vec3 position;
		} camera;

		layout(std430, set = 0, binding = 1) restrict readonly buffer Objects {
			ObjectInfo info[];
		} objects;

		layout(std430, set = 0, binding = 2) restrict readonly buffer Meshes {
			MeshInfo info[];
		} meshes;

		layout(set = 0, binding = 3) restrict readonly buffer JointTransforms {
			mat4 matrix[];
		} jointTransforms;

		layout(push_constant) uniform ObjectID {
			uint objectID;
		} oID;

		layout(location = 0) in vec3 position;
		layout(location = 1) in vec3 normal;
		layout(location = 2) in vec2 uv;
		layout(location = 3) in vec4 color;
		layout(location = 4) in vec4 tangent;
		layout(location = 5) in uvec4 joints;
		layout(location = 6) in vec4 weights;

		layout(location = 0) out vec3 outPosition;
		layout(location = 1) out vec2 outUV;
		layout(location = 2) out vec4 outColor;
		layout(location = 3) out flat uint outMaterialID;
		layout(location = 4) out mat3 outTBN;

		void main() {
			mat4 skinMatrix = mat4(1.0);
			if (meshes.info[objects.info[oID.objectID].meshID].hasSkin == 1) {
				uint jointTransformOffset = objects.info[oID.objectID].jointTransformOffset;

				skinMatrix = (weights.x * jointTransforms.matrix[jointTransformOffset + joints.x]) +
					(weights.y * jointTransforms.matrix[jointTransformOffset + joints.y]) +
					(weights.z * jointTransforms.matrix[jointTransformOffset + joints.z]) +
					(weights.w * jointTransforms.matrix[jointTransformOffset + joints.w]);
			}
			outPosition = vec3(objects.info[oID.objectID].model * skinMatrix * vec4(position, 1.0));
			outUV = uv;
			outColor = color;
			outMaterialID = objects.info[oID.objectID].materialID;

			vec3 skinnedNormal = vec3(transpose(inverse(skinMatrix)) * vec4(normal, 0.0));
			vec3 skinnedTangent = vec3(skinMatrix * vec4(tangent.xyz, 0.0));

			vec3 bitangent = cross(skinnedNormal, skinnedTangent) * tangent.w;
			vec3 T = vec3(objects.info[oID.objectID].transposeInverseModel * vec4(skinnedTangent, 0.0));
			vec3 B = vec3(objects.info[oID.objectID].transposeInverseModel * vec4(bitangent, 0.0));
			vec3 N = vec3(objects.info[oID.objectID].transposeInverseModel * vec4(skinnedNormal, 0.0));
			outTBN = mat3(T, B, N);

			gl_Position = camera.projection * camera.view * vec4(outPosition, 1.0);
		}
	)GLSL";
		const std::vector<uint32_t> vertexShaderSpv = compileShader(vertexShaderCode, ShaderType::Vertex);

		VkShaderModuleCreateInfo vertexShaderModuleCreateInfo = {};
		vertexShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		vertexShaderModuleCreateInfo.pNext = nullptr;
		vertexShaderModuleCreateInfo.flags = 0;
		vertexShaderModuleCreateInfo.codeSize = vertexShaderSpv.size() * sizeof(uint32_t);
		vertexShaderModuleCreateInfo.pCode = vertexShaderSpv.data();
		NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &vertexShaderModuleCreateInfo, nullptr, &m_customVertexShaderModule));
	}

	VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo = {};
	vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderStageCreateInfo.pNext = nullptr;
	vertexShaderStageCreateInfo.flags = 0;
	vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexShaderStageCreateInfo.module = m_customVertexShaderModule;
	vertexShaderStageCreateInfo.pName = "main";
	vertexShaderStageCreateInfo.pSpecializationInfo = nullptr;

	const std::string fragmentShaderPrefixCode = R"GLSL(
		#version 460
		#extension GL_EXT_nonuniform_qualifier : enable

		#define SHADOW_MAPPING_CASCADE_COUNT 3

		#define NtshEngn_position position
		#define NtshEngn_normal TBN[2]
		#define NtshEngn_tangent TBN[0]
		#define NtshEngn_bitangent TBN[1]
		#define NtshEngn_uv uv
		#define NtshEngn_color color
		#define NtshEngn_tbn TBN
		#define NtshEngn_diffuseTexture textures[nonuniformEXT(materials.info[materialID].diffuseTextureIndex)]
		#define NtshEngn_normalTexture textures[nonuniformEXT(materials.info[materialID].normalTextureIndex)]
		#define NtshEngn_metalnessTexture textures[nonuniformEXT(materials.info[materialID].metalnessTextureIndex)]
		#define NtshEngn_roughnessTexture textures[nonuniformEXT(materials.info[materialID].roughnessTextureIndex)]
		#define NtshEngn_occlusionTexture textures[nonuniformEXT(materials.info[materialID].occlusionTextureIndex)]
		#define NtshEngn_emissiveTexture textures[nonuniformEXT(materials.info[materialID].emissiveTextureIndex)]
		#define NtshEngn_emissiveFactor materials.info[materialID].emissiveFactor
		#define NtshEngn_alphaCutoff materials.info[materialID].alphaCutoff
		#define NtshEngn_scaleUV materials.info[materialID].scaleUV
		#define NtshEngn_offsetUV materials.info[materialID].offsetUV
		#define NtshEngn_useTriplanarMapping materials.info[materialID].useTriplanarMapping
		#define NtshEngn_directionalLightCount lights.count.x
		#define NtshEngn_directionalLight(i) lights.info[i]
		#define NtshEngn_directionalLightShadows(i, p) directionalLightShadows(i, p)
		#define NtshEngn_pointLightCount lights.count.y
		#define NtshEngn_pointLight(i) lights.info[lights.count.x + i]
		#define NtshEngn_pointLightShadows(i, p) pointLightShadows(i, p)
		#define NtshEngn_spotLightCount lights.count.z
		#define NtshEngn_spotLight(i) lights.info[lights.count.x + lights.count.y + i]
		#define NtshEngn_spotLightShadows(i, p) spotLightShadows(i, p)
		#define NtshEngn_ambientLightCount lights.count.w
		#define NtshEngn_ambientLight(i) lights.info[lights.count.x + lights.count.y + lights.count.z + i]
		#define NtshEngn_time pC.cameraPositionAndTime.w
		#define NtshEngn_cameraPosition pC.cameraPositionAndTime.xyz
		#define NtshEngn_useReversedDepth true
		#define NtshEngn_outColor outColor
		#define NtshEngn_outDepth gl_FragDepth

		const mat4 shadowOffset = mat4(
			0.5, 0.0, 0.0, 0.0,
			0.0, 0.5, 0.0, 0.0,
			0.0, 0.0, 1.0, 0.0,
			0.5, 0.5, 0.0, 1.0
		);

		struct MaterialInfo {
			uint diffuseTextureIndex;
			uint normalTextureIndex;
			uint metalnessTextureIndex;
			uint roughnessTextureIndex;
			uint occlusionTextureIndex;
			uint emissiveTextureIndex;
			float emissiveFactor;
			float alphaCutoff;
			vec2 scaleUV;
			vec2 offsetUV;
			uint useTriplanarMapping;
		};

		struct LightInfo {
			vec3 position;
			vec3 direction;
			vec3 color;
			float intensity;
			vec2 cutoff;
			float distance;
		};

		struct ShadowInfo {
			mat4 viewProj;
			float splitDepth;
		};

		layout(set = 0, binding = 0) uniform Camera {
			mat4 view;
			mat4 projection;
			vec3 position;
		} camera;

		layout(set = 0, binding = 4) restrict readonly buffer Materials {
			MaterialInfo info[];
		} materials;

		layout(set = 0, binding = 5) restrict readonly buffer Lights {
			uvec4 count;
			LightInfo info[];
		} lights;

		layout(set = 0, binding = 6) uniform sampler2D textures[];

		layout(set = 0, binding = 7) restrict readonly buffer Shadows {
			ShadowInfo info[];
		} shadows;

		layout(set = 0, binding = 8) uniform sampler2DArray shadowMaps[];
		layout(set = 0, binding = 8) uniform samplerCube shadowCubeMaps[];

		layout(push_constant) uniform PushConstants {
			layout(offset = 16) vec4 cameraPositionAndTime;
		} pC;

		layout(location = 0) in vec3 position;
		layout(location = 1) in vec2 uv;
		layout(location = 2) in vec4 color;
		layout(location = 3) in flat uint materialID;
		layout(location = 4) in mat3 TBN;

		layout(location = 0) out vec4 outColor;

		float shadowValue(uint lightIndex, uint layerIndex, vec4 shadowCoord, float bias) {
			float shadow = 0.0;
			if ((shadowCoord.z < -1.0) || (shadowCoord.z > 1.0)) {
				return 1.0;
			}

			const vec2 texelSize = 0.75 * (1.0 / vec2(textureSize(shadowMaps[nonuniformEXT(lightIndex)], 0).xy));
			for (int x = -1; x <= 1; x++) {
				for (int y = -1; y <= 1; y++) {
					const float depth = texture(shadowMaps[nonuniformEXT(lightIndex)], vec3(shadowCoord.xy + (vec2(x, y) * texelSize), layerIndex)).r;
					if (depth >= (shadowCoord.z - bias)) {
						shadow += 1.0;
					}
				}
			}

			return shadow / 9.0;
		}

		float shadowCubeValue(uint lightIndex, vec3 direction, float bias) {
			const float lengthDirection = length(direction);
			const float depth = texture(shadowCubeMaps[nonuniformEXT(lightIndex)], direction).r;
			if ((depth * 50.0) >= (lengthDirection - bias)) {
				return 1.0;
			}

			return 0.0;
		}

		float directionalLightShadows(uint lightIndex, vec3 position) {
			const uint shadowIndex = lightIndex * SHADOW_MAPPING_CASCADE_COUNT;
			const vec3 viewPosition = vec3(camera.view * vec4(position, 1.0));

			uint cascadeIndex = 0;
			for (uint i = 0; i < SHADOW_MAPPING_CASCADE_COUNT - 1; i++) {
				if (viewPosition.z < shadows.info[shadowIndex + i].splitDepth) {
					cascadeIndex = i + 1;
				}
			}

			const vec4 shadowCoord = (shadowOffset * shadows.info[shadowIndex + cascadeIndex].viewProj) * vec4(position, 1.0);

			return shadowValue(lightIndex, cascadeIndex, shadowCoord / shadowCoord.w, 0.00005);
		}

		float pointLightShadows(uint lightIndex, vec3 position) {
			uint globalLightIndex = lights.count.x + lightIndex;

			const vec3 lightDirection = position - lights.info[globalLightIndex].position;

			return shadowCubeValue(globalLightIndex, lightDirection, 0.05);
		}

		float spotLightShadows(uint lightIndex, vec3 position) {
			const uint globalLightIndex = lights.count.x + lights.count.y + lightIndex;
			const uint shadowIndex = (lights.count.x * SHADOW_MAPPING_CASCADE_COUNT) + (lights.count.y * 6) + lightIndex;

			const vec4 shadowCoord = (shadowOffset * shadows.info[shadowIndex].viewProj) * vec4(position, 1.0);

			return shadowValue(globalLightIndex, 0, shadowCoord / shadowCoord.w, 0.00005);
		}

	)GLSL";
	const std::vector<uint32_t> fragmentShaderSpv = compileShader(fragmentShaderPrefixCode + fragmentShader, ShaderType::Fragment);
	if (fragmentShaderSpv.empty()) {
		return false;
	}

	VkShaderModule fragmentShaderModule;
	VkShaderModuleCreateInfo fragmentShaderModuleCreateInfo = {};
	fragmentShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	fragmentShaderModuleCreateInfo.pNext = nullptr;
	fragmentShaderModuleCreateInfo.flags = 0;
	fragmentShaderModuleCreateInfo.codeSize = fragmentShaderSpv.size() * sizeof(uint32_t);
	fragmentShaderModuleCreateInfo.pCode = fragmentShaderSpv.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &fragmentShaderModuleCreateInfo, nullptr, &fragmentShaderModule));

	VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo = {};
	fragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderStageCreateInfo.pNext = nullptr;
	fragmentShaderStageCreateInfo.flags = 0;
	fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentShaderStageCreateInfo.module = fragmentShaderModule;
	fragmentShaderStageCreateInfo.pName = "main";
	fragmentShaderStageCreateInfo.pSpecializationInfo = nullptr;

	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStageCreateInfos = { vertexShaderStageCreateInfo, fragmentShaderStageCreateInfo };

	VkVertexInputBindingDescription vertexInputBindingDescription = {};
	vertexInputBindingDescription.binding = 0;
	vertexInputBindingDescription.stride = sizeof(NtshEngn::Vertex);
	vertexInputBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription vertexPositionInputAttributeDescription = {};
	vertexPositionInputAttributeDescription.location = 0;
	vertexPositionInputAttributeDescription.binding = 0;
	vertexPositionInputAttributeDescription.format = VK_FORMAT_R32G32B32_SFLOAT;
	vertexPositionInputAttributeDescription.offset = 0;

	VkVertexInputAttributeDescription vertexNormalInputAttributeDescription = {};
	vertexNormalInputAttributeDescription.location = 1;
	vertexNormalInputAttributeDescription.binding = 0;
	vertexNormalInputAttributeDescription.format = VK_FORMAT_R32G32B32_SFLOAT;
	vertexNormalInputAttributeDescription.offset = offsetof(NtshEngn::Vertex, normal);

	VkVertexInputAttributeDescription vertexUVInputAttributeDescription = {};
	vertexUVInputAttributeDescription.location = 2;
	vertexUVInputAttributeDescription.binding = 0;
	vertexUVInputAttributeDescription.format = VK_FORMAT_R32G32_SFLOAT;
	vertexUVInputAttributeDescription.offset = offsetof(NtshEngn::Vertex, uv);

	VkVertexInputAttributeDescription vertexColorInputAttributeDescription = {};
	vertexColorInputAttributeDescription.location = 3;
	vertexColorInputAttributeDescription.binding = 0;
	vertexColorInputAttributeDescription.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	vertexColorInputAttributeDescription.offset = offsetof(NtshEngn::Vertex, color);

	VkVertexInputAttributeDescription vertexTangentInputAttributeDescription = {};
	vertexTangentInputAttributeDescription.location = 4;
	vertexTangentInputAttributeDescription.binding = 0;
	vertexTangentInputAttributeDescription.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	vertexTangentInputAttributeDescription.offset = offsetof(NtshEngn::Vertex, tangent);

	VkVertexInputAttributeDescription vertexJointsInputAttributeDescription = {};
	vertexJointsInputAttributeDescription.location = 5;
	vertexJointsInputAttributeDescription.binding = 0;
	vertexJointsInputAttributeDescription.format = VK_FORMAT_R32G32B32A32_UINT;
	vertexJointsInputAttributeDescription.offset = offsetof(NtshEngn::Vertex, joints);

	VkVertexInputAttributeDescription vertexWeightsInputAttributeDescription = {};
	vertexWeightsInputAttributeDescription.location = 6;
	vertexWeightsInputAttributeDescription.binding = 0;
	vertexWeightsInputAttributeDescription.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	vertexWeightsInputAttributeDescription.offset = offsetof(NtshEngn::Vertex, weights);

	std::array<VkVertexInputAttributeDescription, 7> vertexInputAttributeDescriptions = { vertexPositionInputAttributeDescription, vertexNormalInputAttributeDescription, vertexUVInputAttributeDescription, vertexColorInputAttributeDescription, vertexTangentInputAttributeDescription, vertexJointsInputAttributeDescription, vertexWeightsInputAttributeDescription };
	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
	vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateCreateInfo.pNext = nullptr;
	vertexInputStateCreateInfo.flags = 0;
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = 1;
	vertexInputStateCreateInfo.pVertexBindingDescriptions = &vertexInputBindingDescription;
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertexInputAttributeDescriptions.size());
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = vertexInputAttributeDescriptions.data();

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {};
	inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyStateCreateInfo.pNext = nullptr;
	inputAssemblyStateCreateInfo.flags = 0;
	inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.pNext = nullptr;
	viewportStateCreateInfo.flags = 0;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &m_viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &m_scissor;

	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {};
	rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateCreateInfo.pNext = nullptr;
	rasterizationStateCreateInfo.flags = 0;
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
	rasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
	rasterizationStateCreateInfo.depthBiasClamp = 0.0f;
	rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;
	rasterizationStateCreateInfo.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {};
	multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleStateCreateInfo.pNext = nullptr;
	multisampleStateCreateInfo.flags = 0;
	multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
	multisampleStateCreateInfo.minSampleShading = 0.0f;
	multisampleStateCreateInfo.pSampleMask = nullptr;
	multisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
	multisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;

	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {};
	depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilStateCreateInfo.pNext = nullptr;
	depthStencilStateCreateInfo.flags = 0;
	depthStencilStateCreateInfo.depthTestEnable = VK_TRUE;
	depthStencilStateCreateInfo.depthWriteEnable = VK_TRUE;
	depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_GREATER;
	depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.front = {};
	depthStencilStateCreateInfo.back = {};
	depthStencilStateCreateInfo.minDepthBounds = 0.0f;
	depthStencilStateCreateInfo.maxDepthBounds = 1.0f;

	VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
	colorBlendAttachmentState.blendEnable = VK_FALSE;
	colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {};
	colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendStateCreateInfo.pNext = nullptr;
	colorBlendStateCreateInfo.flags = 0;
	colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
	colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	colorBlendStateCreateInfo.attachmentCount = 1;
	colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;

	std::array<VkDynamicState, 2> dynamicStates = { VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_VIEWPORT };
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.pNext = nullptr;
	dynamicStateCreateInfo.flags = 0;
	dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

	if (m_customGraphicsPipelineLayout == VK_NULL_HANDLE) {
		VkPushConstantRange vertexPushConstantRange = {};
		vertexPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		vertexPushConstantRange.offset = 0;
		vertexPushConstantRange.size = sizeof(uint32_t);

		VkPushConstantRange fragmentPushConstantRange = {};
		fragmentPushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		fragmentPushConstantRange.offset = sizeof(NtshEngn::Math::vec4);
		fragmentPushConstantRange.size = sizeof(NtshEngn::Math::vec4) + sizeof(float);

		std::array<VkPushConstantRange, 2> pushConstantRanges = { vertexPushConstantRange, fragmentPushConstantRange };
		VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
		pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		pipelineLayoutCreateInfo.pNext = nullptr;
		pipelineLayoutCreateInfo.flags = 0;
		pipelineLayoutCreateInfo.setLayoutCount = 1;
		pipelineLayoutCreateInfo.pSetLayouts = &m_descriptorSetLayout;
		pipelineLayoutCreateInfo.pushConstantRangeCount = static_cast<uint32_t>(pushConstantRanges.size());
		pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges.data();
		NTSHENGN_VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_customGraphicsPipelineLayout));
	}

	VkPipeline customGraphicsPipeline;
	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
	graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPipelineCreateInfo.pNext = &pipelineRenderingCreateInfo;
	graphicsPipelineCreateInfo.flags = 0;
	graphicsPipelineCreateInfo.stageCount = 2;
	graphicsPipelineCreateInfo.pStages = shaderStageCreateInfos.data();
	graphicsPipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
	graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
	graphicsPipelineCreateInfo.pTessellationState = nullptr;
	graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	graphicsPipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
	graphicsPipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
	graphicsPipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	graphicsPipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	graphicsPipelineCreateInfo.layout = m_customGraphicsPipelineLayout;
	graphicsPipelineCreateInfo.renderPass = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.subpass = 0;
	graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.basePipelineIndex = 0;
	NTSHENGN_VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &customGraphicsPipeline));

	vkDestroyShaderModule(m_device, fragmentShaderModule, nullptr);

	m_customGraphicsPipelines[fragmentShader] = customGraphicsPipeline;

	return true;
}

void ForwardRenderer::createDescriptorSetLayout() {
	VkDescriptorSetLayoutBinding cameraDescriptorSetLayoutBinding = {};
	cameraDescriptorSetLayoutBinding.binding = 0;
	cameraDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	cameraDescriptorSetLayoutBinding.descriptorCount = 1;
	cameraDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
	cameraDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding objectsDescriptorSetLayoutBinding = {};
	objectsDescriptorSetLayoutBinding.binding = 1;
	objectsDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	objectsDescriptorSetLayoutBinding.descriptorCount = 1;
	objectsDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	objectsDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding meshesDescriptorSetLayoutBinding = {};
	meshesDescriptorSetLayoutBinding.binding = 2;
	meshesDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	meshesDescriptorSetLayoutBinding.descriptorCount = 1;
	meshesDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	meshesDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding jointTransformsDescriptorSetLayoutBinding = {};
	jointTransformsDescriptorSetLayoutBinding.binding = 3;
	jointTransformsDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	jointTransformsDescriptorSetLayoutBinding.descriptorCount = 1;
	jointTransformsDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	jointTransformsDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding materialsDescriptorSetLayoutBinding = {};
	materialsDescriptorSetLayoutBinding.binding = 4;
	materialsDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	materialsDescriptorSetLayoutBinding.descriptorCount = 1;
	materialsDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	materialsDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding lightsDescriptorSetLayoutBinding = {};
	lightsDescriptorSetLayoutBinding.binding = 5;
	lightsDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	lightsDescriptorSetLayoutBinding.descriptorCount = 1;
	lightsDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	lightsDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding texturesDescriptorSetLayoutBinding = {};
	texturesDescriptorSetLayoutBinding.binding = 6;
	texturesDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	texturesDescriptorSetLayoutBinding.descriptorCount = 131072;
	texturesDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	texturesDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding shadowSceneDescriptorSetLayoutBinding = {};
	shadowSceneDescriptorSetLayoutBinding.binding = 7;
	shadowSceneDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	shadowSceneDescriptorSetLayoutBinding.descriptorCount = 1;
	shadowSceneDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	shadowSceneDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding shadowMapsDescriptorSetLayoutBinding = {};
	shadowMapsDescriptorSetLayoutBinding.binding = 8;
	shadowMapsDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	shadowMapsDescriptorSetLayoutBinding.descriptorCount = 131072;
	shadowMapsDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	shadowMapsDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	std::array<VkDescriptorBindingFlags, 9> descriptorBindingFlags = { 0, 0, 0, 0, 0, 0, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT, 0, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT };
	VkDescriptorSetLayoutBindingFlagsCreateInfo descriptorSetLayoutBindingFlagsCreateInfo = {};
	descriptorSetLayoutBindingFlagsCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	descriptorSetLayoutBindingFlagsCreateInfo.pNext = nullptr;
	descriptorSetLayoutBindingFlagsCreateInfo.bindingCount = static_cast<uint32_t>(descriptorBindingFlags.size());
	descriptorSetLayoutBindingFlagsCreateInfo.pBindingFlags = descriptorBindingFlags.data();

	std::array<VkDescriptorSetLayoutBinding, 9> descriptorSetLayoutBindings = { cameraDescriptorSetLayoutBinding, objectsDescriptorSetLayoutBinding, meshesDescriptorSetLayoutBinding, jointTransformsDescriptorSetLayoutBinding, materialsDescriptorSetLayoutBinding, lightsDescriptorSetLayoutBinding, texturesDescriptorSetLayoutBinding, shadowSceneDescriptorSetLayoutBinding, shadowMapsDescriptorSetLayoutBinding };
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.pNext = &descriptorSetLayoutBindingFlagsCreateInfo;
	descriptorSetLayoutCreateInfo.flags = 0;
	descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(descriptorSetLayoutBindings.size());
	descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings.data();
	NTSHENGN_VK_CHECK(vkCreateDescriptorSetLayout(m_device, &descriptorSetLayoutCreateInfo, nullptr, &m_descriptorSetLayout));
}

void ForwardRenderer::createDescriptorSets(const std::vector<HostVisibleVulkanBuffer>& cameraBuffers, const std::vector<HostVisibleVulkanBuffer>& objectBuffers, const std::vector<HostVisibleVulkanBuffer>& lightBuffers, VulkanBuffer meshBuffer, const std::vector<HostVisibleVulkanBuffer>& jointTransformBuffers, const std::vector<HostVisibleVulkanBuffer>& materialBuffers, const std::vector<HostVisibleVulkanBuffer>& shadowSceneBuffers) {
	// Create descriptor pool
	VkDescriptorPoolSize cameraDescriptorPoolSize = {};
	cameraDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	cameraDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize objectsDescriptorPoolSize = {};
	objectsDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	objectsDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize meshesDescriptorPoolSize = {};
	meshesDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	meshesDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize jointTransformsDescriptorPoolSize = {};
	jointTransformsDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	jointTransformsDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize materialsDescriptorPoolSize = {};
	materialsDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	materialsDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize lightsDescriptorPoolSize = {};
	lightsDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	lightsDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize texturesDescriptorPoolSize = {};
	texturesDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	texturesDescriptorPoolSize.descriptorCount = 131072 * m_framesInFlight;

	VkDescriptorPoolSize shadowSceneDescriptorPoolSize = {};
	shadowSceneDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	shadowSceneDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize shadowMapsDescriptorPoolSize = {};
	shadowMapsDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	shadowMapsDescriptorPoolSize.descriptorCount = 131072 * m_framesInFlight;

	std::array<VkDescriptorPoolSize, 9> descriptorPoolSizes = { cameraDescriptorPoolSize, objectsDescriptorPoolSize, meshesDescriptorPoolSize, jointTransformsDescriptorPoolSize, materialsDescriptorPoolSize, lightsDescriptorPoolSize, texturesDescriptorPoolSize, shadowSceneDescriptorPoolSize, shadowMapsDescriptorPoolSize };
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext = nullptr;
	descriptorPoolCreateInfo.flags = 0;
	descriptorPoolCreateInfo.maxSets = m_framesInFlight;
	descriptorPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size());
	descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes.data();
	NTSHENGN_VK_CHECK(vkCreateDescriptorPool(m_device, &descriptorPoolCreateInfo, nullptr, &m_descriptorPool));

	// Allocate descriptor sets
	m_descriptorSets.resize(m_framesInFlight);
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
		descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocateInfo.pNext = nullptr;
		descriptorSetAllocateInfo.descriptorPool = m_descriptorPool;
		descriptorSetAllocateInfo.descriptorSetCount = 1;
		descriptorSetAllocateInfo.pSetLayouts = &m_descriptorSetLayout;
		NTSHENGN_VK_CHECK(vkAllocateDescriptorSets(m_device, &descriptorSetAllocateInfo, &m_descriptorSets[i]));
	}

	// Update descriptor sets
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		std::vector<VkWriteDescriptorSet> writeDescriptorSets;

		VkDescriptorBufferInfo cameraDescriptorBufferInfo;
		cameraDescriptorBufferInfo.buffer = cameraBuffers[i].handle;
		cameraDescriptorBufferInfo.offset = 0;
		cameraDescriptorBufferInfo.range = sizeof(NtshEngn::Math::mat4) * 2 + sizeof(NtshEngn::Math::vec4);

		VkWriteDescriptorSet cameraDescriptorWriteDescriptorSet = {};
		cameraDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		cameraDescriptorWriteDescriptorSet.pNext = nullptr;
		cameraDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		cameraDescriptorWriteDescriptorSet.dstBinding = 0;
		cameraDescriptorWriteDescriptorSet.dstArrayElement = 0;
		cameraDescriptorWriteDescriptorSet.descriptorCount = 1;
		cameraDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		cameraDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		cameraDescriptorWriteDescriptorSet.pBufferInfo = &cameraDescriptorBufferInfo;
		cameraDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;
		writeDescriptorSets.push_back(cameraDescriptorWriteDescriptorSet);

		VkDescriptorBufferInfo objectsDescriptorBufferInfo;
		objectsDescriptorBufferInfo.buffer = objectBuffers[i].handle;
		objectsDescriptorBufferInfo.offset = 0;
		objectsDescriptorBufferInfo.range = 32768;

		VkWriteDescriptorSet objectsDescriptorWriteDescriptorSet = {};
		objectsDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		objectsDescriptorWriteDescriptorSet.pNext = nullptr;
		objectsDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		objectsDescriptorWriteDescriptorSet.dstBinding = 1;
		objectsDescriptorWriteDescriptorSet.dstArrayElement = 0;
		objectsDescriptorWriteDescriptorSet.descriptorCount = 1;
		objectsDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		objectsDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		objectsDescriptorWriteDescriptorSet.pBufferInfo = &objectsDescriptorBufferInfo;
		objectsDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;
		writeDescriptorSets.push_back(objectsDescriptorWriteDescriptorSet);

		VkDescriptorBufferInfo meshesDescriptorBufferInfo;
		meshesDescriptorBufferInfo.buffer = meshBuffer.handle;
		meshesDescriptorBufferInfo.offset = 0;
		meshesDescriptorBufferInfo.range = 32768;

		VkWriteDescriptorSet meshesDescriptorWriteDescriptorSet = {};
		meshesDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		meshesDescriptorWriteDescriptorSet.pNext = nullptr;
		meshesDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		meshesDescriptorWriteDescriptorSet.dstBinding = 2;
		meshesDescriptorWriteDescriptorSet.dstArrayElement = 0;
		meshesDescriptorWriteDescriptorSet.descriptorCount = 1;
		meshesDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		meshesDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		meshesDescriptorWriteDescriptorSet.pBufferInfo = &meshesDescriptorBufferInfo;
		meshesDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;
		writeDescriptorSets.push_back(meshesDescriptorWriteDescriptorSet);

		VkDescriptorBufferInfo jointTransformsDescriptorBufferInfo;
		jointTransformsDescriptorBufferInfo.buffer = jointTransformBuffers[i].handle;
		jointTransformsDescriptorBufferInfo.offset = 0;
		jointTransformsDescriptorBufferInfo.range = 4096 * sizeof(NtshEngn::Math::mat4);

		VkWriteDescriptorSet jointTransformsDescriptorWriteDescriptorSet = {};
		jointTransformsDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		jointTransformsDescriptorWriteDescriptorSet.pNext = nullptr;
		jointTransformsDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		jointTransformsDescriptorWriteDescriptorSet.dstBinding = 3;
		jointTransformsDescriptorWriteDescriptorSet.dstArrayElement = 0;
		jointTransformsDescriptorWriteDescriptorSet.descriptorCount = 1;
		jointTransformsDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		jointTransformsDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		jointTransformsDescriptorWriteDescriptorSet.pBufferInfo = &jointTransformsDescriptorBufferInfo;
		jointTransformsDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;
		writeDescriptorSets.push_back(jointTransformsDescriptorWriteDescriptorSet);

		VkDescriptorBufferInfo materialsDescriptorBufferInfo;
		materialsDescriptorBufferInfo.buffer = materialBuffers[i].handle;
		materialsDescriptorBufferInfo.offset = 0;
		materialsDescriptorBufferInfo.range = 32768;

		VkWriteDescriptorSet materialsDescriptorWriteDescriptorSet = {};
		materialsDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		materialsDescriptorWriteDescriptorSet.pNext = nullptr;
		materialsDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		materialsDescriptorWriteDescriptorSet.dstBinding = 4;
		materialsDescriptorWriteDescriptorSet.dstArrayElement = 0;
		materialsDescriptorWriteDescriptorSet.descriptorCount = 1;
		materialsDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		materialsDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		materialsDescriptorWriteDescriptorSet.pBufferInfo = &materialsDescriptorBufferInfo;
		materialsDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;
		writeDescriptorSets.push_back(materialsDescriptorWriteDescriptorSet);

		VkDescriptorBufferInfo lightsDescriptorBufferInfo;
		lightsDescriptorBufferInfo.buffer = lightBuffers[i].handle;
		lightsDescriptorBufferInfo.offset = 0;
		lightsDescriptorBufferInfo.range = 32768;

		VkWriteDescriptorSet lightsDescriptorWriteDescriptorSet = {};
		lightsDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		lightsDescriptorWriteDescriptorSet.pNext = nullptr;
		lightsDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		lightsDescriptorWriteDescriptorSet.dstBinding = 5;
		lightsDescriptorWriteDescriptorSet.dstArrayElement = 0;
		lightsDescriptorWriteDescriptorSet.descriptorCount = 1;
		lightsDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		lightsDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		lightsDescriptorWriteDescriptorSet.pBufferInfo = &lightsDescriptorBufferInfo;
		lightsDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;
		writeDescriptorSets.push_back(lightsDescriptorWriteDescriptorSet);

		VkDescriptorBufferInfo shadowSceneDescriptorBufferInfo;
		shadowSceneDescriptorBufferInfo.buffer = shadowSceneBuffers[i].handle;
		shadowSceneDescriptorBufferInfo.offset = 0;
		shadowSceneDescriptorBufferInfo.range = 65536;

		VkWriteDescriptorSet shadowSceneDescriptorWriteDescriptorSet = {};
		shadowSceneDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		shadowSceneDescriptorWriteDescriptorSet.pNext = nullptr;
		shadowSceneDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		shadowSceneDescriptorWriteDescriptorSet.dstBinding = 7;
		shadowSceneDescriptorWriteDescriptorSet.dstArrayElement = 0;
		shadowSceneDescriptorWriteDescriptorSet.descriptorCount = 1;
		shadowSceneDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		shadowSceneDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		shadowSceneDescriptorWriteDescriptorSet.pBufferInfo = &shadowSceneDescriptorBufferInfo;
		shadowSceneDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;
		writeDescriptorSets.push_back(shadowSceneDescriptorWriteDescriptorSet);

		vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}

	m_descriptorSetsNeedUpdate.resize(m_framesInFlight);
	m_descriptorSetsShadowNeedUpdate.resize(m_framesInFlight);
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		m_descriptorSetsNeedUpdate[i] = false;
		m_descriptorSetsShadowNeedUpdate[i] = false;
	}
}
