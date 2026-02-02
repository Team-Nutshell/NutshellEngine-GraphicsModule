#include "compositing.h"

void Compositing::init(VkDevice device,
	VkQueue graphicsQueue,
	uint32_t graphicsQueueFamilyIndex,
	VmaAllocator allocator,
	VkCommandPool initializationCommandPool,
	VkCommandBuffer initializationCommandBuffer,
	VkFence initializationFence,
	VkViewport viewport,
	VkRect2D scissor,
	uint32_t framesInFlight,
	const std::vector<HostVisibleVulkanBuffer>& cameraBuffers,
	const std::vector<HostVisibleVulkanBuffer>& lightBuffers,
	const std::vector<HostVisibleVulkanBuffer>& shadowSceneBuffers,
	VkImageView gBufferPositionView,
	VkImageView gBufferNormalView,
	VkImageView gBufferDiffuseView,
	VkImageView gBufferMaterialView,
	VkImageView gBufferEmissiveView,
	VkImageView ssaoImageView,
	PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR,
	PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR,
	PFN_vkCmdPipelineBarrier2KHR vkCmdPipelineBarrier2KHR) {
	m_device = device;
	m_graphicsQueue = graphicsQueue;
	m_graphicsQueueFamilyIndex = graphicsQueueFamilyIndex;
	m_allocator = allocator;
	m_initializationCommandPool = initializationCommandPool;
	m_initializationCommandBuffer = initializationCommandBuffer;
	m_initializationFence = initializationFence;
	m_viewport = viewport;
	m_scissor = scissor;
	m_framesInFlight = framesInFlight;
	m_vkCmdBeginRenderingKHR = vkCmdBeginRenderingKHR;
	m_vkCmdEndRenderingKHR = vkCmdEndRenderingKHR;
	m_vkCmdPipelineBarrier2KHR = vkCmdPipelineBarrier2KHR;

	createImage(m_scissor.extent.width, m_scissor.extent.height);
	createDescriptorSetLayout();
	createGraphicsPipeline();
	createSamplers();
	createDescriptorSets(cameraBuffers, lightBuffers, shadowSceneBuffers);
	updateDescriptorSets(gBufferPositionView, gBufferNormalView, gBufferDiffuseView, gBufferMaterialView, gBufferEmissiveView, ssaoImageView);

	m_descriptorSetsShadowNeedUpdate.resize(m_framesInFlight);
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		m_descriptorSetsShadowNeedUpdate[i] = false;
	}
}

void Compositing::destroy() {
	vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);

	vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_graphicsPipelineLayout, nullptr);

	vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);

	vkDestroySampler(m_device, m_shadowSampler, nullptr);
	vkDestroySampler(m_device, m_sampler, nullptr);

	m_image.destroy(m_device, m_allocator);
}

void Compositing::draw(VkCommandBuffer commandBuffer,
	uint32_t currentFrameInFlight,
	const NtshEngn::Math::vec4& backgroundColor) {
	VkRenderingAttachmentInfo compositingAttachmentInfo = {};
	compositingAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	compositingAttachmentInfo.pNext = nullptr;
	compositingAttachmentInfo.imageView = m_image.view;
	compositingAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	compositingAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
	compositingAttachmentInfo.resolveImageView = VK_NULL_HANDLE;
	compositingAttachmentInfo.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	compositingAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	compositingAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	compositingAttachmentInfo.clearValue.color = { backgroundColor.x, backgroundColor.y, backgroundColor.z, backgroundColor.w };

	VkRenderingInfo compositingRenderingInfo = {};
	compositingRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	compositingRenderingInfo.pNext = nullptr;
	compositingRenderingInfo.flags = 0;
	compositingRenderingInfo.renderArea = m_scissor;
	compositingRenderingInfo.layerCount = 1;
	compositingRenderingInfo.viewMask = 0;
	compositingRenderingInfo.colorAttachmentCount = 1;
	compositingRenderingInfo.pColorAttachments = &compositingAttachmentInfo;
	compositingRenderingInfo.pDepthAttachment = nullptr;
	compositingRenderingInfo.pStencilAttachment = nullptr;
	m_vkCmdBeginRenderingKHR(commandBuffer, &compositingRenderingInfo);

	// Bind descriptor set 0
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipelineLayout, 0, 1, &m_descriptorSets[currentFrameInFlight], 0, nullptr);

	// Bind graphics pipeline
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
	vkCmdSetViewport(commandBuffer, 0, 1, &m_viewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &m_scissor);

	vkCmdDraw(commandBuffer, 3, 1, 0, 0);

	m_vkCmdEndRenderingKHR(commandBuffer);
}

void Compositing::onResize(uint32_t width, uint32_t height, VkImageView gBufferPositionView, VkImageView gBufferNormalView, VkImageView gBufferDiffuseView, VkImageView gBufferMaterialView, VkImageView gBufferEmissiveView, VkImageView ssaoView) {
	m_viewport.width = static_cast<float>(width);
	m_viewport.height = static_cast<float>(height);
	m_scissor.extent.width = width;
	m_scissor.extent.height = height;

	m_image.destroy(m_device, m_allocator);
	createImage(width, height);

	updateDescriptorSets(gBufferPositionView, gBufferNormalView, gBufferDiffuseView, gBufferMaterialView, gBufferEmissiveView, ssaoView);
}

