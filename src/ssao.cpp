#include "ssao.h"
#include <random>

void SSAO::init(VkDevice device,
	VkQueue graphicsQueue,
	uint32_t graphicsQueueFamilyIndex,
	VmaAllocator allocator,
	VkCommandPool initializationCommandPool,
	VkCommandBuffer initializationCommandBuffer,
	VkFence initializationFence,
	VkImageView positionImageView,
	VkImageView normalImageView,
	VkViewport viewport,
	VkRect2D scissor,
	uint32_t framesInFlight,
	const std::vector<HostVisibleVulkanBuffer>& cameraBuffers,
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

	createImagesAndBuffer(m_scissor.extent.width, m_scissor.extent.height);
	createImageSamplers();
	createDescriptorSetLayouts();
	createGraphicsPipelines();
	createDescriptorSets(cameraBuffers);
	updateDescriptorSets(positionImageView, normalImageView);
}

void SSAO::destroy() {
	vkDestroyDescriptorPool(m_device, m_ssaoBlurDescriptorPool, nullptr);
	vkDestroyPipeline(m_device, m_ssaoBlurGraphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_ssaoBlurGraphicsPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(m_device, m_ssaoBlurDescriptorSetLayout, nullptr);

	vkDestroyDescriptorPool(m_device, m_ssaoDescriptorPool, nullptr);
	vkDestroyPipeline(m_device, m_ssaoGraphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_ssaoGraphicsPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(m_device, m_ssaoDescriptorSetLayout, nullptr);

	vkDestroySampler(m_device, m_repeatSampler, nullptr);
	vkDestroySampler(m_device, m_nearestSampler, nullptr);

	m_randomSampleBuffer.destroy(m_allocator);
	m_randomImage.destroy(m_device, m_allocator);
	destroySSAOImages();
}

void SSAO::draw(VkCommandBuffer commandBuffer, uint32_t currentFrameInFlight) {
	// Layout transitions
	VkImageMemoryBarrier2 ssaoFragmentToColorAttachmentImageMemoryBarrier = {};
	ssaoFragmentToColorAttachmentImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	ssaoFragmentToColorAttachmentImageMemoryBarrier.pNext = nullptr;
	ssaoFragmentToColorAttachmentImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	ssaoFragmentToColorAttachmentImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	ssaoFragmentToColorAttachmentImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	ssaoFragmentToColorAttachmentImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	ssaoFragmentToColorAttachmentImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	ssaoFragmentToColorAttachmentImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	ssaoFragmentToColorAttachmentImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	ssaoFragmentToColorAttachmentImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	ssaoFragmentToColorAttachmentImageMemoryBarrier.image = m_ssaoImage.handle;
	ssaoFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	ssaoFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	ssaoFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.levelCount = 1;
	ssaoFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	ssaoFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkImageMemoryBarrier2 ssaoBlurFragmentToColorAttachmentImageMemoryBarrier = {};
	ssaoBlurFragmentToColorAttachmentImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	ssaoBlurFragmentToColorAttachmentImageMemoryBarrier.pNext = nullptr;
	ssaoBlurFragmentToColorAttachmentImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	ssaoBlurFragmentToColorAttachmentImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	ssaoBlurFragmentToColorAttachmentImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	ssaoBlurFragmentToColorAttachmentImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	ssaoBlurFragmentToColorAttachmentImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	ssaoBlurFragmentToColorAttachmentImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	ssaoBlurFragmentToColorAttachmentImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	ssaoBlurFragmentToColorAttachmentImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	ssaoBlurFragmentToColorAttachmentImageMemoryBarrier.image = m_ssaoBlurImage.handle;
	ssaoBlurFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	ssaoBlurFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	ssaoBlurFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.levelCount = 1;
	ssaoBlurFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	ssaoBlurFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.layerCount = 1;

	std::array<VkImageMemoryBarrier2, 2> beforeRenderImageMemoryBarriers = { ssaoFragmentToColorAttachmentImageMemoryBarrier, ssaoBlurFragmentToColorAttachmentImageMemoryBarrier };
	VkDependencyInfo beforeRenderDependencyInfo = {};
	beforeRenderDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	beforeRenderDependencyInfo.pNext = nullptr;
	beforeRenderDependencyInfo.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
	beforeRenderDependencyInfo.memoryBarrierCount = 0;
	beforeRenderDependencyInfo.pMemoryBarriers = nullptr;
	beforeRenderDependencyInfo.bufferMemoryBarrierCount = 0;
	beforeRenderDependencyInfo.pBufferMemoryBarriers = nullptr;
	beforeRenderDependencyInfo.imageMemoryBarrierCount = static_cast<uint32_t>(beforeRenderImageMemoryBarriers.size());
	beforeRenderDependencyInfo.pImageMemoryBarriers = beforeRenderImageMemoryBarriers.data();
	m_vkCmdPipelineBarrier2KHR(commandBuffer, &beforeRenderDependencyInfo);

	// SSAO
	VkRenderingAttachmentInfo ssaoAttachmentInfo = {};
	ssaoAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	ssaoAttachmentInfo.pNext = nullptr;
	ssaoAttachmentInfo.imageView = m_ssaoImage.view;
	ssaoAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	ssaoAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
	ssaoAttachmentInfo.resolveImageView = VK_NULL_HANDLE;
	ssaoAttachmentInfo.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	ssaoAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	ssaoAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	ssaoAttachmentInfo.clearValue.color = { 0.0f, 0.0f, 0.0f, 0.0f };

	VkRenderingInfo ssaoRenderingInfo = {};
	ssaoRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	ssaoRenderingInfo.pNext = nullptr;
	ssaoRenderingInfo.flags = 0;
	ssaoRenderingInfo.renderArea = m_scissor;
	ssaoRenderingInfo.layerCount = 1;
	ssaoRenderingInfo.viewMask = 0;
	ssaoRenderingInfo.colorAttachmentCount = 1;
	ssaoRenderingInfo.pColorAttachments = &ssaoAttachmentInfo;
	ssaoRenderingInfo.pDepthAttachment = nullptr;
	ssaoRenderingInfo.pStencilAttachment = nullptr;
	m_vkCmdBeginRenderingKHR(commandBuffer, &ssaoRenderingInfo);

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ssaoGraphicsPipelineLayout, 0, 1, &m_ssaoDescriptorSets[currentFrameInFlight], 0, nullptr);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ssaoGraphicsPipeline);
	vkCmdSetViewport(commandBuffer, 0, 1, &m_viewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &m_scissor);

	vkCmdDraw(commandBuffer, 3, 1, 0, 0);

	m_vkCmdEndRenderingKHR(commandBuffer);

	VkImageMemoryBarrier2 ssaoColorAttachmentToFragmentImageMemoryBarrier = {};
	ssaoColorAttachmentToFragmentImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	ssaoColorAttachmentToFragmentImageMemoryBarrier.pNext = nullptr;
	ssaoColorAttachmentToFragmentImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	ssaoColorAttachmentToFragmentImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	ssaoColorAttachmentToFragmentImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	ssaoColorAttachmentToFragmentImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	ssaoColorAttachmentToFragmentImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	ssaoColorAttachmentToFragmentImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	ssaoColorAttachmentToFragmentImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	ssaoColorAttachmentToFragmentImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	ssaoColorAttachmentToFragmentImageMemoryBarrier.image = m_ssaoImage.handle;
	ssaoColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	ssaoColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	ssaoColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.levelCount = 1;
	ssaoColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	ssaoColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkDependencyInfo ssaoColorAttachmentToFragmentDependencyInfo = {};
	ssaoColorAttachmentToFragmentDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	ssaoColorAttachmentToFragmentDependencyInfo.pNext = nullptr;
	ssaoColorAttachmentToFragmentDependencyInfo.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
	ssaoColorAttachmentToFragmentDependencyInfo.memoryBarrierCount = 0;
	ssaoColorAttachmentToFragmentDependencyInfo.pMemoryBarriers = nullptr;
	ssaoColorAttachmentToFragmentDependencyInfo.bufferMemoryBarrierCount = 0;
	ssaoColorAttachmentToFragmentDependencyInfo.pBufferMemoryBarriers = nullptr;
	ssaoColorAttachmentToFragmentDependencyInfo.imageMemoryBarrierCount = 1;
	ssaoColorAttachmentToFragmentDependencyInfo.pImageMemoryBarriers = &ssaoColorAttachmentToFragmentImageMemoryBarrier;
	m_vkCmdPipelineBarrier2KHR(commandBuffer, &ssaoColorAttachmentToFragmentDependencyInfo);

	// SSAO Blur
	VkRenderingAttachmentInfo ssaoBlurAttachmentInfo = {};
	ssaoBlurAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	ssaoBlurAttachmentInfo.pNext = nullptr;
	ssaoBlurAttachmentInfo.imageView = m_ssaoBlurImage.view;
	ssaoBlurAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	ssaoBlurAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
	ssaoBlurAttachmentInfo.resolveImageView = VK_NULL_HANDLE;
	ssaoBlurAttachmentInfo.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	ssaoBlurAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	ssaoBlurAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	ssaoBlurAttachmentInfo.clearValue.color = { 0.0f, 0.0f, 0.0f, 0.0f };

	VkRenderingInfo ssaoBlurRenderingInfo = {};
	ssaoBlurRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	ssaoBlurRenderingInfo.pNext = nullptr;
	ssaoBlurRenderingInfo.flags = 0;
	ssaoBlurRenderingInfo.renderArea = m_scissor;
	ssaoBlurRenderingInfo.layerCount = 1;
	ssaoBlurRenderingInfo.viewMask = 0;
	ssaoBlurRenderingInfo.colorAttachmentCount = 1;
	ssaoBlurRenderingInfo.pColorAttachments = &ssaoBlurAttachmentInfo;
	ssaoBlurRenderingInfo.pDepthAttachment = nullptr;
	ssaoBlurRenderingInfo.pStencilAttachment = nullptr;
	m_vkCmdBeginRenderingKHR(commandBuffer, &ssaoBlurRenderingInfo);

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ssaoBlurGraphicsPipelineLayout, 0, 1, &m_ssaoBlurDescriptorSet, 0, nullptr);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_ssaoBlurGraphicsPipeline);
	vkCmdSetViewport(commandBuffer, 0, 1, &m_viewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &m_scissor);

	vkCmdDraw(commandBuffer, 3, 1, 0, 0);

	m_vkCmdEndRenderingKHR(commandBuffer);

	VkImageMemoryBarrier2 ssaoBlurColorAttachmentToFragmentImageMemoryBarrier = {};
	ssaoBlurColorAttachmentToFragmentImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	ssaoBlurColorAttachmentToFragmentImageMemoryBarrier.pNext = nullptr;
	ssaoBlurColorAttachmentToFragmentImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	ssaoBlurColorAttachmentToFragmentImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	ssaoBlurColorAttachmentToFragmentImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	ssaoBlurColorAttachmentToFragmentImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	ssaoBlurColorAttachmentToFragmentImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	ssaoBlurColorAttachmentToFragmentImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	ssaoBlurColorAttachmentToFragmentImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	ssaoBlurColorAttachmentToFragmentImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	ssaoBlurColorAttachmentToFragmentImageMemoryBarrier.image = m_ssaoBlurImage.handle;
	ssaoBlurColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	ssaoBlurColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	ssaoBlurColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.levelCount = 1;
	ssaoBlurColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	ssaoBlurColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkDependencyInfo ssaoBlurColorAttachmentToFragmentDependencyInfo = {};
	ssaoBlurColorAttachmentToFragmentDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	ssaoBlurColorAttachmentToFragmentDependencyInfo.pNext = nullptr;
	ssaoBlurColorAttachmentToFragmentDependencyInfo.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
	ssaoBlurColorAttachmentToFragmentDependencyInfo.memoryBarrierCount = 0;
	ssaoBlurColorAttachmentToFragmentDependencyInfo.pMemoryBarriers = nullptr;
	ssaoBlurColorAttachmentToFragmentDependencyInfo.bufferMemoryBarrierCount = 0;
	ssaoBlurColorAttachmentToFragmentDependencyInfo.pBufferMemoryBarriers = nullptr;
	ssaoBlurColorAttachmentToFragmentDependencyInfo.imageMemoryBarrierCount = 1;
	ssaoBlurColorAttachmentToFragmentDependencyInfo.pImageMemoryBarriers = &ssaoBlurColorAttachmentToFragmentImageMemoryBarrier;
	m_vkCmdPipelineBarrier2KHR(commandBuffer, &ssaoBlurColorAttachmentToFragmentDependencyInfo);
}

