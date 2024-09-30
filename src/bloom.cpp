#include "bloom.h"
#include <algorithm>
#include <cmath>

void Bloom::init(VkDevice device,
	VkQueue graphicsQueue,
	uint32_t graphicsQueueFamilyIndex,
	VmaAllocator allocator,
	VkImageView drawImageView,
	VkFormat drawImageFormat,
	VkCommandPool initializationCommandPool,
	VkCommandBuffer initializationCommandBuffer,
	VkFence initializationFence,
	VkViewport viewport,
	VkRect2D scissor,
	PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR,
	PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR,
	PFN_vkCmdPipelineBarrier2KHR vkCmdPipelineBarrier2KHR) {
	m_device = device;
	m_graphicsQueue = graphicsQueue;
	m_graphicsQueueFamilyIndex = graphicsQueueFamilyIndex;
	m_initializationCommandPool = initializationCommandPool;
	m_initializationCommandBuffer = initializationCommandBuffer;
	m_initializationFence = initializationFence;
	m_allocator = allocator;
	m_viewport = viewport;
	m_scissor = scissor;
	m_downscaledViewport = viewport;
	m_downscaledViewport.width = std::max(viewport.width / static_cast<float>(BLOOM_DOWNSCALE), 1.0f);
	m_downscaledViewport.height = std::max(viewport.height / static_cast<float>(BLOOM_DOWNSCALE), 1.0f);
	m_downscaledScissor = scissor;
	m_downscaledScissor.extent.width = static_cast<uint32_t>(m_downscaledViewport.width);
	m_downscaledScissor.extent.height = static_cast<uint32_t>(m_downscaledViewport.height);
	m_vkCmdBeginRenderingKHR = vkCmdBeginRenderingKHR;
	m_vkCmdEndRenderingKHR = vkCmdEndRenderingKHR;
	m_vkCmdPipelineBarrier2KHR = vkCmdPipelineBarrier2KHR;

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
	samplerCreateInfo.maxLod = VK_LOD_CLAMP_NONE;
	samplerCreateInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
	samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
	NTSHENGN_VK_CHECK(vkCreateSampler(m_device, &samplerCreateInfo, nullptr, &m_nearestSampler));

	samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
	samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
	NTSHENGN_VK_CHECK(vkCreateSampler(m_device, &samplerCreateInfo, nullptr, &m_linearSampler));

	createImages(m_downscaledScissor.extent.width, m_downscaledScissor.extent.height);
	createDescriptorSetLayouts();
	createGraphicsPipelines(drawImageFormat);
	createDescriptorSets();
	updateDescriptorSets(drawImageView);
}