void Compositing::shadowDescriptorSetNeedsUpdate(uint32_t frameInFlight) {
	m_descriptorSetsShadowNeedUpdate[frameInFlight] = true;
}

VulkanImage& Compositing::getImage() {
	return m_image;
}

VkFormat Compositing::getImageFormat() {
	return m_imageFormat;
}

void Compositing::createImage(uint32_t width, uint32_t height) {
	VkExtent3D imageExtent;
	imageExtent.width = width;
	imageExtent.height = height;
	imageExtent.depth = 1;

	m_imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;

	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.pNext = nullptr;
	imageCreateInfo.flags = 0;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = m_imageFormat;
	imageCreateInfo.extent.width = imageExtent.width;
	imageCreateInfo.extent.height = imageExtent.height;
	imageCreateInfo.extent.depth = 1;
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.queueFamilyIndexCount = 1;
	imageCreateInfo.pQueueFamilyIndices = &m_graphicsQueueFamilyIndex;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo imageAllocationCreateInfo = {};
	imageAllocationCreateInfo.flags = 0;
	imageAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	NTSHENGN_VK_CHECK(vmaCreateImage(m_allocator, &imageCreateInfo, &imageAllocationCreateInfo, &m_image.handle, &m_image.allocation, nullptr));

	VkImageViewCreateInfo compositingImageViewCreateInfo = {};
	compositingImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	compositingImageViewCreateInfo.pNext = nullptr;
	compositingImageViewCreateInfo.flags = 0;
	compositingImageViewCreateInfo.image = m_image.handle;
	compositingImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	compositingImageViewCreateInfo.format = m_imageFormat;
	compositingImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
	compositingImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
	compositingImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
	compositingImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
	compositingImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	compositingImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	compositingImageViewCreateInfo.subresourceRange.levelCount = 1;
	compositingImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	compositingImageViewCreateInfo.subresourceRange.layerCount = 1;
	NTSHENGN_VK_CHECK(vkCreateImageView(m_device, &compositingImageViewCreateInfo, nullptr, &m_image.view));

	// Layout transition VK_IMAGE_LAYOUT_UNDEFINED -> VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	VkCommandPool commandPool;

	VkCommandPoolCreateInfo commandPoolCreateInfo = {};
	commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	commandPoolCreateInfo.pNext = nullptr;
	commandPoolCreateInfo.flags = 0;
	commandPoolCreateInfo.queueFamilyIndex = m_graphicsQueueFamilyIndex;
	NTSHENGN_VK_CHECK(vkCreateCommandPool(m_device, &commandPoolCreateInfo, nullptr, &commandPool));

	VkCommandBuffer commandBuffer;

	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.pNext = nullptr;
	commandBufferAllocateInfo.commandPool = commandPool;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = 1;
	NTSHENGN_VK_CHECK(vkAllocateCommandBuffers(m_device, &commandBufferAllocateInfo, &commandBuffer));

	VkCommandBufferBeginInfo commandBufferBeginInfo = {};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.pNext = nullptr;
	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	commandBufferBeginInfo.pInheritanceInfo = nullptr;
	NTSHENGN_VK_CHECK(vkBeginCommandBuffer(commandBuffer, &commandBufferBeginInfo));

	VkImageMemoryBarrier2 imageMemoryBarrier = {};
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	imageMemoryBarrier.pNext = nullptr;
	imageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
	imageMemoryBarrier.srcAccessMask = VK_ACCESS_2_NONE;
	imageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	imageMemoryBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	imageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	imageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	imageMemoryBarrier.image = m_image.handle;
	imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	imageMemoryBarrier.subresourceRange.levelCount = 1;
	imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	imageMemoryBarrier.subresourceRange.layerCount = 1;

	VkDependencyInfo dependencyInfo = {};
	dependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	dependencyInfo.pNext = nullptr;
	dependencyInfo.dependencyFlags = 0;
	dependencyInfo.memoryBarrierCount = 0;
	dependencyInfo.pMemoryBarriers = nullptr;
	dependencyInfo.bufferMemoryBarrierCount = 0;
	dependencyInfo.pBufferMemoryBarriers = nullptr;
	dependencyInfo.imageMemoryBarrierCount = 1;
	dependencyInfo.pImageMemoryBarriers = &imageMemoryBarrier;
	m_vkCmdPipelineBarrier2KHR(commandBuffer, &dependencyInfo);

	NTSHENGN_VK_CHECK(vkEndCommandBuffer(commandBuffer));

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.waitSemaphoreCount = 0;
	submitInfo.pWaitSemaphores = nullptr;
	submitInfo.pWaitDstStageMask = nullptr;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;
	submitInfo.signalSemaphoreCount = 0;
	submitInfo.pSignalSemaphores = nullptr;
	NTSHENGN_VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_initializationFence));
	NTSHENGN_VK_CHECK(vkWaitForFences(m_device, 1, &m_initializationFence, VK_TRUE, std::numeric_limits<uint64_t>::max()));
	NTSHENGN_VK_CHECK(vkResetFences(m_device, 1, &m_initializationFence));

	vkDestroyCommandPool(m_device, commandPool, nullptr);
}