void SSAO::onResize(uint32_t width,
	uint32_t height,
	VkImageView positionImageView,
	VkImageView normalImageView) {
	m_viewport.width = static_cast<float>(width);
	m_viewport.height = static_cast<float>(height);
	m_scissor.extent.width = width;
	m_scissor.extent.height = height;

	destroySSAOImages();
	createSSAOImages(width, height);

	updateDescriptorSets(positionImageView, normalImageView);
}

VulkanImage& SSAO::getSSAO() {
	return m_ssaoBlurImage;
}

void SSAO::createImagesAndBuffer(uint32_t width, uint32_t height) {
	std::uniform_real_distribution<float> uniformRealDistribution(0.0f, 1.0f);
	std::default_random_engine gen;

	// SSAO images
	createSSAOImages(width, height);

	// Random image
	VkImageCreateInfo randomImageCreateInfo = {};
	randomImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	randomImageCreateInfo.pNext = nullptr;
	randomImageCreateInfo.flags = 0;
	randomImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	randomImageCreateInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	randomImageCreateInfo.extent.width = 4;
	randomImageCreateInfo.extent.height = 4;
	randomImageCreateInfo.extent.depth = 1;
	randomImageCreateInfo.mipLevels = 1;
	randomImageCreateInfo.arrayLayers = 1;
	randomImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	randomImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	randomImageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	randomImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	randomImageCreateInfo.queueFamilyIndexCount = 1;
	randomImageCreateInfo.pQueueFamilyIndices = &m_graphicsQueueFamilyIndex;
	randomImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo randomImageAllocationCreateInfo = {};
	randomImageAllocationCreateInfo.flags = 0;
	randomImageAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	NTSHENGN_VK_CHECK(vmaCreateImage(m_allocator, &randomImageCreateInfo, &randomImageAllocationCreateInfo, &m_randomImage.handle, &m_randomImage.allocation, nullptr));

	VkImageViewCreateInfo randomImageViewCreateInfo = {};
	randomImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	randomImageViewCreateInfo.pNext = nullptr;
	randomImageViewCreateInfo.flags = 0;
	randomImageViewCreateInfo.image = m_randomImage.handle;
	randomImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	randomImageViewCreateInfo.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	randomImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
	randomImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
	randomImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
	randomImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
	randomImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	randomImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	randomImageViewCreateInfo.subresourceRange.levelCount = 1;
	randomImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	randomImageViewCreateInfo.subresourceRange.layerCount = 1;
	NTSHENGN_VK_CHECK(vkCreateImageView(m_device, &randomImageViewCreateInfo, nullptr, &m_randomImage.view));

	VulkanBuffer stagingBuffer;

	VkBufferCreateInfo stagingBufferCreateInfo = {};
	stagingBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	stagingBufferCreateInfo.pNext = nullptr;
	stagingBufferCreateInfo.flags = 0;
	stagingBufferCreateInfo.size = 16 * 4 * sizeof(float);
	stagingBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	stagingBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	stagingBufferCreateInfo.queueFamilyIndexCount = 1;
	stagingBufferCreateInfo.pQueueFamilyIndices = &m_graphicsQueueFamilyIndex;

	VmaAllocationCreateInfo stagingBufferAllocationCreateInfo = {};
	stagingBufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	stagingBufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
	NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &stagingBufferCreateInfo, &stagingBufferAllocationCreateInfo, &stagingBuffer.handle, &stagingBuffer.allocation, nullptr));

	std::array<float, 16 * 4> randomData;
	for (uint8_t i = 0; i < 16; i++) {
		randomData[(i * 4)] = uniformRealDistribution(gen) * 2.0f - 1.0f;
		randomData[(i * 4) + 1] = uniformRealDistribution(gen) * 2.0f - 1.0f;
		randomData[(i * 4) + 2] = 0.0f;
		randomData[(i * 4) + 3] = 1.0f;
	}

	void* data;
	NTSHENGN_VK_CHECK(vmaMapMemory(m_allocator, stagingBuffer.allocation, &data));
	memcpy(data, randomData.data(), 16 * 4 * sizeof(float));
	vmaUnmapMemory(m_allocator, stagingBuffer.allocation);

	NTSHENGN_VK_CHECK(vkResetCommandPool(m_device, m_initializationCommandPool, 0));

	VkCommandBufferBeginInfo commandBufferBeginInfo = {};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.pNext = nullptr;
	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	commandBufferBeginInfo.pInheritanceInfo = nullptr;
	NTSHENGN_VK_CHECK(vkBeginCommandBuffer(m_initializationCommandBuffer, &commandBufferBeginInfo));

	VkImageMemoryBarrier2 undefinedToTransferDstImageMemoryBarrier = {};
	undefinedToTransferDstImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	undefinedToTransferDstImageMemoryBarrier.pNext = nullptr;
	undefinedToTransferDstImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
	undefinedToTransferDstImageMemoryBarrier.srcAccessMask = 0;
	undefinedToTransferDstImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
	undefinedToTransferDstImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	undefinedToTransferDstImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	undefinedToTransferDstImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	undefinedToTransferDstImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	undefinedToTransferDstImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	undefinedToTransferDstImageMemoryBarrier.image = m_randomImage.handle;
	undefinedToTransferDstImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	undefinedToTransferDstImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	undefinedToTransferDstImageMemoryBarrier.subresourceRange.levelCount = 1;
	undefinedToTransferDstImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	undefinedToTransferDstImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkDependencyInfo undefinedToTransferDstDependencyInfo = {};
	undefinedToTransferDstDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	undefinedToTransferDstDependencyInfo.pNext = nullptr;
	undefinedToTransferDstDependencyInfo.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
	undefinedToTransferDstDependencyInfo.memoryBarrierCount = 0;
	undefinedToTransferDstDependencyInfo.pMemoryBarriers = nullptr;
	undefinedToTransferDstDependencyInfo.bufferMemoryBarrierCount = 0;
	undefinedToTransferDstDependencyInfo.pBufferMemoryBarriers = nullptr;
	undefinedToTransferDstDependencyInfo.imageMemoryBarrierCount = 1;
	undefinedToTransferDstDependencyInfo.pImageMemoryBarriers = &undefinedToTransferDstImageMemoryBarrier;
	m_vkCmdPipelineBarrier2KHR(m_initializationCommandBuffer, &undefinedToTransferDstDependencyInfo);

	VkBufferImageCopy bufferImageCopy = {};
	bufferImageCopy.bufferOffset = 0;
	bufferImageCopy.bufferRowLength = 0;
	bufferImageCopy.bufferImageHeight = 0;
	bufferImageCopy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	bufferImageCopy.imageSubresource.mipLevel = 0;
	bufferImageCopy.imageSubresource.baseArrayLayer = 0;
	bufferImageCopy.imageSubresource.layerCount = 1;
	bufferImageCopy.imageOffset.x = 0;
	bufferImageCopy.imageOffset.y = 0;
	bufferImageCopy.imageOffset.z = 0;
	bufferImageCopy.imageExtent.width = 4;
	bufferImageCopy.imageExtent.height = 4;
	bufferImageCopy.imageExtent.depth = 1;
	vkCmdCopyBufferToImage(m_initializationCommandBuffer, stagingBuffer.handle, m_randomImage.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferImageCopy);

	VkImageMemoryBarrier2 transferDstToShaderReadOnlyImageMemoryBarrier = {};
	transferDstToShaderReadOnlyImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	transferDstToShaderReadOnlyImageMemoryBarrier.pNext = nullptr;
	transferDstToShaderReadOnlyImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
	transferDstToShaderReadOnlyImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	transferDstToShaderReadOnlyImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	transferDstToShaderReadOnlyImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	transferDstToShaderReadOnlyImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	transferDstToShaderReadOnlyImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	transferDstToShaderReadOnlyImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	transferDstToShaderReadOnlyImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	transferDstToShaderReadOnlyImageMemoryBarrier.image = m_randomImage.handle;
	transferDstToShaderReadOnlyImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	transferDstToShaderReadOnlyImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	transferDstToShaderReadOnlyImageMemoryBarrier.subresourceRange.levelCount = 1;
	transferDstToShaderReadOnlyImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	transferDstToShaderReadOnlyImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkDependencyInfo transferDstToShaderReadOnlyDependencyInfo = {};
	transferDstToShaderReadOnlyDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	transferDstToShaderReadOnlyDependencyInfo.pNext = nullptr;
	transferDstToShaderReadOnlyDependencyInfo.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
	transferDstToShaderReadOnlyDependencyInfo.memoryBarrierCount = 0;
	transferDstToShaderReadOnlyDependencyInfo.pMemoryBarriers = nullptr;
	transferDstToShaderReadOnlyDependencyInfo.bufferMemoryBarrierCount = 0;
	transferDstToShaderReadOnlyDependencyInfo.pBufferMemoryBarriers = nullptr;
	transferDstToShaderReadOnlyDependencyInfo.imageMemoryBarrierCount = 1;
	transferDstToShaderReadOnlyDependencyInfo.pImageMemoryBarriers = &transferDstToShaderReadOnlyImageMemoryBarrier;
	m_vkCmdPipelineBarrier2KHR(m_initializationCommandBuffer, &transferDstToShaderReadOnlyDependencyInfo);

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

	stagingBuffer.destroy(m_allocator);

	// Random sample buffer
	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.pNext = nullptr;
	bufferCreateInfo.flags = 0;
	bufferCreateInfo.size = SSAO_SAMPLE_COUNT * sizeof(NtshEngn::Math::vec4);
	bufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bufferCreateInfo.queueFamilyIndexCount = 1;
	bufferCreateInfo.pQueueFamilyIndices = &m_graphicsQueueFamilyIndex;

	VmaAllocationCreateInfo bufferAllocationCreateInfo = {};
	bufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	bufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
	NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &bufferCreateInfo, &bufferAllocationCreateInfo, &m_randomSampleBuffer.handle, &m_randomSampleBuffer.allocation, nullptr));

	std::array<NtshEngn::Math::vec4, SSAO_SAMPLE_COUNT> samples;
	for (uint32_t i = 0; i < SSAO_SAMPLE_COUNT; i++) {
		NtshEngn::Math::vec3 tmpSample = NtshEngn::Math::vec3(uniformRealDistribution(gen) * 2.0f - 1.0f,
			uniformRealDistribution(gen) * 2.0f - 1.0f,
			uniformRealDistribution(gen));

		tmpSample = NtshEngn::Math::normalize(tmpSample);
		tmpSample *= uniformRealDistribution(gen);

		float scale = static_cast<float>(i) / static_cast<float>(SSAO_SAMPLE_COUNT);
		scale = 0.1f + (scale * scale) * (1.0f - 0.1f);
		tmpSample *= scale;

		samples[i] = NtshEngn::Math::vec4(tmpSample, 0.0f);
	}

	NTSHENGN_VK_CHECK(vmaMapMemory(m_allocator, m_randomSampleBuffer.allocation, &data));
	memcpy(data, samples.data(), SSAO_SAMPLE_COUNT * sizeof(NtshEngn::Math::vec4));
	vmaUnmapMemory(m_allocator, m_randomSampleBuffer.allocation);
}