void Bloom::destroy() {
	vkDestroyDescriptorPool(m_device, m_bloomDescriptorPool, nullptr);
	vkDestroyDescriptorPool(m_device, m_blurDescriptorPool, nullptr);
	vkDestroyDescriptorPool(m_device, m_resizeThresholdDescriptorPool, nullptr);

	vkDestroyPipeline(m_device, m_bloomGraphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_bloomGraphicsPipelineLayout, nullptr);
	vkDestroyPipeline(m_device, m_blurGraphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_blurGraphicsPipelineLayout, nullptr);
	vkDestroyPipeline(m_device, m_resizeThresholdGraphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_resizeThresholdGraphicsPipelineLayout, nullptr);

	vkDestroyDescriptorSetLayout(m_device, m_bloomDescriptorSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(m_device, m_blurDescriptorSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(m_device, m_resizeThresholdDescriptorSetLayout, nullptr);

	vkDestroySampler(m_device, m_linearSampler, nullptr);
	vkDestroySampler(m_device, m_nearestSampler, nullptr);

	destroyImages();
}

void Bloom::draw(VkCommandBuffer commandBuffer, VkImage drawImage, VkImageView drawImageView) {
	VkImageMemoryBarrier2 bloomImageMemoryBarrier = {};
	bloomImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	bloomImageMemoryBarrier.pNext = nullptr;
	bloomImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	bloomImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	bloomImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	bloomImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	bloomImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	bloomImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	bloomImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	bloomImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	bloomImageMemoryBarrier.image = m_bloomImage.handle;
	bloomImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	bloomImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	bloomImageMemoryBarrier.subresourceRange.levelCount = m_mipLevels;
	bloomImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	bloomImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkDependencyInfo bloomDependencyInfo = {};
	bloomDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	bloomDependencyInfo.pNext = nullptr;
	bloomDependencyInfo.dependencyFlags = 0;
	bloomDependencyInfo.memoryBarrierCount = 0;
	bloomDependencyInfo.pMemoryBarriers = nullptr;
	bloomDependencyInfo.bufferMemoryBarrierCount = 0;
	bloomDependencyInfo.pBufferMemoryBarriers = nullptr;
	bloomDependencyInfo.imageMemoryBarrierCount = 1;
	bloomDependencyInfo.pImageMemoryBarriers = &bloomImageMemoryBarrier;
	m_vkCmdPipelineBarrier2KHR(commandBuffer, &bloomDependencyInfo);

	// Resize and Threshold
	VkRenderingAttachmentInfo resizeThresholdAttachmentInfo = {};
	resizeThresholdAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	resizeThresholdAttachmentInfo.pNext = nullptr;
	resizeThresholdAttachmentInfo.imageView = m_bloomImage.layerMipViews[0];
	resizeThresholdAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	resizeThresholdAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
	resizeThresholdAttachmentInfo.resolveImageView = VK_NULL_HANDLE;
	resizeThresholdAttachmentInfo.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	resizeThresholdAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	resizeThresholdAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	resizeThresholdAttachmentInfo.clearValue.color = { 0.0f, 0.0f, 0.0f, 0.0f };

	VkRenderingInfo resizeThresholdRenderingInfo = {};
	resizeThresholdRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	resizeThresholdRenderingInfo.pNext = nullptr;
	resizeThresholdRenderingInfo.flags = 0;
	resizeThresholdRenderingInfo.renderArea = m_downscaledScissor;
	resizeThresholdRenderingInfo.layerCount = 1;
	resizeThresholdRenderingInfo.viewMask = 0;
	resizeThresholdRenderingInfo.colorAttachmentCount = 1;
	resizeThresholdRenderingInfo.pColorAttachments = &resizeThresholdAttachmentInfo;
	resizeThresholdRenderingInfo.pDepthAttachment = nullptr;
	resizeThresholdRenderingInfo.pStencilAttachment = nullptr;
	m_vkCmdBeginRenderingKHR(commandBuffer, &resizeThresholdRenderingInfo);

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_resizeThresholdGraphicsPipelineLayout, 0, 1, &m_resizeThresholdDescriptorSet, 0, nullptr);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_resizeThresholdGraphicsPipeline);
	vkCmdSetViewport(commandBuffer, 0, 1, &m_downscaledViewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &m_downscaledScissor);

	vkCmdDraw(commandBuffer, 3, 1, 0, 0);

	m_vkCmdEndRenderingKHR(commandBuffer);

	VkImageMemoryBarrier2 resizeThresholdBloomImageMemoryBarrier = {};
	resizeThresholdBloomImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	resizeThresholdBloomImageMemoryBarrier.pNext = nullptr;
	resizeThresholdBloomImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	resizeThresholdBloomImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	resizeThresholdBloomImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	resizeThresholdBloomImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	resizeThresholdBloomImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	resizeThresholdBloomImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	resizeThresholdBloomImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	resizeThresholdBloomImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	resizeThresholdBloomImageMemoryBarrier.image = m_bloomImage.handle;
	resizeThresholdBloomImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	resizeThresholdBloomImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	resizeThresholdBloomImageMemoryBarrier.subresourceRange.levelCount = 1;
	resizeThresholdBloomImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	resizeThresholdBloomImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkImageMemoryBarrier2 resizeThresholdDrawImageMemoryBarrier = {};
	resizeThresholdDrawImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	resizeThresholdDrawImageMemoryBarrier.pNext = nullptr;
	resizeThresholdDrawImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	resizeThresholdDrawImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	resizeThresholdDrawImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	resizeThresholdDrawImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	resizeThresholdDrawImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	resizeThresholdDrawImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	resizeThresholdDrawImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	resizeThresholdDrawImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	resizeThresholdDrawImageMemoryBarrier.image = drawImage;
	resizeThresholdDrawImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	resizeThresholdDrawImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	resizeThresholdDrawImageMemoryBarrier.subresourceRange.levelCount = 1;
	resizeThresholdDrawImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	resizeThresholdDrawImageMemoryBarrier.subresourceRange.layerCount = 1;

	std::array<VkImageMemoryBarrier2, 2> resizeThresholdImageMemoryBarriers = { resizeThresholdBloomImageMemoryBarrier, resizeThresholdDrawImageMemoryBarrier };
	VkDependencyInfo resizeThresholdDependencyInfo = {};
	resizeThresholdDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	resizeThresholdDependencyInfo.pNext = nullptr;
	resizeThresholdDependencyInfo.dependencyFlags = 0;
	resizeThresholdDependencyInfo.memoryBarrierCount = 0;
	resizeThresholdDependencyInfo.pMemoryBarriers = nullptr;
	resizeThresholdDependencyInfo.bufferMemoryBarrierCount = 0;
	resizeThresholdDependencyInfo.pBufferMemoryBarriers = nullptr;
	resizeThresholdDependencyInfo.imageMemoryBarrierCount = static_cast<uint32_t>(resizeThresholdImageMemoryBarriers.size());
	resizeThresholdDependencyInfo.pImageMemoryBarriers = resizeThresholdImageMemoryBarriers.data();
	m_vkCmdPipelineBarrier2KHR(commandBuffer, &resizeThresholdDependencyInfo);

	// Blur
	std::array<uint32_t, 2> pushConstants;
	for (uint32_t mipLevel = 0; mipLevel < m_mipLevels; mipLevel++) {
		VkViewport mipLevelViewport = m_viewport;
		mipLevelViewport.width = static_cast<float>(m_mipSizes[mipLevel].first);
		mipLevelViewport.height = static_cast<float>(m_mipSizes[mipLevel].second);
		VkRect2D mipLevelScissor = m_scissor;
		mipLevelScissor.extent.width = m_mipSizes[mipLevel].first;
		mipLevelScissor.extent.height = m_mipSizes[mipLevel].second;

		// Horizontal
		VkRenderingAttachmentInfo blurMipAttachmentInfo = {};
		blurMipAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		blurMipAttachmentInfo.pNext = nullptr;
		blurMipAttachmentInfo.imageView = m_blurImage.layerMipViews[mipLevel];
		blurMipAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		blurMipAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
		blurMipAttachmentInfo.resolveImageView = VK_NULL_HANDLE;
		blurMipAttachmentInfo.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		blurMipAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		blurMipAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		blurMipAttachmentInfo.clearValue.color = { 0.0f, 0.0f, 0.0f, 0.0f };

		VkRenderingInfo blurHorizontalRenderingInfo = {};
		blurHorizontalRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		blurHorizontalRenderingInfo.pNext = nullptr;
		blurHorizontalRenderingInfo.flags = 0;
		blurHorizontalRenderingInfo.renderArea = mipLevelScissor;
		blurHorizontalRenderingInfo.layerCount = 1;
		blurHorizontalRenderingInfo.viewMask = 0;
		blurHorizontalRenderingInfo.colorAttachmentCount = 1;
		blurHorizontalRenderingInfo.pColorAttachments = &blurMipAttachmentInfo;
		blurHorizontalRenderingInfo.pDepthAttachment = nullptr;
		blurHorizontalRenderingInfo.pStencilAttachment = nullptr;
		m_vkCmdBeginRenderingKHR(commandBuffer, &blurHorizontalRenderingInfo);

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_blurGraphicsPipelineLayout, 0, 1, &m_blurDescriptorSets[mipLevel], 0, nullptr);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_blurGraphicsPipeline);
		vkCmdSetViewport(commandBuffer, 0, 1, &mipLevelViewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &mipLevelScissor);

		if (mipLevel == 0) {
			pushConstants[1] = 0;
		}
		else {
			pushConstants[1] = 1;
		}

		pushConstants[0] = 1;
		vkCmdPushConstants(commandBuffer, m_blurGraphicsPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(uint32_t) * 2, pushConstants.data());

		vkCmdDraw(commandBuffer, 3, 1, 0, 0);

		m_vkCmdEndRenderingKHR(commandBuffer);

		std::vector<VkImageMemoryBarrier2> blurHorizontalImageMemoryBarriers;

		VkImageMemoryBarrier2 blurHorizontalBlurImageMemoryBarrier = {};
		blurHorizontalBlurImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		blurHorizontalBlurImageMemoryBarrier.pNext = nullptr;
		blurHorizontalBlurImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		blurHorizontalBlurImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		blurHorizontalBlurImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		blurHorizontalBlurImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
		blurHorizontalBlurImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		blurHorizontalBlurImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		blurHorizontalBlurImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
		blurHorizontalBlurImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
		blurHorizontalBlurImageMemoryBarrier.image = m_blurImage.handle;
		blurHorizontalBlurImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blurHorizontalBlurImageMemoryBarrier.subresourceRange.baseMipLevel = mipLevel;
		blurHorizontalBlurImageMemoryBarrier.subresourceRange.levelCount = 1;
		blurHorizontalBlurImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
		blurHorizontalBlurImageMemoryBarrier.subresourceRange.layerCount = 1;
		blurHorizontalImageMemoryBarriers.push_back(blurHorizontalBlurImageMemoryBarrier);

		if (mipLevel == 0) {
			VkImageMemoryBarrier2 blurHorizontalBloomImageMemoryBarrier = {};
			blurHorizontalBloomImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
			blurHorizontalBloomImageMemoryBarrier.pNext = nullptr;
			blurHorizontalBloomImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
			blurHorizontalBloomImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
			blurHorizontalBloomImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
			blurHorizontalBloomImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
			blurHorizontalBloomImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			blurHorizontalBloomImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			blurHorizontalBloomImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
			blurHorizontalBloomImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
			blurHorizontalBloomImageMemoryBarrier.image = m_bloomImage.handle;
			blurHorizontalBloomImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blurHorizontalBloomImageMemoryBarrier.subresourceRange.baseMipLevel = mipLevel;
			blurHorizontalBloomImageMemoryBarrier.subresourceRange.levelCount = 1;
			blurHorizontalBloomImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
			blurHorizontalBloomImageMemoryBarrier.subresourceRange.layerCount = 1;
			blurHorizontalImageMemoryBarriers.push_back(blurHorizontalBloomImageMemoryBarrier);
		}

		VkDependencyInfo blurHorizontalDependencyInfo = {};
		blurHorizontalDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		blurHorizontalDependencyInfo.pNext = nullptr;
		blurHorizontalDependencyInfo.dependencyFlags = 0;
		blurHorizontalDependencyInfo.memoryBarrierCount = 0;
		blurHorizontalDependencyInfo.pMemoryBarriers = nullptr;
		blurHorizontalDependencyInfo.bufferMemoryBarrierCount = 0;
		blurHorizontalDependencyInfo.pBufferMemoryBarriers = nullptr;
		blurHorizontalDependencyInfo.imageMemoryBarrierCount = static_cast<uint32_t>(blurHorizontalImageMemoryBarriers.size());
		blurHorizontalDependencyInfo.pImageMemoryBarriers = blurHorizontalImageMemoryBarriers.data();
		m_vkCmdPipelineBarrier2KHR(commandBuffer, &blurHorizontalDependencyInfo);

		// Vertical
		VkRenderingAttachmentInfo bloomMipAttachmentInfo = {};
		bloomMipAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		bloomMipAttachmentInfo.pNext = nullptr;
		bloomMipAttachmentInfo.imageView = m_bloomImage.layerMipViews[mipLevel];
		bloomMipAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		bloomMipAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
		bloomMipAttachmentInfo.resolveImageView = VK_NULL_HANDLE;
		bloomMipAttachmentInfo.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		bloomMipAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		bloomMipAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		bloomMipAttachmentInfo.clearValue.color = { 0.0f, 0.0f, 0.0f, 0.0f };

		VkRenderingInfo blurVerticalRenderingInfo = {};
		blurVerticalRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		blurVerticalRenderingInfo.pNext = nullptr;
		blurVerticalRenderingInfo.flags = 0;
		blurVerticalRenderingInfo.renderArea = mipLevelScissor;
		blurVerticalRenderingInfo.layerCount = 1;
		blurVerticalRenderingInfo.viewMask = 0;
		blurVerticalRenderingInfo.colorAttachmentCount = 1;
		blurVerticalRenderingInfo.pColorAttachments = &bloomMipAttachmentInfo;
		blurVerticalRenderingInfo.pDepthAttachment = nullptr;
		blurVerticalRenderingInfo.pStencilAttachment = nullptr;
		m_vkCmdBeginRenderingKHR(commandBuffer, &blurVerticalRenderingInfo);

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_blurGraphicsPipelineLayout, 0, 1, &m_blurBackDescriptorSets[mipLevel], 0, nullptr);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_blurGraphicsPipeline);
		vkCmdSetViewport(commandBuffer, 0, 1, &mipLevelViewport);
		vkCmdSetScissor(commandBuffer, 0, 1, &mipLevelScissor);

		pushConstants[0] = 0;
		vkCmdPushConstants(commandBuffer, m_blurGraphicsPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(uint32_t) * 2, pushConstants.data());

		vkCmdDraw(commandBuffer, 3, 1, 0, 0);

		m_vkCmdEndRenderingKHR(commandBuffer);

		VkImageMemoryBarrier2 blurVerticalBloomImageMemoryBarrier = {};
		blurVerticalBloomImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		blurVerticalBloomImageMemoryBarrier.pNext = nullptr;
		blurVerticalBloomImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		blurVerticalBloomImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		blurVerticalBloomImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		blurVerticalBloomImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
		blurVerticalBloomImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		blurVerticalBloomImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		blurVerticalBloomImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
		blurVerticalBloomImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
		blurVerticalBloomImageMemoryBarrier.image = m_bloomImage.handle;
		blurVerticalBloomImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blurVerticalBloomImageMemoryBarrier.subresourceRange.baseMipLevel = mipLevel;
		blurVerticalBloomImageMemoryBarrier.subresourceRange.levelCount = 1;
		blurVerticalBloomImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
		blurVerticalBloomImageMemoryBarrier.subresourceRange.layerCount = 1;

		VkImageMemoryBarrier2 blurVerticalBlurImageMemoryBarrier = {};
		blurVerticalBlurImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
		blurVerticalBlurImageMemoryBarrier.pNext = nullptr;
		blurVerticalBlurImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
		blurVerticalBlurImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
		blurVerticalBlurImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
		blurVerticalBlurImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		blurVerticalBlurImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		blurVerticalBlurImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		blurVerticalBlurImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
		blurVerticalBlurImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
		blurVerticalBlurImageMemoryBarrier.image = m_blurImage.handle;
		blurVerticalBlurImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blurVerticalBlurImageMemoryBarrier.subresourceRange.baseMipLevel = mipLevel;
		blurVerticalBlurImageMemoryBarrier.subresourceRange.levelCount = 1;
		blurVerticalBlurImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
		blurVerticalBlurImageMemoryBarrier.subresourceRange.layerCount = 1;

		std::array<VkImageMemoryBarrier2, 2> blurVerticalImageMemoryBarriers = { blurVerticalBloomImageMemoryBarrier, blurVerticalBlurImageMemoryBarrier };
		VkDependencyInfo blurVerticalDependencyInfo = {};
		blurVerticalDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		blurVerticalDependencyInfo.pNext = nullptr;
		blurVerticalDependencyInfo.dependencyFlags = 0;
		blurVerticalDependencyInfo.memoryBarrierCount = 0;
		blurVerticalDependencyInfo.pMemoryBarriers = nullptr;
		blurVerticalDependencyInfo.bufferMemoryBarrierCount = 0;
		blurVerticalDependencyInfo.pBufferMemoryBarriers = nullptr;
		blurVerticalDependencyInfo.imageMemoryBarrierCount = static_cast<uint32_t>(blurVerticalImageMemoryBarriers.size());
		blurVerticalDependencyInfo.pImageMemoryBarriers = blurVerticalImageMemoryBarriers.data();
		m_vkCmdPipelineBarrier2KHR(commandBuffer, &blurVerticalDependencyInfo);
	}

	// Bloom
	VkRenderingAttachmentInfo drawImageAttachmentInfo = {};
	drawImageAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	drawImageAttachmentInfo.pNext = nullptr;
	drawImageAttachmentInfo.imageView = drawImageView;
	drawImageAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	drawImageAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
	drawImageAttachmentInfo.resolveImageView = VK_NULL_HANDLE;
	drawImageAttachmentInfo.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	drawImageAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	drawImageAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	drawImageAttachmentInfo.clearValue.color = { 0.0f, 0.0f, 0.0f, 0.0f };

	VkRenderingInfo bloomRenderingInfo = {};
	bloomRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	bloomRenderingInfo.pNext = nullptr;
	bloomRenderingInfo.flags = 0;
	bloomRenderingInfo.renderArea = m_scissor;
	bloomRenderingInfo.layerCount = 1;
	bloomRenderingInfo.viewMask = 0;
	bloomRenderingInfo.colorAttachmentCount = 1;
	bloomRenderingInfo.pColorAttachments = &drawImageAttachmentInfo;
	bloomRenderingInfo.pDepthAttachment = nullptr;
	bloomRenderingInfo.pStencilAttachment = nullptr;
	m_vkCmdBeginRenderingKHR(commandBuffer, &bloomRenderingInfo);

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_bloomGraphicsPipelineLayout, 0, 1, &m_bloomDescriptorSet, 0, nullptr);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_bloomGraphicsPipeline);
	vkCmdSetViewport(commandBuffer, 0, 1, &m_viewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &m_scissor);

	vkCmdDraw(commandBuffer, 3, 1, 0, 0);

	m_vkCmdEndRenderingKHR(commandBuffer);

	// Compositing layout transition VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL -> VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	VkImageMemoryBarrier2 compositingColorAttachmentToFragmentImageMemoryBarrier = {};
	compositingColorAttachmentToFragmentImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	compositingColorAttachmentToFragmentImageMemoryBarrier.pNext = nullptr;
	compositingColorAttachmentToFragmentImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	compositingColorAttachmentToFragmentImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	compositingColorAttachmentToFragmentImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	compositingColorAttachmentToFragmentImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	compositingColorAttachmentToFragmentImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	compositingColorAttachmentToFragmentImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	compositingColorAttachmentToFragmentImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	compositingColorAttachmentToFragmentImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	compositingColorAttachmentToFragmentImageMemoryBarrier.image = drawImage;
	compositingColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	compositingColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	compositingColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.levelCount = 1;
	compositingColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	compositingColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkDependencyInfo compositingDependencyInfo = {};
	compositingDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	compositingDependencyInfo.pNext = nullptr;
	compositingDependencyInfo.dependencyFlags = 0;
	compositingDependencyInfo.memoryBarrierCount = 0;
	compositingDependencyInfo.pMemoryBarriers = nullptr;
	compositingDependencyInfo.bufferMemoryBarrierCount = 0;
	compositingDependencyInfo.pBufferMemoryBarriers = nullptr;
	compositingDependencyInfo.imageMemoryBarrierCount = 1;
	compositingDependencyInfo.pImageMemoryBarriers = &compositingColorAttachmentToFragmentImageMemoryBarrier;
	m_vkCmdPipelineBarrier2KHR(commandBuffer, &compositingDependencyInfo);
}