void Compositing::createDescriptorSetLayout() {
	VkDescriptorSetLayoutBinding cameraDescriptorSetLayoutBinding = {};
	cameraDescriptorSetLayoutBinding.binding = 0;
	cameraDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	cameraDescriptorSetLayoutBinding.descriptorCount = 1;
	cameraDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	cameraDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding lightsDescriptorSetLayoutBinding = {};
	lightsDescriptorSetLayoutBinding.binding = 1;
	lightsDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	lightsDescriptorSetLayoutBinding.descriptorCount = 1;
	lightsDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	lightsDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding gBufferPositionDescriptorSetLayoutBinding = {};
	gBufferPositionDescriptorSetLayoutBinding.binding = 2;
	gBufferPositionDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	gBufferPositionDescriptorSetLayoutBinding.descriptorCount = 1;
	gBufferPositionDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	gBufferPositionDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding gBufferNormalDescriptorSetLayoutBinding = {};
	gBufferNormalDescriptorSetLayoutBinding.binding = 3;
	gBufferNormalDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	gBufferNormalDescriptorSetLayoutBinding.descriptorCount = 1;
	gBufferNormalDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	gBufferNormalDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding gBufferDiffuseDescriptorSetLayoutBinding = {};
	gBufferDiffuseDescriptorSetLayoutBinding.binding = 4;
	gBufferDiffuseDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	gBufferDiffuseDescriptorSetLayoutBinding.descriptorCount = 1;
	gBufferDiffuseDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	gBufferDiffuseDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding gBufferMaterialDescriptorSetLayoutBinding = {};
	gBufferMaterialDescriptorSetLayoutBinding.binding = 5;
	gBufferMaterialDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	gBufferMaterialDescriptorSetLayoutBinding.descriptorCount = 1;
	gBufferMaterialDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	gBufferMaterialDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding gBufferEmissiveDescriptorSetLayoutBinding = {};
	gBufferEmissiveDescriptorSetLayoutBinding.binding = 6;
	gBufferEmissiveDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	gBufferEmissiveDescriptorSetLayoutBinding.descriptorCount = 1;
	gBufferEmissiveDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	gBufferEmissiveDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding ssaoDescriptorSetLayoutBinding = {};
	ssaoDescriptorSetLayoutBinding.binding = 7;
	ssaoDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	ssaoDescriptorSetLayoutBinding.descriptorCount = 1;
	ssaoDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	ssaoDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding cascadeSceneDescriptorSetLayoutBinding = {};
	cascadeSceneDescriptorSetLayoutBinding.binding = 8;
	cascadeSceneDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	cascadeSceneDescriptorSetLayoutBinding.descriptorCount = 1;
	cascadeSceneDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	cascadeSceneDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding shadowMapsDescriptorSetLayoutBinding = {};
	shadowMapsDescriptorSetLayoutBinding.binding = 9;
	shadowMapsDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	shadowMapsDescriptorSetLayoutBinding.descriptorCount = 131072;
	shadowMapsDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	shadowMapsDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	std::array<VkDescriptorBindingFlags, 10> descriptorBindingFlags = { 0, 0, 0, 0, 0, 0, 0, 0, 0, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT };
	VkDescriptorSetLayoutBindingFlagsCreateInfo descriptorSetLayoutBindingFlagsCreateInfo = {};
	descriptorSetLayoutBindingFlagsCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	descriptorSetLayoutBindingFlagsCreateInfo.pNext = nullptr;
	descriptorSetLayoutBindingFlagsCreateInfo.bindingCount = static_cast<uint32_t>(descriptorBindingFlags.size());
	descriptorSetLayoutBindingFlagsCreateInfo.pBindingFlags = descriptorBindingFlags.data();

	std::array<VkDescriptorSetLayoutBinding, 10> compositingDescriptorSetLayoutBindings = { cameraDescriptorSetLayoutBinding, lightsDescriptorSetLayoutBinding, gBufferPositionDescriptorSetLayoutBinding, gBufferNormalDescriptorSetLayoutBinding, gBufferDiffuseDescriptorSetLayoutBinding, gBufferMaterialDescriptorSetLayoutBinding, gBufferEmissiveDescriptorSetLayoutBinding, ssaoDescriptorSetLayoutBinding, cascadeSceneDescriptorSetLayoutBinding, shadowMapsDescriptorSetLayoutBinding };
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.pNext = &descriptorSetLayoutBindingFlagsCreateInfo;
	descriptorSetLayoutCreateInfo.flags = 0;
	descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(compositingDescriptorSetLayoutBindings.size());
	descriptorSetLayoutCreateInfo.pBindings = compositingDescriptorSetLayoutBindings.data();
	NTSHENGN_VK_CHECK(vkCreateDescriptorSetLayout(m_device, &descriptorSetLayoutCreateInfo, nullptr, &m_descriptorSetLayout));
}