void SSAO::createSSAOImages(uint32_t width, uint32_t height) {
	VkImageCreateInfo ssaoImageCreateInfo = {};
	ssaoImageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ssaoImageCreateInfo.pNext = nullptr;
	ssaoImageCreateInfo.flags = 0;
	ssaoImageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	ssaoImageCreateInfo.format = VK_FORMAT_R8_UNORM;
	ssaoImageCreateInfo.extent.width = width;
	ssaoImageCreateInfo.extent.height = height;
	ssaoImageCreateInfo.extent.depth = 1;
	ssaoImageCreateInfo.mipLevels = 1;
	ssaoImageCreateInfo.arrayLayers = 1;
	ssaoImageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	ssaoImageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	ssaoImageCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	ssaoImageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ssaoImageCreateInfo.queueFamilyIndexCount = 1;
	ssaoImageCreateInfo.pQueueFamilyIndices = &m_graphicsQueueFamilyIndex;
	ssaoImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo ssaoImageAllocationCreateInfo = {};
	ssaoImageAllocationCreateInfo.flags = 0;
	ssaoImageAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	NTSHENGN_VK_CHECK(vmaCreateImage(m_allocator, &ssaoImageCreateInfo, &ssaoImageAllocationCreateInfo, &m_ssaoImage.handle, &m_ssaoImage.allocation, nullptr));
	NTSHENGN_VK_CHECK(vmaCreateImage(m_allocator, &ssaoImageCreateInfo, &ssaoImageAllocationCreateInfo, &m_ssaoBlurImage.handle, &m_ssaoBlurImage.allocation, nullptr));

	VkImageViewCreateInfo ssaoImageViewCreateInfo = {};
	ssaoImageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	ssaoImageViewCreateInfo.pNext = nullptr;
	ssaoImageViewCreateInfo.flags = 0;
	ssaoImageViewCreateInfo.image = m_ssaoImage.handle;
	ssaoImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	ssaoImageViewCreateInfo.format = VK_FORMAT_R8_UNORM;
	ssaoImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
	ssaoImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
	ssaoImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
	ssaoImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
	ssaoImageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	ssaoImageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	ssaoImageViewCreateInfo.subresourceRange.levelCount = 1;
	ssaoImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	ssaoImageViewCreateInfo.subresourceRange.layerCount = 1;
	NTSHENGN_VK_CHECK(vkCreateImageView(m_device, &ssaoImageViewCreateInfo, nullptr, &m_ssaoImage.view));

	ssaoImageViewCreateInfo.image = m_ssaoBlurImage.handle;
	NTSHENGN_VK_CHECK(vkCreateImageView(m_device, &ssaoImageViewCreateInfo, nullptr, &m_ssaoBlurImage.view));
}