void Bloom::onResize(uint32_t width,
	uint32_t height,
	VkImageView drawImageView) {
	m_viewport.width = static_cast<float>(width);
	m_viewport.height = static_cast<float>(height);
	m_scissor.extent.width = width;
	m_scissor.extent.height = height;
	m_downscaledViewport.width = std::max(static_cast<float>(width) / static_cast<float>(BLOOM_DOWNSCALE), 1.0f);
	m_downscaledViewport.height = std::max(static_cast<float>(height) / static_cast<float>(BLOOM_DOWNSCALE), 1.0f);
	m_downscaledScissor.extent.width = static_cast<uint32_t>(m_downscaledViewport.width);
	m_downscaledScissor.extent.height = static_cast<uint32_t>(m_downscaledViewport.height);

	destroyImages();
	createImages(m_downscaledScissor.extent.width, m_downscaledScissor.extent.height);

	// Destroy blur descriptor pool
	vkDestroyDescriptorPool(m_device, m_blurDescriptorPool, nullptr);
	createBlurDescriptorSets();

	updateDescriptorSets(drawImageView);
}

void Bloom::createImages(uint32_t width, uint32_t height) {
	VkExtent3D imageExtent;
	imageExtent.width = width;
	imageExtent.height = height;
	imageExtent.depth = 1;

	m_mipLevels = std::min<uint32_t>(static_cast<uint32_t>(std::floor(std::log2(std::min(width, height)))) + 1, 5);

	m_mipSizes.resize(m_mipLevels);
	for (uint32_t mipLevel = 0; mipLevel < m_mipLevels; mipLevel++) {
		m_mipSizes[mipLevel] = { static_cast<uint32_t>(static_cast<float>(width) * static_cast<float>(std::pow(0.5f, mipLevel))), static_cast<uint32_t>(static_cast<float>(height) * static_cast<float>(std::pow(0.5f, mipLevel))) };
	}

	VkImageCreateInfo bloomBlurImageCreateInfo = {};
	bloomBlurImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	bloomBlurImageCreateInfo.pNext = nullptr;
	bloomBlurImageCreateInfo.flags = 0;
	bloomBlurImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	bloomBlurImageCreateInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	bloomBlurImageCreateInfo.extent = imageExtent;
	bloomBlurImageCreateInfo.mipLevels = m_mipLevels;
	bloomBlurImageCreateInfo.arrayLayers = 1;
	bloomBlurImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	bloomBlurImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	bloomBlurImageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	bloomBlurImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bloomBlurImageCreateInfo.queueFamilyIndexCount = 1;
	bloomBlurImageCreateInfo.pQueueFamilyIndices = &m_graphicsQueueFamilyIndex;
	bloomBlurImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo bloomBlurImageAllocationCreateInfo = {};
	bloomBlurImageAllocationCreateInfo.flags = 0;
	bloomBlurImageAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	NTSHENGN_VK_CHECK(vmaCreateImage(m_allocator, &bloomBlurImageCreateInfo, &bloomBlurImageAllocationCreateInfo, &m_bloomImage.handle, &m_bloomImage.allocation, nullptr));
	NTSHENGN_VK_CHECK(vmaCreateImage(m_allocator, &bloomBlurImageCreateInfo, &bloomBlurImageAllocationCreateInfo, &m_blurImage.handle, &m_blurImage.allocation, nullptr));

	VkImageViewCreateInfo bloomBlurImageViewCreateInfo = {};
	bloomBlurImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	bloomBlurImageViewCreateInfo.pNext = nullptr;
	bloomBlurImageViewCreateInfo.flags = 0;
	bloomBlurImageViewCreateInfo.image = m_bloomImage.handle;
	bloomBlurImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	bloomBlurImageViewCreateInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	bloomBlurImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
	bloomBlurImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
	bloomBlurImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
	bloomBlurImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
	bloomBlurImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	bloomBlurImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	bloomBlurImageViewCreateInfo.subresourceRange.levelCount = m_mipLevels;
	bloomBlurImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	bloomBlurImageViewCreateInfo.subresourceRange.layerCount = 1;
	NTSHENGN_VK_CHECK(vkCreateImageView(m_device, &bloomBlurImageViewCreateInfo, nullptr, &m_bloomImage.view));

	m_bloomImage.layerMipViews.resize(m_mipLevels);
	m_blurImage.layerMipViews.resize(m_mipLevels);
	bloomBlurImageViewCreateInfo.subresourceRange.levelCount = 1;
	for (uint32_t mipLevel = 0; mipLevel < m_mipLevels; mipLevel++) {
		bloomBlurImageViewCreateInfo.subresourceRange.baseMipLevel = mipLevel;

		bloomBlurImageViewCreateInfo.image = m_bloomImage.handle;
		NTSHENGN_VK_CHECK(vkCreateImageView(m_device, &bloomBlurImageViewCreateInfo, nullptr, &m_bloomImage.layerMipViews[mipLevel]));
		bloomBlurImageViewCreateInfo.image = m_blurImage.handle;
		NTSHENGN_VK_CHECK(vkCreateImageView(m_device, &bloomBlurImageViewCreateInfo, nullptr, &m_blurImage.layerMipViews[mipLevel]));
	}

	// Layout transition VK_IMAGE_LAYOUT_UNDEFINED -> VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL
	NTSHENGN_VK_CHECK(vkResetCommandPool(m_device, m_initializationCommandPool, 0));

	VkCommandBufferBeginInfo commandBufferBeginInfo = {};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.pNext = nullptr;
	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	commandBufferBeginInfo.pInheritanceInfo = nullptr;
	NTSHENGN_VK_CHECK(vkBeginCommandBuffer(m_initializationCommandBuffer, &commandBufferBeginInfo));

	VkImageMemoryBarrier2 bloomImageMemoryBarrier = {};
	bloomImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	bloomImageMemoryBarrier.pNext = nullptr;
	bloomImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
	bloomImageMemoryBarrier.srcAccessMask = 0;
	bloomImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	bloomImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	bloomImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	bloomImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	bloomImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	bloomImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	bloomImageMemoryBarrier.image = m_bloomImage.handle;
	bloomImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	bloomImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	bloomImageMemoryBarrier.subresourceRange.levelCount = m_mipLevels;
	bloomImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	bloomImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkImageMemoryBarrier2 blurImageMemoryBarrier = {};
	blurImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	blurImageMemoryBarrier.pNext = nullptr;
	blurImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
	blurImageMemoryBarrier.srcAccessMask = 0;
	blurImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	blurImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	blurImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	blurImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	blurImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	blurImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	blurImageMemoryBarrier.image = m_blurImage.handle;
	blurImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	blurImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	blurImageMemoryBarrier.subresourceRange.levelCount = m_mipLevels;
	blurImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	blurImageMemoryBarrier.subresourceRange.layerCount = 1;

	std::array<VkImageMemoryBarrier2, 2> imageMemoryBarriers = { bloomImageMemoryBarrier, blurImageMemoryBarrier };
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
	m_vkCmdPipelineBarrier2KHR(m_initializationCommandBuffer, &dependencyInfo);

	NTSHENGN_VK_CHECK(vkEndCommandBuffer(m_initializationCommandBuffer));

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.waitSemaphoreCount = 0;
	submitInfo.pWaitSemaphores = nullptr;
	submitInfo.pWaitDstStageMask = nullptr;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_initializationCommandBuffer;
	submitInfo.signalSemaphoreCount = 0;
	submitInfo.pSignalSemaphores = nullptr;
	NTSHENGN_VK_CHECK(vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_initializationFence));
	NTSHENGN_VK_CHECK(vkWaitForFences(m_device, 1, &m_initializationFence, VK_TRUE, std::numeric_limits<uint64_t>::max()));
	NTSHENGN_VK_CHECK(vkResetFences(m_device, 1, &m_initializationFence));
}