void Compositing::createGraphicsPipeline() {
	VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {};
	pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	pipelineRenderingCreateInfo.pNext = nullptr;
	pipelineRenderingCreateInfo.viewMask = 0;
	pipelineRenderingCreateInfo.colorAttachmentCount = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &m_imageFormat;
	pipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
	pipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

	const std::string vertexShaderCode = R"GLSL(
		#version 460

		layout(location = 0) out vec2 outUv;

		void main() {
			outUv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
			gl_Position = vec4(outUv * 2.0 + -1.0, 0.0, 1.0);
		}
	)GLSL";
	const std::vector<uint32_t> vertexShaderSpv = compileShader(vertexShaderCode, ShaderType::Vertex);

	VkShaderModule vertexShaderModule;
	VkShaderModuleCreateInfo vertexShaderModuleCreateInfo = {};
	vertexShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertexShaderModuleCreateInfo.pNext = nullptr;
	vertexShaderModuleCreateInfo.flags = 0;
	vertexShaderModuleCreateInfo.codeSize = vertexShaderSpv.size() * sizeof(uint32_t);
	vertexShaderModuleCreateInfo.pCode = vertexShaderSpv.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &vertexShaderModuleCreateInfo, nullptr, &vertexShaderModule));

	VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo = {};
	vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderStageCreateInfo.pNext = nullptr;
	vertexShaderStageCreateInfo.flags = 0;
	vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexShaderStageCreateInfo.module = vertexShaderModule;
	vertexShaderStageCreateInfo.pName = "main";
	vertexShaderStageCreateInfo.pSpecializationInfo = nullptr;

	std::string fragmentShaderCode = R"GLSL(
		#version 460
		#extension GL_EXT_nonuniform_qualifier : enable

		#define SHADOW_MAPPING_CASCADE_COUNT )GLSL";
	fragmentShaderCode += std::to_string(SHADOW_MAPPING_CASCADE_COUNT);
	fragmentShaderCode += R"GLSL(
		#define M_PI 3.1415926535897932384626433832795

		const mat4 shadowOffset = mat4(
			0.5, 0.0, 0.0, 0.0,
			0.0, 0.5, 0.0, 0.0,
			0.0, 0.0, 1.0, 0.0,
			0.5, 0.5, 0.0, 1.0
		);

		// BRDF
		float distribution(float NdotH, float roughness) {
			const float a = roughness * roughness;
			const float aSquare = a * a;
			const float NdotHSquare = NdotH * NdotH;
			const float denom = NdotHSquare * (aSquare - 1.0) + 1.0;

			return aSquare / (M_PI * denom * denom);
		}

		vec3 fresnel(float cosTheta, vec3 f0) {
			return f0 + (1.0 - f0) * pow(1.0 - cosTheta, 5.0);
		}

		float g(float NdotV, float roughness) {
			const float r = roughness + 1.0;
			const float k = (r * r) / 8.0;
			const float denom = NdotV * (1.0 - k) + k;

			return NdotV / denom;
		}

		float smith(float LdotN, float VdotN, float roughness) {
			const float gv = g(VdotN, roughness);
			const float gl = g(LdotN, roughness);

			return gv * gl;
		}

		vec3 diffuseFresnelCorrection(vec3 ior) {
			const vec3 iorSquare = ior * ior;
			const bvec3 TIR = lessThan(ior, vec3(1.0));
			const vec3 invDenum = mix(vec3(1.0), vec3(1.0) / (iorSquare * iorSquare * (vec3(554.33) * 380.7 * ior)), TIR);
			vec3 num = ior * mix(vec3(0.1921156102251088), ior * 298.25 - 261.38 * iorSquare + 138.43, TIR);
			num += mix(vec3(0.8078843897748912), vec3(-1.07), TIR);

			return num * invDenum;
		}

		vec3 brdf(float LdotH, float NdotH, float VdotH, float LdotN, float VdotN, vec3 diffuse, float metalness, float roughness) {
			const float d = distribution(NdotH, roughness);
			const vec3 f = fresnel(LdotH, mix(vec3(0.04), diffuse, metalness));
			const vec3 fT = fresnel(LdotN, mix(vec3(0.04), diffuse, metalness));
			const vec3 fTIR = fresnel(VdotN, mix(vec3(0.04), diffuse, metalness));
			const float g = smith(LdotN, VdotN, roughness);
			const vec3 dfc = diffuseFresnelCorrection(vec3(1.05));

			const vec3 lambertian = diffuse / M_PI;

			return (d * f * g) / max(4.0 * LdotN * VdotN, 0.001) + ((vec3(1.0) - fT) * (vec3(1.0 - fTIR)) * lambertian) * dfc;
		}

		vec3 shade(vec3 n, vec3 v, vec3 l, vec3 lc, vec3 diffuse, float metalness, float roughness) {
			const vec3 h = normalize(v + l);

			const float LdotH = max(dot(l, h), 0.0);
			const float NdotH = max(dot(n, h), 0.0);
			const float VdotH = max(dot(v, h), 0.0);
			const float LdotN = max(dot(l, n), 0.0);
			const float VdotN = max(dot(v, n), 0.0);

			const vec3 brdf = brdf(LdotH, NdotH, VdotH, LdotN, VdotN, diffuse, metalness, roughness);
	
			return lc * brdf * LdotN;
		}

		vec3 sRGBToLinear(vec3 rgb) {
			return mix(pow((rgb + 0.055) * (1.0 / 1.055), vec3(2.4)), rgb * (1.0 / 12.92), lessThanEqual(rgb, vec3(0.04045)));
		}

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

		layout(set = 0, binding = 1) restrict readonly buffer Lights {
			uvec4 count;
			LightInfo info[];
		} lights;

		layout(set = 0, binding = 2) uniform sampler2D gBufferPositionSampler;
		layout(set = 0, binding = 3) uniform sampler2D gBufferNormalSampler;
		layout(set = 0, binding = 4) uniform sampler2D gBufferDiffuseSampler;
		layout(set = 0, binding = 5) uniform sampler2D gBufferMaterialSampler;
		layout(set = 0, binding = 6) uniform sampler2D gBufferEmissiveSampler;

		layout(set = 0, binding = 7) uniform sampler2D ssaoSampler;

		layout(set = 0, binding = 8) restrict readonly buffer Shadows {
			ShadowInfo info[];
		} shadows;

		layout(set = 0, binding = 9) uniform sampler2DArray shadowMaps[];
		layout(set = 0, binding = 9) uniform samplerCube shadowCubeMaps[];

		layout(location = 0) in vec2 uv;

		layout(location = 0) out vec4 outColor;

		// Shadows
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

		void main() {
			vec4 diffuseSample = texture(gBufferDiffuseSampler, uv);
			if (diffuseSample.a == 0.0f) {
				discard;
			}
			diffuseSample.rgb = sRGBToLinear(diffuseSample.rgb);
			const vec3 positionSample = texture(gBufferPositionSampler, uv).xyz;
			const vec3 normalSample = texture(gBufferNormalSampler, uv).xyz;
			const vec3 materialSample = texture(gBufferMaterialSampler, uv).xyz;
			const float metalnessSample = materialSample.b;
			const float roughnessSample = materialSample.g;
			const float occlusionSample = materialSample.r;
			vec3 emissiveSample = texture(gBufferEmissiveSampler, uv).rgb;
			emissiveSample.rgb = sRGBToLinear(emissiveSample.rgb);

			const float ssaoSample = texture(ssaoSampler, uv).r;

			const vec3 position = positionSample;
			const vec3 viewPosition = vec3(camera.view * vec4(position, 1.0));
			const vec3 n = normalSample;
			const vec3 d = vec3(diffuseSample);
			const vec3 v = normalize(camera.position - position);

			vec3 color = vec3(0.0);

			uint lightIndex = 0;
			// Directional Lights
			for (uint i = 0; i < lights.count.x; i++) {
				const vec3 l = -lights.info[lightIndex].direction;

				uint cascadeIndex = 0;
				for (uint j = 0; j < SHADOW_MAPPING_CASCADE_COUNT - 1; j++) {
					if (viewPosition.z < shadows.info[i * SHADOW_MAPPING_CASCADE_COUNT + j].splitDepth) {
						cascadeIndex = j + 1;
					}
				}

				const vec4 shadowCoord = (shadowOffset * shadows.info[(i * SHADOW_MAPPING_CASCADE_COUNT) + cascadeIndex].viewProj) * vec4(position, 1.0);

				color += shade(n, v, l, lights.info[lightIndex].color * lights.info[lightIndex].intensity, d, metalnessSample, roughnessSample) * shadowValue(i, cascadeIndex, shadowCoord / shadowCoord.w, 0.00005);

				lightIndex++;
			}
			// Point Lights
			for (uint i = 0; i < lights.count.y; i++) {
				const vec3 l = normalize(lights.info[lightIndex].position - position);

				const float distance = length(lights.info[lightIndex].position - position);
				if (lights.info[lightIndex].distance >= distance) {
					const float attenuation = 1.0 / (distance * distance);
					const vec3 radiance = (lights.info[lightIndex].color * lights.info[lightIndex].intensity) * attenuation;

					color += shade(n, v, l, radiance, d, metalnessSample, roughnessSample) * shadowCubeValue(lightIndex, position - lights.info[lightIndex].position, 0.05);
				}

				lightIndex++;
			}
			// Spot Lights
			for (uint i = 0; i < lights.count.z; i++) {
				const float distance = length(lights.info[lightIndex].position - position);
				if (lights.info[lightIndex].distance >= distance) {
					const vec3 l = normalize(lights.info[lightIndex].position - position);
					const float theta = dot(l, -lights.info[lightIndex].direction);
					const float epsilon = cos(lights.info[lightIndex].cutoff.y) - cos(lights.info[lightIndex].cutoff.x);
					const float intensity = 1.0 - clamp((theta - cos(lights.info[lightIndex].cutoff.x)) / epsilon, 0.0, 1.0);

					const vec4 shadowCoord = (shadowOffset * shadows.info[(lights.count.x * SHADOW_MAPPING_CASCADE_COUNT) + (lights.count.y * 6) + i].viewProj) * vec4(position, 1.0);

					color += shade(n, v, l, (lights.info[lightIndex].color * lights.info[lightIndex].intensity) * intensity, d * intensity, metalnessSample, roughnessSample) * shadowValue(lightIndex, 0, shadowCoord / shadowCoord.w, 0.00005);
				}

				lightIndex++;
			}
			// Ambient Lights
			for (uint i = 0; i < lights.count.w; i++) {
				color += (lights.info[lightIndex].color * lights.info[lightIndex].intensity) * d;

				lightIndex++;
			}

			color *= occlusionSample;
			color *= ssaoSample;
			color += emissiveSample;

			outColor = vec4(color, 1.0);
		}
	)GLSL";
	const std::vector<uint32_t> fragmentShaderSpv = compileShader(fragmentShaderCode, ShaderType::Fragment);

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

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
	vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateCreateInfo.pNext = nullptr;
	vertexInputStateCreateInfo.flags = 0;
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexBindingDescriptions = nullptr;
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = nullptr;

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
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
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
	depthStencilStateCreateInfo.depthTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.depthWriteEnable = VK_FALSE;
	depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_NEVER;
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

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.pNext = nullptr;
	pipelineLayoutCreateInfo.flags = 0;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &m_descriptorSetLayout;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;
	NTSHENGN_VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_graphicsPipelineLayout));

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
	graphicsPipelineCreateInfo.layout = m_graphicsPipelineLayout;
	graphicsPipelineCreateInfo.renderPass = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.subpass = 0;
	graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.basePipelineIndex = 0;
	NTSHENGN_VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &m_graphicsPipeline));

	vkDestroyShaderModule(m_device, vertexShaderModule, nullptr);
	vkDestroyShaderModule(m_device, fragmentShaderModule, nullptr);
}