void SSAO::destroySSAOImages() {
	m_ssaoBlurImage.destroy(m_device, m_allocator);
	m_ssaoImage.destroy(m_device, m_allocator);
}

void SSAO::createImageSamplers() {
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
	NTSHENGN_VK_CHECK(vkCreateSampler(m_device, &samplerCreateInfo, nullptr, &m_nearestSampler));

	samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	NTSHENGN_VK_CHECK(vkCreateSampler(m_device, &samplerCreateInfo, nullptr, &m_repeatSampler));
}

void SSAO::createDescriptorSetLayouts() {
	VkDescriptorSetLayoutBinding positionDescriptorSetLayoutBinding = {};
	positionDescriptorSetLayoutBinding.binding = 0;
	positionDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	positionDescriptorSetLayoutBinding.descriptorCount = 1;
	positionDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	positionDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding normalDescriptorSetLayoutBinding = {};
	normalDescriptorSetLayoutBinding.binding = 1;
	normalDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	normalDescriptorSetLayoutBinding.descriptorCount = 1;
	normalDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	normalDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding randomImageDescriptorSetLayoutBinding = {};
	randomImageDescriptorSetLayoutBinding.binding = 2;
	randomImageDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	randomImageDescriptorSetLayoutBinding.descriptorCount = 1;
	randomImageDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	randomImageDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding randomSampleBufferDescriptorSetLayoutBinding = {};
	randomSampleBufferDescriptorSetLayoutBinding.binding = 3;
	randomSampleBufferDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	randomSampleBufferDescriptorSetLayoutBinding.descriptorCount = 1;
	randomSampleBufferDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	randomSampleBufferDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding cameraBufferDescriptorSetLayoutBinding = {};
	cameraBufferDescriptorSetLayoutBinding.binding = 4;
	cameraBufferDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	cameraBufferDescriptorSetLayoutBinding.descriptorCount = 1;
	cameraBufferDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	cameraBufferDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	std::array<VkDescriptorSetLayoutBinding, 5> descriptorSetLayoutBindings = { positionDescriptorSetLayoutBinding, normalDescriptorSetLayoutBinding, randomImageDescriptorSetLayoutBinding, randomSampleBufferDescriptorSetLayoutBinding, cameraBufferDescriptorSetLayoutBinding };
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.pNext = nullptr;
	descriptorSetLayoutCreateInfo.flags = 0;
	descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(descriptorSetLayoutBindings.size());
	descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings.data();
	NTSHENGN_VK_CHECK(vkCreateDescriptorSetLayout(m_device, &descriptorSetLayoutCreateInfo, nullptr, &m_ssaoDescriptorSetLayout));

	VkDescriptorSetLayoutBinding ssaoDescriptorSetLayoutBinding = {};
	ssaoDescriptorSetLayoutBinding.binding = 0;
	ssaoDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	ssaoDescriptorSetLayoutBinding.descriptorCount = 1;
	ssaoDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	ssaoDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	descriptorSetLayoutCreateInfo.bindingCount = 1;
	descriptorSetLayoutCreateInfo.pBindings = &ssaoDescriptorSetLayoutBinding;
	NTSHENGN_VK_CHECK(vkCreateDescriptorSetLayout(m_device, &descriptorSetLayoutCreateInfo, nullptr, &m_ssaoBlurDescriptorSetLayout));
}