void Bloom::destroyImages() {
	m_blurImage.destroy(m_device, m_allocator);
	m_bloomImage.destroy(m_device, m_allocator);
}

void Bloom::createDescriptorSetLayouts() {
	createResizeThresholdDescriptorSetLayout();
	createBlurDescriptorSetLayout();
	createBloomDescriptorSetLayout();
}

void Bloom::createResizeThresholdDescriptorSetLayout() {
	VkDescriptorSetLayoutBinding drawImageDescriptorSetLayoutBinding = {};
	drawImageDescriptorSetLayoutBinding.binding = 0;
	drawImageDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	drawImageDescriptorSetLayoutBinding.descriptorCount = 1;
	drawImageDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	drawImageDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.pNext = nullptr;
	descriptorSetLayoutCreateInfo.flags = 0;
	descriptorSetLayoutCreateInfo.bindingCount = 1;
	descriptorSetLayoutCreateInfo.pBindings = &drawImageDescriptorSetLayoutBinding;
	NTSHENGN_VK_CHECK(vkCreateDescriptorSetLayout(m_device, &descriptorSetLayoutCreateInfo, nullptr, &m_resizeThresholdDescriptorSetLayout));
}

void Bloom::createBlurDescriptorSetLayout() {
	VkDescriptorSetLayoutBinding bloomImageDescriptorSetLayoutBinding = {};
	bloomImageDescriptorSetLayoutBinding.binding = 0;
	bloomImageDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bloomImageDescriptorSetLayoutBinding.descriptorCount = 1;
	bloomImageDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bloomImageDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.pNext = nullptr;
	descriptorSetLayoutCreateInfo.flags = 0;
	descriptorSetLayoutCreateInfo.bindingCount = 1;
	descriptorSetLayoutCreateInfo.pBindings = &bloomImageDescriptorSetLayoutBinding;
	NTSHENGN_VK_CHECK(vkCreateDescriptorSetLayout(m_device, &descriptorSetLayoutCreateInfo, nullptr, &m_blurDescriptorSetLayout));
}