void Compositing::createSamplers() {
	VkSamplerCreateInfo samplerCreateInfo = {};
	samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	samplerCreateInfo.pNext = nullptr;
	samplerCreateInfo.flags = 0;
	samplerCreateInfo.magFilter = VK_FILTER_NEAREST;
	samplerCreateInfo.minFilter = VK_FILTER_NEAREST;
	samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
	samplerCreateInfo.mipLodBias = 0.0f;
	samplerCreateInfo.anisotropyEnable = VK_FALSE;
	samplerCreateInfo.maxAnisotropy = 0.0f;
	samplerCreateInfo.compareEnable = VK_FALSE;
	samplerCreateInfo.compareOp = VK_COMPARE_OP_NEVER;
	samplerCreateInfo.minLod = 0.0f;
	samplerCreateInfo.maxLod = 0.0f;
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
	samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
	NTSHENGN_VK_CHECK(vkCreateSampler(m_device, &samplerCreateInfo, nullptr, &m_sampler));

	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	NTSHENGN_VK_CHECK(vkCreateSampler(m_device, &samplerCreateInfo, nullptr, &m_shadowSampler));
}

void Compositing::createDescriptorSets(const std::vector<HostVisibleVulkanBuffer>& cameraBuffers, const std::vector<HostVisibleVulkanBuffer>& lightBuffers, const std::vector<HostVisibleVulkanBuffer>& shadowSceneBuffers) {
	// Create descriptor pool
	VkDescriptorPoolSize cameraDescriptorPoolSize = {};
	cameraDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	cameraDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize lightsDescriptorPoolSize = {};
	lightsDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	lightsDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize gBufferImagesDescriptorPoolSize = {};
	gBufferImagesDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	gBufferImagesDescriptorPoolSize.descriptorCount = 5 * m_framesInFlight;

	VkDescriptorPoolSize ssaoDescriptorPoolSize = {};
	ssaoDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	ssaoDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize cascadeSceneDescriptorPoolSize = {};
	cascadeSceneDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	cascadeSceneDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize shadowMapsDescriptorPoolSize = {};
	shadowMapsDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	shadowMapsDescriptorPoolSize.descriptorCount = 131072 * m_framesInFlight;

	std::array<VkDescriptorPoolSize, 6> descriptorPoolSizes = { cameraDescriptorPoolSize, lightsDescriptorPoolSize, gBufferImagesDescriptorPoolSize, ssaoDescriptorPoolSize, cascadeSceneDescriptorPoolSize, shadowMapsDescriptorPoolSize };
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext = nullptr;
	descriptorPoolCreateInfo.flags = 0;
	descriptorPoolCreateInfo.maxSets = m_framesInFlight;
	descriptorPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size());
	descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes.data();
	NTSHENGN_VK_CHECK(vkCreateDescriptorPool(m_device, &descriptorPoolCreateInfo, nullptr, &m_descriptorPool));

	// Allocate descriptor set
	m_descriptorSets.resize(m_framesInFlight);
	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
	descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetAllocateInfo.pNext = nullptr;
	descriptorSetAllocateInfo.descriptorPool = m_descriptorPool;
	descriptorSetAllocateInfo.descriptorSetCount = 1;
	descriptorSetAllocateInfo.pSetLayouts = &m_descriptorSetLayout;
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		NTSHENGN_VK_CHECK(vkAllocateDescriptorSets(m_device, &descriptorSetAllocateInfo, &m_descriptorSets[i]));
	}

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
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

		VkDescriptorBufferInfo lightsDescriptorBufferInfo;
		lightsDescriptorBufferInfo.buffer = lightBuffers[i].handle;
		lightsDescriptorBufferInfo.offset = 0;
		lightsDescriptorBufferInfo.range = 32768;

		VkWriteDescriptorSet lightsDescriptorWriteDescriptorSet = {};
		lightsDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		lightsDescriptorWriteDescriptorSet.pNext = nullptr;
		lightsDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		lightsDescriptorWriteDescriptorSet.dstBinding = 1;
		lightsDescriptorWriteDescriptorSet.dstArrayElement = 0;
		lightsDescriptorWriteDescriptorSet.descriptorCount = 1;
		lightsDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		lightsDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		lightsDescriptorWriteDescriptorSet.pBufferInfo = &lightsDescriptorBufferInfo;
		lightsDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

		VkDescriptorBufferInfo shadowSceneDescriptorBufferInfo;
		shadowSceneDescriptorBufferInfo.buffer = shadowSceneBuffers[i].handle;
		shadowSceneDescriptorBufferInfo.offset = 0;
		shadowSceneDescriptorBufferInfo.range = 65536;

		VkWriteDescriptorSet shadowSceneDescriptorWriteDescriptorSet = {};
		shadowSceneDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		shadowSceneDescriptorWriteDescriptorSet.pNext = nullptr;
		shadowSceneDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		shadowSceneDescriptorWriteDescriptorSet.dstBinding = 8;
		shadowSceneDescriptorWriteDescriptorSet.dstArrayElement = 0;
		shadowSceneDescriptorWriteDescriptorSet.descriptorCount = 1;
		shadowSceneDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		shadowSceneDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		shadowSceneDescriptorWriteDescriptorSet.pBufferInfo = &shadowSceneDescriptorBufferInfo;
		shadowSceneDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

		std::array<VkWriteDescriptorSet, 3> writeDescriptorSets = { cameraDescriptorWriteDescriptorSet, lightsDescriptorWriteDescriptorSet, shadowSceneDescriptorWriteDescriptorSet };

		vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}
}