void SSAO::createGraphicsPipelines() {
	createSSAOGraphicsPipeline();
	createSSAOBlurGraphicsPipeline();
}

void SSAO::createSSAOGraphicsPipeline() {
	VkFormat pipelineRenderingFormat = VK_FORMAT_R8_UNORM;
	VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {};
	pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	pipelineRenderingCreateInfo.pNext = nullptr;
	pipelineRenderingCreateInfo.viewMask = 0;
	pipelineRenderingCreateInfo.colorAttachmentCount = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &pipelineRenderingFormat;
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

		#define SSAO_SAMPLE_COUNT )GLSL";
	fragmentShaderCode += std::to_string(SSAO_SAMPLE_COUNT);
	fragmentShaderCode += R"GLSL(
		
		layout(set = 0, binding = 0) uniform sampler2D positionSampler;
		layout(set = 0, binding = 1) uniform sampler2D normalSampler;
		layout(set = 0, binding = 2) uniform sampler2D randomSampler;

		layout(set = 0, binding = 3) uniform KernelSamples {
			vec3 samples[SSAO_SAMPLE_COUNT];
		} kernel;

		layout(set = 0, binding = 4) uniform Camera {
			mat4 view;
			mat4 projection;
			vec3 position;
		} camera;

		layout(location = 0) in vec2 uv;

		layout(location = 0) out float outColor;

		void main() {
			vec2 imageSize = textureSize(positionSampler, 0);
			vec2 randomScale = imageSize / 4.0;
			float radius = 0.25;
			float bias = 0.025;

			vec3 position = texture(positionSampler, uv).xyz;
			position = vec3(camera.view * vec4(position, 1.0));
			vec3 normal = texture(normalSampler, uv).xyz;
			normal = normalize(vec3(camera.view * vec4(normal, 0.0)));
			vec3 random = texture(randomSampler, uv * randomScale).xyz;

			vec3 tangent = normalize(random - (normal * dot(random, normal)));
			vec3 bitangent = cross(normal, tangent);
			mat3 TBN = mat3(tangent, bitangent, normal);

			float occlusion = 0.0;
			for (uint i = 0; i < SSAO_SAMPLE_COUNT; i++) {
				vec3 samplePos = TBN * kernel.samples[i];
				samplePos = position + (samplePos * radius);

				vec4 offset = vec4(samplePos, 1.0);
				offset = camera.projection * offset;
				offset.xyz /= offset.w;
				offset.xyz = offset.xyz * 0.5 + 0.5;
				
				vec4 depthPosition = texture(positionSampler, offset.xy);
				if (depthPosition.w == 1.0) {
					float depth = (camera.view * depthPosition).z;

					float rangeCheck = smoothstep(0.0, 1.0, radius / abs(position.z - depth));
					occlusion += ((depth >= (samplePos.z + bias)) ? 1.0 : 0.0) * rangeCheck;
				}
			}
			occlusion = 1.0 - (occlusion / float(SSAO_SAMPLE_COUNT));

			outColor = occlusion;
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
	pipelineLayoutCreateInfo.pSetLayouts = &m_ssaoDescriptorSetLayout;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;
	NTSHENGN_VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_ssaoGraphicsPipelineLayout));

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
	graphicsPipelineCreateInfo.layout = m_ssaoGraphicsPipelineLayout;
	graphicsPipelineCreateInfo.renderPass = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.subpass = 0;
	graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.basePipelineIndex = 0;
	NTSHENGN_VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &m_ssaoGraphicsPipeline));

	vkDestroyShaderModule(m_device, vertexShaderModule, nullptr);
	vkDestroyShaderModule(m_device, fragmentShaderModule, nullptr);
}