void Bloom::createBloomDescriptorSetLayout() {
	VkDescriptorSetLayoutBinding bloomImageDescriptorSetLayoutBinding = {};
	bloomImageDescriptorSetLayoutBinding.binding = 0;
	bloomImageDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bloomImageDescriptorSetLayoutBinding.descriptorCount = 1;
	bloomImageDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	bloomImageDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.pNext = nullptr;
	descriptorSetLayoutCreateInfo.flags = 0;
	descriptorSetLayoutCreateInfo.bindingCount = 1;
	descriptorSetLayoutCreateInfo.pBindings = &bloomImageDescriptorSetLayoutBinding;
	NTSHENGN_VK_CHECK(vkCreateDescriptorSetLayout(m_device, &descriptorSetLayoutCreateInfo, nullptr, &m_bloomDescriptorSetLayout));
}

void Bloom::createGraphicsPipelines(VkFormat drawImageFormat) {
	createResizeThresholdGraphicsPipeline(drawImageFormat);
	createBlurGraphicsPipeline(drawImageFormat);
	createBloomGraphicsPipeline(drawImageFormat);
}

void Bloom::createResizeThresholdGraphicsPipeline(VkFormat drawImageFormat) {
	VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {};
	pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	pipelineRenderingCreateInfo.pNext = nullptr;
	pipelineRenderingCreateInfo.viewMask = 0;
	pipelineRenderingCreateInfo.colorAttachmentCount = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &drawImageFormat;
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

	const std::string fragmentShaderCode = R"GLSL(
		#version 460

		layout(set = 0, binding = 0) uniform sampler2D imageSampler;

		layout(location = 0) in vec2 uv;

		layout(location = 0) out vec4 outColor;

		void main() {
			vec2 texelSize = 1.0 / vec2(textureSize(imageSampler, 0));

			outColor = texture(imageSampler, uv + (vec2(-1.0, -1.0) * texelSize)) +
				texture(imageSampler, uv + (vec2(1.0, -1.0) * texelSize)) +
				texture(imageSampler, uv + (vec2(-1.0, 1.0) * texelSize)) +
				texture(imageSampler, uv + (vec2(1.0, 1.0) * texelSize));
			outColor /= 4.0;
			outColor = min(outColor, 10.0);
			outColor = vec4(max(outColor.rgb - 1.0, 0.0), 0.0);
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
	viewportStateCreateInfo.pViewports = &m_downscaledViewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &m_downscaledScissor;

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
	pipelineLayoutCreateInfo.pSetLayouts = &m_resizeThresholdDescriptorSetLayout;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;
	NTSHENGN_VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_resizeThresholdGraphicsPipelineLayout));

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
	graphicsPipelineCreateInfo.layout = m_resizeThresholdGraphicsPipelineLayout;
	graphicsPipelineCreateInfo.renderPass = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.subpass = 0;
	graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.basePipelineIndex = 0;
	NTSHENGN_VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &m_resizeThresholdGraphicsPipeline));

	vkDestroyShaderModule(m_device, vertexShaderModule, nullptr);
	vkDestroyShaderModule(m_device, fragmentShaderModule, nullptr);
}