void Compositing::updateDescriptorSets(VkImageView gBufferPositionView, VkImageView gBufferNormalView, VkImageView gBufferDiffuseView, VkImageView gBufferMaterialView, VkImageView gBufferEmissiveView, VkImageView ssaoImageView) {
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		VkDescriptorImageInfo gBufferPositionImageDescriptorImageInfo;
		gBufferPositionImageDescriptorImageInfo.sampler = m_sampler;
		gBufferPositionImageDescriptorImageInfo.imageView = gBufferPositionView;
		gBufferPositionImageDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet gBufferPositionImageDescriptorWriteDescriptorSet = {};
		gBufferPositionImageDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		gBufferPositionImageDescriptorWriteDescriptorSet.pNext = nullptr;
		gBufferPositionImageDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		gBufferPositionImageDescriptorWriteDescriptorSet.dstBinding = 2;
		gBufferPositionImageDescriptorWriteDescriptorSet.dstArrayElement = 0;
		gBufferPositionImageDescriptorWriteDescriptorSet.descriptorCount = 1;
		gBufferPositionImageDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		gBufferPositionImageDescriptorWriteDescriptorSet.pImageInfo = &gBufferPositionImageDescriptorImageInfo;
		gBufferPositionImageDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
		gBufferPositionImageDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

		VkDescriptorImageInfo gBufferNormalImageDescriptorImageInfo;
		gBufferNormalImageDescriptorImageInfo.sampler = m_sampler;
		gBufferNormalImageDescriptorImageInfo.imageView = gBufferNormalView;
		gBufferNormalImageDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet gBufferNormalImageDescriptorWriteDescriptorSet = {};
		gBufferNormalImageDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		gBufferNormalImageDescriptorWriteDescriptorSet.pNext = nullptr;
		gBufferNormalImageDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		gBufferNormalImageDescriptorWriteDescriptorSet.dstBinding = 3;
		gBufferNormalImageDescriptorWriteDescriptorSet.dstArrayElement = 0;
		gBufferNormalImageDescriptorWriteDescriptorSet.descriptorCount = 1;
		gBufferNormalImageDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		gBufferNormalImageDescriptorWriteDescriptorSet.pImageInfo = &gBufferNormalImageDescriptorImageInfo;
		gBufferNormalImageDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
		gBufferNormalImageDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

		VkDescriptorImageInfo gBufferDiffuseImageDescriptorImageInfo;
		gBufferDiffuseImageDescriptorImageInfo.sampler = m_sampler;
		gBufferDiffuseImageDescriptorImageInfo.imageView = gBufferDiffuseView;
		gBufferDiffuseImageDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet gBufferDiffuseImageDescriptorWriteDescriptorSet = {};
		gBufferDiffuseImageDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		gBufferDiffuseImageDescriptorWriteDescriptorSet.pNext = nullptr;
		gBufferDiffuseImageDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		gBufferDiffuseImageDescriptorWriteDescriptorSet.dstBinding = 4;
		gBufferDiffuseImageDescriptorWriteDescriptorSet.dstArrayElement = 0;
		gBufferDiffuseImageDescriptorWriteDescriptorSet.descriptorCount = 1;
		gBufferDiffuseImageDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		gBufferDiffuseImageDescriptorWriteDescriptorSet.pImageInfo = &gBufferDiffuseImageDescriptorImageInfo;
		gBufferDiffuseImageDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
		gBufferDiffuseImageDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

		VkDescriptorImageInfo gBufferMaterialImageDescriptorImageInfo;
		gBufferMaterialImageDescriptorImageInfo.sampler = m_sampler;
		gBufferMaterialImageDescriptorImageInfo.imageView = gBufferMaterialView;
		gBufferMaterialImageDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet gBufferMaterialImageDescriptorWriteDescriptorSet = {};
		gBufferMaterialImageDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		gBufferMaterialImageDescriptorWriteDescriptorSet.pNext = nullptr;
		gBufferMaterialImageDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		gBufferMaterialImageDescriptorWriteDescriptorSet.dstBinding = 5;
		gBufferMaterialImageDescriptorWriteDescriptorSet.dstArrayElement = 0;
		gBufferMaterialImageDescriptorWriteDescriptorSet.descriptorCount = 1;
		gBufferMaterialImageDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		gBufferMaterialImageDescriptorWriteDescriptorSet.pImageInfo = &gBufferMaterialImageDescriptorImageInfo;
		gBufferMaterialImageDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
		gBufferMaterialImageDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

		VkDescriptorImageInfo gBufferEmissiveImageDescriptorImageInfo;
		gBufferEmissiveImageDescriptorImageInfo.sampler = m_sampler;
		gBufferEmissiveImageDescriptorImageInfo.imageView = gBufferEmissiveView;
		gBufferEmissiveImageDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet gBufferEmissiveImageDescriptorWriteDescriptorSet = {};
		gBufferEmissiveImageDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		gBufferEmissiveImageDescriptorWriteDescriptorSet.pNext = nullptr;
		gBufferEmissiveImageDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		gBufferEmissiveImageDescriptorWriteDescriptorSet.dstBinding = 6;
		gBufferEmissiveImageDescriptorWriteDescriptorSet.dstArrayElement = 0;
		gBufferEmissiveImageDescriptorWriteDescriptorSet.descriptorCount = 1;
		gBufferEmissiveImageDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		gBufferEmissiveImageDescriptorWriteDescriptorSet.pImageInfo = &gBufferEmissiveImageDescriptorImageInfo;
		gBufferEmissiveImageDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
		gBufferEmissiveImageDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

		VkDescriptorImageInfo ssaoDescriptorImageInfo;
		ssaoDescriptorImageInfo.sampler = m_sampler;
		ssaoDescriptorImageInfo.imageView = ssaoImageView;
		ssaoDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet ssaoDescriptorWriteDescriptorSet = {};
		ssaoDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		ssaoDescriptorWriteDescriptorSet.pNext = nullptr;
		ssaoDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		ssaoDescriptorWriteDescriptorSet.dstBinding = 7;
		ssaoDescriptorWriteDescriptorSet.dstArrayElement = 0;
		ssaoDescriptorWriteDescriptorSet.descriptorCount = 1;
		ssaoDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		ssaoDescriptorWriteDescriptorSet.pImageInfo = &ssaoDescriptorImageInfo;
		ssaoDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
		ssaoDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

		std::array<VkWriteDescriptorSet, 6> writeDescriptorSets = { gBufferPositionImageDescriptorWriteDescriptorSet, gBufferNormalImageDescriptorWriteDescriptorSet, gBufferDiffuseImageDescriptorWriteDescriptorSet, gBufferMaterialImageDescriptorWriteDescriptorSet, gBufferEmissiveImageDescriptorWriteDescriptorSet, ssaoDescriptorWriteDescriptorSet };
		vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}
}