void SSAO::createSSAOBlurGraphicsPipeline() {
	VkFormat pipelineRenderingFormat = VK_FORMAT_R8_UNORM;
	VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {};
	pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	pipelineRenderingCreateInfo.pNext = nullptr;
	pipelineRenderingCreateInfo.viewMask = 0;
	pipelineRenderingCreateInfo.colorAttachmentCount = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &pipelineRenderingFormat;
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

		layout(set = 0, binding = 0) uniform sampler2D ssaoSampler;

		layout(location = 0) in vec2 uv;

		layout(location = 0) out float outColor;

		void main() {
			vec2 texelSize = 1.0 / vec2(textureSize(ssaoSampler, 0));

			float result = 0.0;
			for (float x = -2.0; x < 2.0; x++) {
				for (float y = -2.0; y < 2.0; y++) {
					vec2 offset = vec2(x, y) * texelSize;
					result += texture(ssaoSampler, uv + offset).r;
				}
			}

			outColor = result / 16.0;
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
	pipelineLayoutCreateInfo.pSetLayouts = &m_ssaoBlurDescriptorSetLayout;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;
	NTSHENGN_VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_ssaoBlurGraphicsPipelineLayout));

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
	graphicsPipelineCreateInfo.layout = m_ssaoBlurGraphicsPipelineLayout;
	graphicsPipelineCreateInfo.renderPass = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.subpass = 0;
	graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.basePipelineIndex = 0;
	NTSHENGN_VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &m_ssaoBlurGraphicsPipeline));

	vkDestroyShaderModule(m_device, vertexShaderModule, nullptr);
	vkDestroyShaderModule(m_device, fragmentShaderModule, nullptr);
}