void Bloom::createBlurGraphicsPipeline(VkFormat drawImageFormat) {
	VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {};
	pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	pipelineRenderingCreateInfo.pNext = nullptr;
	pipelineRenderingCreateInfo.viewMask = 0;
	pipelineRenderingCreateInfo.colorAttachmentCount = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &drawImageFormat;
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

	const std::string fragmentShaderCode = R"GLSL(
		#version 460

		layout(set = 0, binding = 0) uniform sampler2D bloomSampler;

		layout(push_constant) uniform PushConstants {
			uint horizontalBlur;
			uint doubleTexelSize;
		} pC;

		layout(location = 0) in vec2 uv;

		layout(location = 0) out vec4 outColor;

		const float weights_33[33] = float[] (
			0.004013,
			0.005554,
			0.007527,
			0.00999,
			0.012984,
			0.016524,
			0.020594,
			0.025133,
			0.030036,
			0.035151,
			0.040283,
			0.045207,
			0.049681,
			0.053463,
			0.056341,
			0.058141,
			0.058754,
			0.058141,
			0.056341,
			0.053463,
			0.049681,
			0.045207,
			0.040283,
			0.035151,
			0.030036,
			0.025133,
			0.020594,
			0.016524,
			0.012984,
			0.00999,
			0.007527,
			0.005554,
			0.004013
		);

		void main() {
			vec2 texelSize = 1.0 / vec2(textureSize(bloomSampler, 0));

			vec4 color = vec4(0.0);

			for (int i = -16; i <= 16; i++) {
				vec2 uvOffset;
				if (pC.horizontalBlur == 1) {
					if (pC.doubleTexelSize == 1) {
						uvOffset = vec2((texelSize.x * 2.0) * float(i), 0.0);
					}
					else {
						uvOffset = vec2((texelSize.x) * float(i), 0.0);
					}
				}
				else {
					uvOffset = vec2(0.0, texelSize.y * float(i));
				}
				color += texture(bloomSampler, uv + uvOffset) * weights_33[i + 16];
			}

			outColor = color;
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

	VkPushConstantRange pushConstantRange = {};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(uint32_t) * 2;

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.pNext = nullptr;
	pipelineLayoutCreateInfo.flags = 0;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &m_blurDescriptorSetLayout;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
	NTSHENGN_VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_blurGraphicsPipelineLayout));

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
	graphicsPipelineCreateInfo.layout = m_blurGraphicsPipelineLayout;
	graphicsPipelineCreateInfo.renderPass = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.subpass = 0;
	graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.basePipelineIndex = 0;
	NTSHENGN_VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &m_blurGraphicsPipeline));

	vkDestroyShaderModule(m_device, vertexShaderModule, nullptr);
	vkDestroyShaderModule(m_device, fragmentShaderModule, nullptr);
}