void Compositing::updateShadowDescriptorSets(uint32_t frameInFlight, const std::vector<VulkanImage> shadowMaps) {
	if (!m_descriptorSetsShadowNeedUpdate[frameInFlight]) {
		return;
	}

	std::vector<VkDescriptorImageInfo> shadowMapImageDescriptorImageInfos(shadowMaps.size());
	for (uint32_t i = 0; i < shadowMaps.size(); i++) {
		shadowMapImageDescriptorImageInfos[i].sampler = m_shadowSampler;
		shadowMapImageDescriptorImageInfos[i].imageView = shadowMaps[i].view;
		shadowMapImageDescriptorImageInfos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	}

	VkWriteDescriptorSet shadowMapImageDescriptorWriteDescriptorSet = {};
	shadowMapImageDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	shadowMapImageDescriptorWriteDescriptorSet.pNext = nullptr;
	shadowMapImageDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[frameInFlight];
	shadowMapImageDescriptorWriteDescriptorSet.dstBinding = 9;
	shadowMapImageDescriptorWriteDescriptorSet.dstArrayElement = 0;
	shadowMapImageDescriptorWriteDescriptorSet.descriptorCount = static_cast<uint32_t>(shadowMapImageDescriptorImageInfos.size());
	shadowMapImageDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	shadowMapImageDescriptorWriteDescriptorSet.pImageInfo = shadowMapImageDescriptorImageInfos.data();
	shadowMapImageDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
	shadowMapImageDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

	vkUpdateDescriptorSets(m_device, 1, &shadowMapImageDescriptorWriteDescriptorSet, 0, nullptr);
}