void SSAO::createDescriptorSets(const std::vector<HostVisibleVulkanBuffer>& cameraBuffers) {
	createSSAODescriptorSets(cameraBuffers);
	createSSAOBlurDescriptorSet();
}

void SSAO::createSSAODescriptorSets(const std::vector<HostVisibleVulkanBuffer>& cameraBuffers) {
	// Create descriptor pool
	VkDescriptorPoolSize positionDescriptorPoolSize = {};
	positionDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	positionDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize normalDescriptorPoolSize = {};
	normalDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	normalDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize randomImageDescriptorPoolSize = {};
	randomImageDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	randomImageDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize randomSampleDescriptorPoolSize = {};
	randomSampleDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	randomSampleDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize cameraDescriptorPoolSize = {};
	cameraDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	cameraDescriptorPoolSize.descriptorCount = m_framesInFlight;

	std::array<VkDescriptorPoolSize, 5> descriptorPoolSizes = { positionDescriptorPoolSize, normalDescriptorPoolSize, randomImageDescriptorPoolSize, randomSampleDescriptorPoolSize, cameraDescriptorPoolSize };
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext = nullptr;
	descriptorPoolCreateInfo.flags = 0;
	descriptorPoolCreateInfo.maxSets = m_framesInFlight;
	descriptorPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size());
	descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes.data();
	NTSHENGN_VK_CHECK(vkCreateDescriptorPool(m_device, &descriptorPoolCreateInfo, nullptr, &m_ssaoDescriptorPool));

	// Allocate descriptor set
	m_ssaoDescriptorSets.resize(m_framesInFlight);
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
		descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocateInfo.pNext = nullptr;
		descriptorSetAllocateInfo.descriptorPool = m_ssaoDescriptorPool;
		descriptorSetAllocateInfo.descriptorSetCount = 1;
		descriptorSetAllocateInfo.pSetLayouts = &m_ssaoDescriptorSetLayout;
		NTSHENGN_VK_CHECK(vkAllocateDescriptorSets(m_device, &descriptorSetAllocateInfo, &m_ssaoDescriptorSets[i]));
	}

	// Update descriptor set
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		VkDescriptorImageInfo randomImageDescriptorImageInfo;
		randomImageDescriptorImageInfo.sampler = m_repeatSampler;
		randomImageDescriptorImageInfo.imageView = m_randomImage.view;
		randomImageDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet randomImageDescriptorWriteDescriptorSet = {};
		randomImageDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		randomImageDescriptorWriteDescriptorSet.pNext = nullptr;
		randomImageDescriptorWriteDescriptorSet.dstSet = m_ssaoDescriptorSets[i];
		randomImageDescriptorWriteDescriptorSet.dstBinding = 2;
		randomImageDescriptorWriteDescriptorSet.dstArrayElement = 0;
		randomImageDescriptorWriteDescriptorSet.descriptorCount = 1;
		randomImageDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		randomImageDescriptorWriteDescriptorSet.pImageInfo = &randomImageDescriptorImageInfo;
		randomImageDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
		randomImageDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

		VkDescriptorBufferInfo randomSampleDescriptorBufferInfo;
		randomSampleDescriptorBufferInfo.buffer = m_randomSampleBuffer.handle;
		randomSampleDescriptorBufferInfo.offset = 0;
		randomSampleDescriptorBufferInfo.range = SSAO_SAMPLE_COUNT * sizeof(NtshEngn::Math::vec4);

		VkWriteDescriptorSet randomSampleDescriptorWriteDescriptorSet = {};
		randomSampleDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		randomSampleDescriptorWriteDescriptorSet.pNext = nullptr;
		randomSampleDescriptorWriteDescriptorSet.dstSet = m_ssaoDescriptorSets[i];
		randomSampleDescriptorWriteDescriptorSet.dstBinding = 3;
		randomSampleDescriptorWriteDescriptorSet.dstArrayElement = 0;
		randomSampleDescriptorWriteDescriptorSet.descriptorCount = 1;
		randomSampleDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		randomSampleDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		randomSampleDescriptorWriteDescriptorSet.pBufferInfo = &randomSampleDescriptorBufferInfo;
		randomSampleDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

		VkDescriptorBufferInfo cameraDescriptorBufferInfo;
		cameraDescriptorBufferInfo.buffer = cameraBuffers[i].handle;
		cameraDescriptorBufferInfo.offset = 0;
		cameraDescriptorBufferInfo.range = sizeof(NtshEngn::Math::mat4) * 2 + sizeof(NtshEngn::Math::vec4);

		VkWriteDescriptorSet cameraDescriptorWriteDescriptorSet = {};
		cameraDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		cameraDescriptorWriteDescriptorSet.pNext = nullptr;
		cameraDescriptorWriteDescriptorSet.dstSet = m_ssaoDescriptorSets[i];
		cameraDescriptorWriteDescriptorSet.dstBinding = 4;
		cameraDescriptorWriteDescriptorSet.dstArrayElement = 0;
		cameraDescriptorWriteDescriptorSet.descriptorCount = 1;
		cameraDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		cameraDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		cameraDescriptorWriteDescriptorSet.pBufferInfo = &cameraDescriptorBufferInfo;
		cameraDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

		std::array<VkWriteDescriptorSet, 3> writeDescriptorSets = { randomImageDescriptorWriteDescriptorSet, randomSampleDescriptorWriteDescriptorSet, cameraDescriptorWriteDescriptorSet };
		vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}
}