void Bloom::createBloomGraphicsPipeline(VkFormat drawImageFormat) {
	VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {};
	pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	pipelineRenderingCreateInfo.pNext = nullptr;
	pipelineRenderingCreateInfo.viewMask = 0;
	pipelineRenderingCreateInfo.colorAttachmentCount = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &drawImageFormat;
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

	const std::string fragmentShaderCode = R"GLSL(
		#version 460

		layout(set = 0, binding = 0) uniform sampler2D bloomSampler;

		layout(location = 0) in vec2 uv;

		layout(location = 0) out vec4 outColor;

		void main() {
			outColor = textureLod(bloomSampler, uv, 1.5) +
				textureLod(bloomSampler, uv, 3.5) +
				textureLod(bloomSampler, uv, 4.5);
			outColor /= 3.0;
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
	colorBlendAttachmentState.blendEnable = VK_TRUE;
	colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
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
	pipelineLayoutCreateInfo.pSetLayouts = &m_bloomDescriptorSetLayout;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;
	NTSHENGN_VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_bloomGraphicsPipelineLayout));

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
	graphicsPipelineCreateInfo.layout = m_bloomGraphicsPipelineLayout;
	graphicsPipelineCreateInfo.renderPass = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.subpass = 0;
	graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.basePipelineIndex = 0;
	NTSHENGN_VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &m_bloomGraphicsPipeline));

	vkDestroyShaderModule(m_device, vertexShaderModule, nullptr);
	vkDestroyShaderModule(m_device, fragmentShaderModule, nullptr);
}

void Bloom::createDescriptorSets() {
	createResizeThresholdDescriptorSet();
	createBlurDescriptorSets();
	createBloomDescriptorSet();
}

void Bloom::createResizeThresholdDescriptorSet() {
	// Create descriptor pool
	VkDescriptorPoolSize drawImageDescriptorPoolSize = {};
	drawImageDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	drawImageDescriptorPoolSize.descriptorCount = 1;

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext = nullptr;
	descriptorPoolCreateInfo.flags = 0;
	descriptorPoolCreateInfo.maxSets = 1;
	descriptorPoolCreateInfo.poolSizeCount = 1;
	descriptorPoolCreateInfo.pPoolSizes = &drawImageDescriptorPoolSize;
	NTSHENGN_VK_CHECK(vkCreateDescriptorPool(m_device, &descriptorPoolCreateInfo, nullptr, &m_resizeThresholdDescriptorPool));

	// Allocate descriptor set
	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
	descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetAllocateInfo.pNext = nullptr;
	descriptorSetAllocateInfo.descriptorPool = m_resizeThresholdDescriptorPool;
	descriptorSetAllocateInfo.descriptorSetCount = 1;
	descriptorSetAllocateInfo.pSetLayouts = &m_resizeThresholdDescriptorSetLayout;
	NTSHENGN_VK_CHECK(vkAllocateDescriptorSets(m_device, &descriptorSetAllocateInfo, &m_resizeThresholdDescriptorSet));
}

void Bloom::createBlurDescriptorSets() {
	// Create descriptor pool
	VkDescriptorPoolSize bloomImageDescriptorPoolSize = {};
	bloomImageDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bloomImageDescriptorPoolSize.descriptorCount = m_mipLevels * 2;

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext = nullptr;
	descriptorPoolCreateInfo.flags = 0;
	descriptorPoolCreateInfo.maxSets = m_mipLevels * 2;
	descriptorPoolCreateInfo.poolSizeCount = 1;
	descriptorPoolCreateInfo.pPoolSizes = &bloomImageDescriptorPoolSize;
	NTSHENGN_VK_CHECK(vkCreateDescriptorPool(m_device, &descriptorPoolCreateInfo, nullptr, &m_blurDescriptorPool));

	// Allocate descriptor sets
	m_blurDescriptorSets.resize(m_mipLevels);
	m_blurBackDescriptorSets.resize(m_mipLevels);
	for (uint32_t mipLevel = 0; mipLevel < m_mipLevels; mipLevel++) {
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
		descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocateInfo.pNext = nullptr;
		descriptorSetAllocateInfo.descriptorPool = m_blurDescriptorPool;
		descriptorSetAllocateInfo.descriptorSetCount = 1;
		descriptorSetAllocateInfo.pSetLayouts = &m_blurDescriptorSetLayout;
		NTSHENGN_VK_CHECK(vkAllocateDescriptorSets(m_device, &descriptorSetAllocateInfo, &m_blurDescriptorSets[mipLevel]));
		NTSHENGN_VK_CHECK(vkAllocateDescriptorSets(m_device, &descriptorSetAllocateInfo, &m_blurBackDescriptorSets[mipLevel]));
	}
}