void SSAO::createSSAOBlurDescriptorSet() {
	// Create descriptor pool
	VkDescriptorPoolSize ssaoDescriptorPoolSize = {};
	ssaoDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	ssaoDescriptorPoolSize.descriptorCount = 1;

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext = nullptr;
	descriptorPoolCreateInfo.flags = 0;
	descriptorPoolCreateInfo.maxSets = 1;
	descriptorPoolCreateInfo.poolSizeCount = 1;
	descriptorPoolCreateInfo.pPoolSizes = &ssaoDescriptorPoolSize;
	NTSHENGN_VK_CHECK(vkCreateDescriptorPool(m_device, &descriptorPoolCreateInfo, nullptr, &m_ssaoBlurDescriptorPool));

	// Allocate descriptor set
	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
	descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetAllocateInfo.pNext = nullptr;
	descriptorSetAllocateInfo.descriptorPool = m_ssaoBlurDescriptorPool;
	descriptorSetAllocateInfo.descriptorSetCount = 1;
	descriptorSetAllocateInfo.pSetLayouts = &m_ssaoBlurDescriptorSetLayout;
	NTSHENGN_VK_CHECK(vkAllocateDescriptorSets(m_device, &descriptorSetAllocateInfo, &m_ssaoBlurDescriptorSet));
}

void SSAO::updateDescriptorSets(VkImageView positionImageView, VkImageView normalImageView) {
	updateSSAODescriptorSet(positionImageView, normalImageView);
	updateSSAOBlurDescriptorSet();
}

void SSAO::updateSSAODescriptorSet(VkImageView positionImageView, VkImageView normalImageView) {
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		VkDescriptorImageInfo positionDescriptorImageInfo;
		positionDescriptorImageInfo.sampler = m_nearestSampler;
		positionDescriptorImageInfo.imageView = positionImageView;
		positionDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet positionDescriptorWriteDescriptorSet = {};
		positionDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		positionDescriptorWriteDescriptorSet.pNext = nullptr;
		positionDescriptorWriteDescriptorSet.dstSet = m_ssaoDescriptorSets[i];
		positionDescriptorWriteDescriptorSet.dstBinding = 0;
		positionDescriptorWriteDescriptorSet.dstArrayElement = 0;
		positionDescriptorWriteDescriptorSet.descriptorCount = 1;
		positionDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		positionDescriptorWriteDescriptorSet.pImageInfo = &positionDescriptorImageInfo;
		positionDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
		positionDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

		VkDescriptorImageInfo normalDescriptorImageInfo;
		normalDescriptorImageInfo.sampler = m_nearestSampler;
		normalDescriptorImageInfo.imageView = normalImageView;
		normalDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

		VkWriteDescriptorSet normalDescriptorWriteDescriptorSet = {};
		normalDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		normalDescriptorWriteDescriptorSet.pNext = nullptr;
		normalDescriptorWriteDescriptorSet.dstSet = m_ssaoDescriptorSets[i];
		normalDescriptorWriteDescriptorSet.dstBinding = 1;
		normalDescriptorWriteDescriptorSet.dstArrayElement = 0;
		normalDescriptorWriteDescriptorSet.descriptorCount = 1;
		normalDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		normalDescriptorWriteDescriptorSet.pImageInfo = &normalDescriptorImageInfo;
		normalDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
		normalDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

		std::array<VkWriteDescriptorSet, 2> writeDescriptorSets = { positionDescriptorWriteDescriptorSet, normalDescriptorWriteDescriptorSet };
		vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}
}

void SSAO::updateSSAOBlurDescriptorSet() {
	VkDescriptorImageInfo ssaoDescriptorImageInfo;
	ssaoDescriptorImageInfo.sampler = m_nearestSampler;
	ssaoDescriptorImageInfo.imageView = m_ssaoImage.view;
	ssaoDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	VkWriteDescriptorSet ssaoDescriptorWriteDescriptorSet = {};
	ssaoDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	ssaoDescriptorWriteDescriptorSet.pNext = nullptr;
	ssaoDescriptorWriteDescriptorSet.dstSet = m_ssaoBlurDescriptorSet;
	ssaoDescriptorWriteDescriptorSet.dstBinding = 0;
	ssaoDescriptorWriteDescriptorSet.dstArrayElement = 0;
	ssaoDescriptorWriteDescriptorSet.descriptorCount = 1;
	ssaoDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	ssaoDescriptorWriteDescriptorSet.pImageInfo = &ssaoDescriptorImageInfo;
	ssaoDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
	ssaoDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

	vkUpdateDescriptorSets(m_device, 1, &ssaoDescriptorWriteDescriptorSet, 0, nullptr);
}