void Bloom::createBloomDescriptorSet() {
	// Create descriptor pool
	VkDescriptorPoolSize bloomImageDescriptorPoolSize = {};
	bloomImageDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bloomImageDescriptorPoolSize.descriptorCount = 1;

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext = nullptr;
	descriptorPoolCreateInfo.flags = 0;
	descriptorPoolCreateInfo.maxSets = 1;
	descriptorPoolCreateInfo.poolSizeCount = 1;
	descriptorPoolCreateInfo.pPoolSizes = &bloomImageDescriptorPoolSize;
	NTSHENGN_VK_CHECK(vkCreateDescriptorPool(m_device, &descriptorPoolCreateInfo, nullptr, &m_bloomDescriptorPool));

	// Allocate descriptor set
	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
	descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetAllocateInfo.pNext = nullptr;
	descriptorSetAllocateInfo.descriptorPool = m_bloomDescriptorPool;
	descriptorSetAllocateInfo.descriptorSetCount = 1;
	descriptorSetAllocateInfo.pSetLayouts = &m_bloomDescriptorSetLayout;
	NTSHENGN_VK_CHECK(vkAllocateDescriptorSets(m_device, &descriptorSetAllocateInfo, &m_bloomDescriptorSet));
}

void Bloom::updateDescriptorSets(VkImageView drawImageView) {
	std::vector<VkWriteDescriptorSet> writeDescriptorSets;

	VkDescriptorImageInfo drawImageDescriptorImageInfo;
	drawImageDescriptorImageInfo.sampler = m_nearestSampler;
	drawImageDescriptorImageInfo.imageView = drawImageView;
	drawImageDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkWriteDescriptorSet drawImageDescriptorWriteDescriptorSet = {};
	drawImageDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	drawImageDescriptorWriteDescriptorSet.pNext = nullptr;
	drawImageDescriptorWriteDescriptorSet.dstSet = m_resizeThresholdDescriptorSet;
	drawImageDescriptorWriteDescriptorSet.dstBinding = 0;
	drawImageDescriptorWriteDescriptorSet.dstArrayElement = 0;
	drawImageDescriptorWriteDescriptorSet.descriptorCount = 1;
	drawImageDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	drawImageDescriptorWriteDescriptorSet.pImageInfo = &drawImageDescriptorImageInfo;
	drawImageDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
	drawImageDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;
	writeDescriptorSets.push_back(drawImageDescriptorWriteDescriptorSet);

	std::vector<VkDescriptorImageInfo> bloomMipImageDescriptorImageInfos(m_mipLevels);
	std::vector<VkDescriptorImageInfo> blurMipImageDescriptorImageInfos(m_mipLevels);
	for (uint32_t mipLevel = 0; mipLevel < m_mipLevels; mipLevel++) {
		bloomMipImageDescriptorImageInfos[mipLevel].sampler = m_nearestSampler;
		bloomMipImageDescriptorImageInfos[mipLevel].imageView = (mipLevel == 0) ? m_bloomImage.layerMipViews[0] : m_bloomImage.layerMipViews[mipLevel - 1];
		bloomMipImageDescriptorImageInfos[mipLevel].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet bloomMipImageDescriptorWriteDescriptorSet = {};
		bloomMipImageDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		bloomMipImageDescriptorWriteDescriptorSet.pNext = nullptr;
		bloomMipImageDescriptorWriteDescriptorSet.dstSet = m_blurDescriptorSets[mipLevel];
		bloomMipImageDescriptorWriteDescriptorSet.dstBinding = 0;
		bloomMipImageDescriptorWriteDescriptorSet.dstArrayElement = 0;
		bloomMipImageDescriptorWriteDescriptorSet.descriptorCount = 1;
		bloomMipImageDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bloomMipImageDescriptorWriteDescriptorSet.pImageInfo = &bloomMipImageDescriptorImageInfos[mipLevel];
		bloomMipImageDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
		bloomMipImageDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;
		writeDescriptorSets.push_back(bloomMipImageDescriptorWriteDescriptorSet);

		blurMipImageDescriptorImageInfos[mipLevel].sampler = m_nearestSampler;
		blurMipImageDescriptorImageInfos[mipLevel].imageView = m_blurImage.layerMipViews[mipLevel];
		blurMipImageDescriptorImageInfos[mipLevel].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet blurMipImageDescriptorWriteDescriptorSet = {};
		blurMipImageDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		blurMipImageDescriptorWriteDescriptorSet.pNext = nullptr;
		blurMipImageDescriptorWriteDescriptorSet.dstSet = m_blurBackDescriptorSets[mipLevel];
		blurMipImageDescriptorWriteDescriptorSet.dstBinding = 0;
		blurMipImageDescriptorWriteDescriptorSet.dstArrayElement = 0;
		blurMipImageDescriptorWriteDescriptorSet.descriptorCount = 1;
		blurMipImageDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		blurMipImageDescriptorWriteDescriptorSet.pImageInfo = &blurMipImageDescriptorImageInfos[mipLevel];
		blurMipImageDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
		blurMipImageDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;
		writeDescriptorSets.push_back(blurMipImageDescriptorWriteDescriptorSet);
	}

	VkDescriptorImageInfo bloomImageDescriptorImageInfo;
	bloomImageDescriptorImageInfo.sampler = m_linearSampler;
	bloomImageDescriptorImageInfo.imageView = m_bloomImage.view;
	bloomImageDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkWriteDescriptorSet bloomImageDescriptorWriteDescriptorSet = {};
	bloomImageDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	bloomImageDescriptorWriteDescriptorSet.pNext = nullptr;
	bloomImageDescriptorWriteDescriptorSet.dstSet = m_bloomDescriptorSet;
	bloomImageDescriptorWriteDescriptorSet.dstBinding = 0;
	bloomImageDescriptorWriteDescriptorSet.dstArrayElement = 0;
	bloomImageDescriptorWriteDescriptorSet.descriptorCount = 1;
	bloomImageDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	bloomImageDescriptorWriteDescriptorSet.pImageInfo = &bloomImageDescriptorImageInfo;
	bloomImageDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
	bloomImageDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;
	writeDescriptorSets.push_back(bloomImageDescriptorWriteDescriptorSet);

	vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
}
