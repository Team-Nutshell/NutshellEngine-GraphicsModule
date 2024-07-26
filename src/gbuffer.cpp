#include "gbuffer.h"

void GBuffer::init(VkDevice device,
	VkQueue graphicsQueue,
	uint32_t graphicsQueueFamilyIndex,
	VmaAllocator allocator,
	VkCommandPool initializationCommandPool,
	VkCommandBuffer initializationCommandBuffer,
	VkFence initializationFence,
	VkViewport viewport,
	VkRect2D scissor,
	uint32_t framesInFlight,
	const std::vector<VkBuffer>& perDrawBuffers,
	const std::vector<HostVisibleVulkanBuffer>& cameraBuffers,
	const std::vector<HostVisibleVulkanBuffer>& objectBuffers,
	VulkanBuffer meshBuffer,
	const std::vector<HostVisibleVulkanBuffer>& jointTransformBuffers,
	const std::vector<HostVisibleVulkanBuffer>& materialBuffers,
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
	m_vkCmdDrawIndexedIndirectCountKHR = (PFN_vkCmdDrawIndexedIndirectCountKHR)vkGetDeviceProcAddr(m_device, "vkCmdDrawIndexedIndirectCountKHR");

	createImages(m_scissor.extent.width, m_scissor.extent.height);
	createDescriptorSetLayout();
	createGraphicsPipeline();
	createDescriptorSets(perDrawBuffers, cameraBuffers, objectBuffers, meshBuffer, jointTransformBuffers, materialBuffers);
}

void GBuffer::destroy() {
	vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);

	vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_graphicsPipelineLayout, nullptr);

	vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);

	destroyImages();
}

void GBuffer::draw(VkCommandBuffer commandBuffer,
	uint32_t currentFrameInFlight,
	VulkanBuffer& drawIndirectBuffer,
	uint32_t drawIndirectCount,
	VulkanBuffer vertexBuffer,
	VulkanBuffer indexBuffer) {
	// G-Buffer layout transition
	VkImageMemoryBarrier2 positionFragmentToColorAttachmentImageMemoryBarrier = {};
	positionFragmentToColorAttachmentImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	positionFragmentToColorAttachmentImageMemoryBarrier.pNext = nullptr;
	positionFragmentToColorAttachmentImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	positionFragmentToColorAttachmentImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	positionFragmentToColorAttachmentImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	positionFragmentToColorAttachmentImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	positionFragmentToColorAttachmentImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	positionFragmentToColorAttachmentImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	positionFragmentToColorAttachmentImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	positionFragmentToColorAttachmentImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	positionFragmentToColorAttachmentImageMemoryBarrier.image = m_position.handle;
	positionFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	positionFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	positionFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.levelCount = 1;
	positionFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	positionFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkImageMemoryBarrier2 normalFragmentToColorAttachmentImageMemoryBarrier = {};
	normalFragmentToColorAttachmentImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	normalFragmentToColorAttachmentImageMemoryBarrier.pNext = nullptr;
	normalFragmentToColorAttachmentImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	normalFragmentToColorAttachmentImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	normalFragmentToColorAttachmentImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	normalFragmentToColorAttachmentImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	normalFragmentToColorAttachmentImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	normalFragmentToColorAttachmentImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	normalFragmentToColorAttachmentImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	normalFragmentToColorAttachmentImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	normalFragmentToColorAttachmentImageMemoryBarrier.image = m_normal.handle;
	normalFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	normalFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	normalFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.levelCount = 1;
	normalFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	normalFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkImageMemoryBarrier2 diffuseFragmentToColorAttachmentImageMemoryBarrier = {};
	diffuseFragmentToColorAttachmentImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	diffuseFragmentToColorAttachmentImageMemoryBarrier.pNext = nullptr;
	diffuseFragmentToColorAttachmentImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	diffuseFragmentToColorAttachmentImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	diffuseFragmentToColorAttachmentImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	diffuseFragmentToColorAttachmentImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	diffuseFragmentToColorAttachmentImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	diffuseFragmentToColorAttachmentImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	diffuseFragmentToColorAttachmentImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	diffuseFragmentToColorAttachmentImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	diffuseFragmentToColorAttachmentImageMemoryBarrier.image = m_diffuse.handle;
	diffuseFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	diffuseFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	diffuseFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.levelCount = 1;
	diffuseFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	diffuseFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkImageMemoryBarrier2 materialFragmentToColorAttachmentImageMemoryBarrier = {};
	materialFragmentToColorAttachmentImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	materialFragmentToColorAttachmentImageMemoryBarrier.pNext = nullptr;
	materialFragmentToColorAttachmentImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	materialFragmentToColorAttachmentImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	materialFragmentToColorAttachmentImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	materialFragmentToColorAttachmentImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	materialFragmentToColorAttachmentImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	materialFragmentToColorAttachmentImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	materialFragmentToColorAttachmentImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	materialFragmentToColorAttachmentImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	materialFragmentToColorAttachmentImageMemoryBarrier.image = m_material.handle;
	materialFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	materialFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	materialFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.levelCount = 1;
	materialFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	materialFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkImageMemoryBarrier2 emissiveFragmentToColorAttachmentImageMemoryBarrier = {};
	emissiveFragmentToColorAttachmentImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	emissiveFragmentToColorAttachmentImageMemoryBarrier.pNext = nullptr;
	emissiveFragmentToColorAttachmentImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	emissiveFragmentToColorAttachmentImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	emissiveFragmentToColorAttachmentImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	emissiveFragmentToColorAttachmentImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	emissiveFragmentToColorAttachmentImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	emissiveFragmentToColorAttachmentImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	emissiveFragmentToColorAttachmentImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	emissiveFragmentToColorAttachmentImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	emissiveFragmentToColorAttachmentImageMemoryBarrier.image = m_emissive.handle;
	emissiveFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	emissiveFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	emissiveFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.levelCount = 1;
	emissiveFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	emissiveFragmentToColorAttachmentImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkImageMemoryBarrier2 depthImageMemoryBarrier = {};
	depthImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	depthImageMemoryBarrier.pNext = nullptr;
	depthImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
	depthImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	depthImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
	depthImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	depthImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depthImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	depthImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	depthImageMemoryBarrier.image = m_depth.handle;
	depthImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	depthImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	depthImageMemoryBarrier.subresourceRange.levelCount = 1;
	depthImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	depthImageMemoryBarrier.subresourceRange.layerCount = 1;

	std::array<VkImageMemoryBarrier2, 6> beforeRenderImageMemoryBarriers = { positionFragmentToColorAttachmentImageMemoryBarrier, normalFragmentToColorAttachmentImageMemoryBarrier, diffuseFragmentToColorAttachmentImageMemoryBarrier, materialFragmentToColorAttachmentImageMemoryBarrier, emissiveFragmentToColorAttachmentImageMemoryBarrier, depthImageMemoryBarrier };
	VkDependencyInfo beforeRenderDependencyInfo = {};
	beforeRenderDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	beforeRenderDependencyInfo.pNext = nullptr;
	beforeRenderDependencyInfo.dependencyFlags = 0;
	beforeRenderDependencyInfo.memoryBarrierCount = 0;
	beforeRenderDependencyInfo.pMemoryBarriers = nullptr;
	beforeRenderDependencyInfo.bufferMemoryBarrierCount = 0;
	beforeRenderDependencyInfo.pBufferMemoryBarriers = nullptr;
	beforeRenderDependencyInfo.imageMemoryBarrierCount = static_cast<uint32_t>(beforeRenderImageMemoryBarriers.size());
	beforeRenderDependencyInfo.pImageMemoryBarriers = beforeRenderImageMemoryBarriers.data();
	m_vkCmdPipelineBarrier2KHR(commandBuffer, &beforeRenderDependencyInfo);

	// Begin G-Buffer rendering
	VkRenderingAttachmentInfo positionAttachmentInfo = {};
	positionAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	positionAttachmentInfo.pNext = nullptr;
	positionAttachmentInfo.imageView = m_position.view;
	positionAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	positionAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
	positionAttachmentInfo.resolveImageView = VK_NULL_HANDLE;
	positionAttachmentInfo.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	positionAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	positionAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	positionAttachmentInfo.clearValue.color = { 0.0f, 0.0f, 0.0f, 0.0f };

	VkRenderingAttachmentInfo normalAttachmentInfo = {};
	normalAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	normalAttachmentInfo.pNext = nullptr;
	normalAttachmentInfo.imageView = m_normal.view;
	normalAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	normalAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
	normalAttachmentInfo.resolveImageView = VK_NULL_HANDLE;
	normalAttachmentInfo.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	normalAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	normalAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	normalAttachmentInfo.clearValue.color = { 0.0f, 0.0f, 0.0f, 0.0f };

	VkRenderingAttachmentInfo diffuseAttachmentInfo = {};
	diffuseAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	diffuseAttachmentInfo.pNext = nullptr;
	diffuseAttachmentInfo.imageView = m_diffuse.view;
	diffuseAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	diffuseAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
	diffuseAttachmentInfo.resolveImageView = VK_NULL_HANDLE;
	diffuseAttachmentInfo.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	diffuseAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	diffuseAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	diffuseAttachmentInfo.clearValue.color = { 0.0f, 0.0f, 0.0f, 0.0f };

	VkRenderingAttachmentInfo materialAttachmentInfo = {};
	materialAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	materialAttachmentInfo.pNext = nullptr;
	materialAttachmentInfo.imageView = m_material.view;
	materialAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	materialAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
	materialAttachmentInfo.resolveImageView = VK_NULL_HANDLE;
	materialAttachmentInfo.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	materialAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	materialAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	materialAttachmentInfo.clearValue.color = { 0.0f, 0.0f, 0.0f, 0.0f };

	VkRenderingAttachmentInfo emissiveAttachmentInfo = {};
	emissiveAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	emissiveAttachmentInfo.pNext = nullptr;
	emissiveAttachmentInfo.imageView = m_emissive.view;
	emissiveAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	emissiveAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
	emissiveAttachmentInfo.resolveImageView = VK_NULL_HANDLE;
	emissiveAttachmentInfo.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	emissiveAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	emissiveAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	emissiveAttachmentInfo.clearValue.color = { 0.0f, 0.0f, 0.0f, 0.0f };

	VkRenderingAttachmentInfo depthAttachmentInfo = {};
	depthAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	depthAttachmentInfo.pNext = nullptr;
	depthAttachmentInfo.imageView = m_depth.view;
	depthAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depthAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
	depthAttachmentInfo.resolveImageView = VK_NULL_HANDLE;
	depthAttachmentInfo.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	depthAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	depthAttachmentInfo.clearValue.depthStencil = { 1.0f, 0 };

	std::array<VkRenderingAttachmentInfo, 5> gBufferAttachmentInfos = { positionAttachmentInfo, normalAttachmentInfo, diffuseAttachmentInfo, materialAttachmentInfo, emissiveAttachmentInfo };
	VkRenderingInfo gBufferRenderingInfo = {};
	gBufferRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	gBufferRenderingInfo.pNext = nullptr;
	gBufferRenderingInfo.flags = 0;
	gBufferRenderingInfo.renderArea = m_scissor;
	gBufferRenderingInfo.layerCount = 1;
	gBufferRenderingInfo.viewMask = 0;
	gBufferRenderingInfo.colorAttachmentCount = static_cast<uint32_t>(gBufferAttachmentInfos.size());
	gBufferRenderingInfo.pColorAttachments = gBufferAttachmentInfos.data();
	gBufferRenderingInfo.pDepthAttachment = &depthAttachmentInfo;
	gBufferRenderingInfo.pStencilAttachment = nullptr;
	m_vkCmdBeginRenderingKHR(commandBuffer, &gBufferRenderingInfo);

	// Bind vertex and index buffers
	VkDeviceSize vertexBufferOffset = 0;
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer.handle, &vertexBufferOffset);
	vkCmdBindIndexBuffer(commandBuffer, indexBuffer.handle, 0, VK_INDEX_TYPE_UINT32);

	// Bind descriptor set 0
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipelineLayout, 0, 1, &m_descriptorSets[currentFrameInFlight], 0, nullptr);

	// Bind graphics pipeline
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
	vkCmdSetViewport(commandBuffer, 0, 1, &m_viewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &m_scissor);

	m_vkCmdDrawIndexedIndirectCountKHR(commandBuffer, drawIndirectBuffer.handle, sizeof(uint32_t), drawIndirectBuffer.handle, 0, drawIndirectCount, sizeof(VkDrawIndexedIndirectCommand));

	// End G-Buffer rendering
	m_vkCmdEndRenderingKHR(commandBuffer);

	// G-Buffer layout transition
	VkImageMemoryBarrier2 positionColorAttachmentToFragmentImageMemoryBarrier = {};
	positionColorAttachmentToFragmentImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	positionColorAttachmentToFragmentImageMemoryBarrier.pNext = nullptr;
	positionColorAttachmentToFragmentImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	positionColorAttachmentToFragmentImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	positionColorAttachmentToFragmentImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	positionColorAttachmentToFragmentImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	positionColorAttachmentToFragmentImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	positionColorAttachmentToFragmentImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	positionColorAttachmentToFragmentImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	positionColorAttachmentToFragmentImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	positionColorAttachmentToFragmentImageMemoryBarrier.image = m_position.handle;
	positionColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	positionColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	positionColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.levelCount = 1;
	positionColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	positionColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkImageMemoryBarrier2 normalColorAttachmentToFragmentImageMemoryBarrier = {};
	normalColorAttachmentToFragmentImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	normalColorAttachmentToFragmentImageMemoryBarrier.pNext = nullptr;
	normalColorAttachmentToFragmentImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	normalColorAttachmentToFragmentImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	normalColorAttachmentToFragmentImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	normalColorAttachmentToFragmentImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	normalColorAttachmentToFragmentImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	normalColorAttachmentToFragmentImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	normalColorAttachmentToFragmentImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	normalColorAttachmentToFragmentImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	normalColorAttachmentToFragmentImageMemoryBarrier.image = m_normal.handle;
	normalColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	normalColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	normalColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.levelCount = 1;
	normalColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	normalColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkImageMemoryBarrier2 diffuseColorAttachmentToFragmentImageMemoryBarrier = {};
	diffuseColorAttachmentToFragmentImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	diffuseColorAttachmentToFragmentImageMemoryBarrier.pNext = nullptr;
	diffuseColorAttachmentToFragmentImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	diffuseColorAttachmentToFragmentImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	diffuseColorAttachmentToFragmentImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	diffuseColorAttachmentToFragmentImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	diffuseColorAttachmentToFragmentImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	diffuseColorAttachmentToFragmentImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	diffuseColorAttachmentToFragmentImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	diffuseColorAttachmentToFragmentImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	diffuseColorAttachmentToFragmentImageMemoryBarrier.image = m_diffuse.handle;
	diffuseColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	diffuseColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	diffuseColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.levelCount = 1;
	diffuseColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	diffuseColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkImageMemoryBarrier2 materialColorAttachmentToFragmentImageMemoryBarrier = {};
	materialColorAttachmentToFragmentImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	materialColorAttachmentToFragmentImageMemoryBarrier.pNext = nullptr;
	materialColorAttachmentToFragmentImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	materialColorAttachmentToFragmentImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	materialColorAttachmentToFragmentImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	materialColorAttachmentToFragmentImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	materialColorAttachmentToFragmentImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	materialColorAttachmentToFragmentImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	materialColorAttachmentToFragmentImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	materialColorAttachmentToFragmentImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	materialColorAttachmentToFragmentImageMemoryBarrier.image = m_material.handle;
	materialColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	materialColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	materialColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.levelCount = 1;
	materialColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	materialColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkImageMemoryBarrier2 emissiveColorAttachmentToFragmentImageMemoryBarrier = {};
	emissiveColorAttachmentToFragmentImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	emissiveColorAttachmentToFragmentImageMemoryBarrier.pNext = nullptr;
	emissiveColorAttachmentToFragmentImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	emissiveColorAttachmentToFragmentImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	emissiveColorAttachmentToFragmentImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
	emissiveColorAttachmentToFragmentImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
	emissiveColorAttachmentToFragmentImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	emissiveColorAttachmentToFragmentImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	emissiveColorAttachmentToFragmentImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	emissiveColorAttachmentToFragmentImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	emissiveColorAttachmentToFragmentImageMemoryBarrier.image = m_emissive.handle;
	emissiveColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	emissiveColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	emissiveColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.levelCount = 1;
	emissiveColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	emissiveColorAttachmentToFragmentImageMemoryBarrier.subresourceRange.layerCount = 1;

	std::array<VkImageMemoryBarrier2, 5> afterRenderImageMemoryBarriers = { positionColorAttachmentToFragmentImageMemoryBarrier, normalColorAttachmentToFragmentImageMemoryBarrier, diffuseColorAttachmentToFragmentImageMemoryBarrier, materialColorAttachmentToFragmentImageMemoryBarrier, emissiveColorAttachmentToFragmentImageMemoryBarrier };
	VkDependencyInfo afterRenderDependencyInfo = {};
	afterRenderDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	afterRenderDependencyInfo.pNext = nullptr;
	afterRenderDependencyInfo.dependencyFlags = 0;
	afterRenderDependencyInfo.memoryBarrierCount = 0;
	afterRenderDependencyInfo.pMemoryBarriers = nullptr;
	afterRenderDependencyInfo.bufferMemoryBarrierCount = 0;
	afterRenderDependencyInfo.pBufferMemoryBarriers = nullptr;
	afterRenderDependencyInfo.imageMemoryBarrierCount = static_cast<uint32_t>(afterRenderImageMemoryBarriers.size());
	afterRenderDependencyInfo.pImageMemoryBarriers = afterRenderImageMemoryBarriers.data();
	m_vkCmdPipelineBarrier2KHR(commandBuffer, &afterRenderDependencyInfo);
}

void GBuffer::descriptorSetNeedsUpdate(uint32_t frameInFlight) {
	m_descriptorSetsNeedUpdate[frameInFlight] = true;
}

void GBuffer::updateDescriptorSets(uint32_t frameInFlight,
	const std::vector<InternalTexture>& textures,
	const std::vector<VkImageView>& textureImageViews,
	const std::unordered_map<std::string, VkSampler>& textureSamplers) {
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

void GBuffer::onResize(uint32_t width, uint32_t height) {
	m_viewport.width = static_cast<float>(width);
	m_viewport.height = static_cast<float>(height);
	m_scissor.extent.width = width;
	m_scissor.extent.height = height;

	destroyImages();
	createImages(width, height);
}

VulkanImage& GBuffer::getPosition() {
	return m_position;
}

VulkanImage& GBuffer::getNormal() {
	return m_normal;
}

VulkanImage& GBuffer::getDiffuse() {
	return m_diffuse;
}

VulkanImage& GBuffer::getMaterial() {
	return m_material;
}

VulkanImage& GBuffer::getEmissive() {
	return m_emissive;
}

void GBuffer::createImages(uint32_t width, uint32_t height) {
	VkExtent3D imageExtent;
	imageExtent.width = width;
	imageExtent.height = height;
	imageExtent.depth = 1;

	VkImageCreateInfo gBufferCreateInfo = {};
	gBufferCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	gBufferCreateInfo.pNext = nullptr;
	gBufferCreateInfo.flags = 0;
	gBufferCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	gBufferCreateInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	gBufferCreateInfo.extent = imageExtent;
	gBufferCreateInfo.mipLevels = 1;
	gBufferCreateInfo.arrayLayers = 1;
	gBufferCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	gBufferCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	gBufferCreateInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
	gBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	gBufferCreateInfo.queueFamilyIndexCount = 1;
	gBufferCreateInfo.pQueueFamilyIndices = &m_graphicsQueueFamilyIndex;
	gBufferCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	VmaAllocationCreateInfo gBufferAllocationCreateInfo = {};
	gBufferAllocationCreateInfo.flags = 0;
	gBufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	NTSHENGN_VK_CHECK(vmaCreateImage(m_allocator, &gBufferCreateInfo, &gBufferAllocationCreateInfo, &m_position.handle, &m_position.allocation, nullptr));
	NTSHENGN_VK_CHECK(vmaCreateImage(m_allocator, &gBufferCreateInfo, &gBufferAllocationCreateInfo, &m_normal.handle, &m_normal.allocation, nullptr));

	gBufferCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
	NTSHENGN_VK_CHECK(vmaCreateImage(m_allocator, &gBufferCreateInfo, &gBufferAllocationCreateInfo, &m_diffuse.handle, &m_diffuse.allocation, nullptr));

	gBufferCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	NTSHENGN_VK_CHECK(vmaCreateImage(m_allocator, &gBufferCreateInfo, &gBufferAllocationCreateInfo, &m_material.handle, &m_material.allocation, nullptr));

	gBufferCreateInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	NTSHENGN_VK_CHECK(vmaCreateImage(m_allocator, &gBufferCreateInfo, &gBufferAllocationCreateInfo, &m_emissive.handle, &m_emissive.allocation, nullptr));

	gBufferCreateInfo.format = VK_FORMAT_D32_SFLOAT;
	gBufferCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	NTSHENGN_VK_CHECK(vmaCreateImage(m_allocator, &gBufferCreateInfo, &gBufferAllocationCreateInfo, &m_depth.handle, &m_depth.allocation, nullptr));

	VkImageViewCreateInfo gBufferViewCreateInfo = {};
	gBufferViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	gBufferViewCreateInfo.pNext = nullptr;
	gBufferViewCreateInfo.flags = 0;
	gBufferViewCreateInfo.image = m_position.handle;
	gBufferViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	gBufferViewCreateInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	gBufferViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
	gBufferViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
	gBufferViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
	gBufferViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
	gBufferViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	gBufferViewCreateInfo.subresourceRange.baseMipLevel = 0;
	gBufferViewCreateInfo.subresourceRange.levelCount = 1;
	gBufferViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	gBufferViewCreateInfo.subresourceRange.layerCount = 1;
	NTSHENGN_VK_CHECK(vkCreateImageView(m_device, &gBufferViewCreateInfo, nullptr, &m_position.view));

	gBufferViewCreateInfo.image = m_normal.handle;
	NTSHENGN_VK_CHECK(vkCreateImageView(m_device, &gBufferViewCreateInfo, nullptr, &m_normal.view));

	gBufferViewCreateInfo.image = m_diffuse.handle;
	gBufferViewCreateInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
	NTSHENGN_VK_CHECK(vkCreateImageView(m_device, &gBufferViewCreateInfo, nullptr, &m_diffuse.view));

	gBufferViewCreateInfo.image = m_material.handle;
	gBufferViewCreateInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
	NTSHENGN_VK_CHECK(vkCreateImageView(m_device, &gBufferViewCreateInfo, nullptr, &m_material.view));

	gBufferViewCreateInfo.image = m_emissive.handle;
	gBufferViewCreateInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
	NTSHENGN_VK_CHECK(vkCreateImageView(m_device, &gBufferViewCreateInfo, nullptr, &m_emissive.view));

	gBufferViewCreateInfo.image = m_depth.handle;
	gBufferViewCreateInfo.format = VK_FORMAT_D32_SFLOAT;
	gBufferViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	NTSHENGN_VK_CHECK(vkCreateImageView(m_device, &gBufferViewCreateInfo, nullptr, &m_depth.view));

	// Layout transition VK_IMAGE_LAYOUT_UNDEFINED -> VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL and VK_IMAGE_LAYOUT_UNDEFINED -> VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL
	NTSHENGN_VK_CHECK(vkResetCommandPool(m_device, m_initializationCommandPool, 0));

	VkCommandBufferBeginInfo commandBufferBeginInfo = {};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.pNext = nullptr;
	commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	commandBufferBeginInfo.pInheritanceInfo = nullptr;
	NTSHENGN_VK_CHECK(vkBeginCommandBuffer(m_initializationCommandBuffer, &commandBufferBeginInfo));

	VkImageMemoryBarrier2 gBufferPositionImageMemoryBarrier = {};
	gBufferPositionImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	gBufferPositionImageMemoryBarrier.pNext = nullptr;
	gBufferPositionImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
	gBufferPositionImageMemoryBarrier.srcAccessMask = 0;
	gBufferPositionImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	gBufferPositionImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	gBufferPositionImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	gBufferPositionImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	gBufferPositionImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	gBufferPositionImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	gBufferPositionImageMemoryBarrier.image = m_position.handle;
	gBufferPositionImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	gBufferPositionImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	gBufferPositionImageMemoryBarrier.subresourceRange.levelCount = 1;
	gBufferPositionImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	gBufferPositionImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkImageMemoryBarrier2 gBufferNormalImageMemoryBarrier = {};
	gBufferNormalImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	gBufferNormalImageMemoryBarrier.pNext = nullptr;
	gBufferNormalImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
	gBufferNormalImageMemoryBarrier.srcAccessMask = 0;
	gBufferNormalImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	gBufferNormalImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	gBufferNormalImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	gBufferNormalImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	gBufferNormalImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	gBufferNormalImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	gBufferNormalImageMemoryBarrier.image = m_normal.handle;
	gBufferNormalImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	gBufferNormalImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	gBufferNormalImageMemoryBarrier.subresourceRange.levelCount = 1;
	gBufferNormalImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	gBufferNormalImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkImageMemoryBarrier2 gBufferDiffuseImageMemoryBarrier = {};
	gBufferDiffuseImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	gBufferDiffuseImageMemoryBarrier.pNext = nullptr;
	gBufferDiffuseImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
	gBufferDiffuseImageMemoryBarrier.srcAccessMask = 0;
	gBufferDiffuseImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	gBufferDiffuseImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	gBufferDiffuseImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	gBufferDiffuseImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	gBufferDiffuseImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	gBufferDiffuseImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	gBufferDiffuseImageMemoryBarrier.image = m_diffuse.handle;
	gBufferDiffuseImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	gBufferDiffuseImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	gBufferDiffuseImageMemoryBarrier.subresourceRange.levelCount = 1;
	gBufferDiffuseImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	gBufferDiffuseImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkImageMemoryBarrier2 gBufferMaterialImageMemoryBarrier = {};
	gBufferMaterialImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	gBufferMaterialImageMemoryBarrier.pNext = nullptr;
	gBufferMaterialImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
	gBufferMaterialImageMemoryBarrier.srcAccessMask = 0;
	gBufferMaterialImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	gBufferMaterialImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	gBufferMaterialImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	gBufferMaterialImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	gBufferMaterialImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	gBufferMaterialImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	gBufferMaterialImageMemoryBarrier.image = m_material.handle;
	gBufferMaterialImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	gBufferMaterialImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	gBufferMaterialImageMemoryBarrier.subresourceRange.levelCount = 1;
	gBufferMaterialImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	gBufferMaterialImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkImageMemoryBarrier2 gBufferEmissiveImageMemoryBarrier = {};
	gBufferEmissiveImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	gBufferEmissiveImageMemoryBarrier.pNext = nullptr;
	gBufferEmissiveImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
	gBufferEmissiveImageMemoryBarrier.srcAccessMask = 0;
	gBufferEmissiveImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	gBufferEmissiveImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	gBufferEmissiveImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	gBufferEmissiveImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	gBufferEmissiveImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	gBufferEmissiveImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	gBufferEmissiveImageMemoryBarrier.image = m_emissive.handle;
	gBufferEmissiveImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	gBufferEmissiveImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	gBufferEmissiveImageMemoryBarrier.subresourceRange.levelCount = 1;
	gBufferEmissiveImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	gBufferEmissiveImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkImageMemoryBarrier2 depthImageMemoryBarrier = {};
	depthImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	depthImageMemoryBarrier.pNext = nullptr;
	depthImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
	depthImageMemoryBarrier.srcAccessMask = 0;
	depthImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
	depthImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	depthImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	depthImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depthImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	depthImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsQueueFamilyIndex;
	depthImageMemoryBarrier.image = m_depth.handle;
	depthImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	depthImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	depthImageMemoryBarrier.subresourceRange.levelCount = 1;
	depthImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	depthImageMemoryBarrier.subresourceRange.layerCount = 1;

	std::array<VkImageMemoryBarrier2, 6> imageMemoryBarriers = { gBufferPositionImageMemoryBarrier, gBufferNormalImageMemoryBarrier, gBufferDiffuseImageMemoryBarrier, gBufferMaterialImageMemoryBarrier, gBufferEmissiveImageMemoryBarrier, depthImageMemoryBarrier };
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

void GBuffer::destroyImages() {
	m_depth.destroy(m_device, m_allocator);
	m_emissive.destroy(m_device, m_allocator);
	m_material.destroy(m_device, m_allocator);
	m_diffuse.destroy(m_device, m_allocator);
	m_normal.destroy(m_device, m_allocator);
	m_position.destroy(m_device, m_allocator);
}

void GBuffer::createDescriptorSetLayout() {
	VkDescriptorSetLayoutBinding perDrawDescriptorSetLayoutBinding = {};
	perDrawDescriptorSetLayoutBinding.binding = 0;
	perDrawDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	perDrawDescriptorSetLayoutBinding.descriptorCount = 1;
	perDrawDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	perDrawDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding cameraDescriptorSetLayoutBinding = {};
	cameraDescriptorSetLayoutBinding.binding = 1;
	cameraDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	cameraDescriptorSetLayoutBinding.descriptorCount = 1;
	cameraDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	cameraDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding objectsDescriptorSetLayoutBinding = {};
	objectsDescriptorSetLayoutBinding.binding = 2;
	objectsDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	objectsDescriptorSetLayoutBinding.descriptorCount = 1;
	objectsDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	objectsDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding meshesDescriptorSetLayoutBinding = {};
	meshesDescriptorSetLayoutBinding.binding = 3;
	meshesDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	meshesDescriptorSetLayoutBinding.descriptorCount = 1;
	meshesDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	meshesDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding jointTransformsDescriptorSetLayoutBinding = {};
	jointTransformsDescriptorSetLayoutBinding.binding = 4;
	jointTransformsDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	jointTransformsDescriptorSetLayoutBinding.descriptorCount = 1;
	jointTransformsDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	jointTransformsDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding materialsDescriptorSetLayoutBinding = {};
	materialsDescriptorSetLayoutBinding.binding = 5;
	materialsDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	materialsDescriptorSetLayoutBinding.descriptorCount = 1;
	materialsDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	materialsDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding texturesDescriptorSetLayoutBinding = {};
	texturesDescriptorSetLayoutBinding.binding = 6;
	texturesDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	texturesDescriptorSetLayoutBinding.descriptorCount = 131072;
	texturesDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	texturesDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	std::array<VkDescriptorBindingFlags, 7> descriptorBindingFlags = { 0, 0, 0, 0, 0, 0, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT };
	VkDescriptorSetLayoutBindingFlagsCreateInfo descriptorSetLayoutBindingFlagsCreateInfo = {};
	descriptorSetLayoutBindingFlagsCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	descriptorSetLayoutBindingFlagsCreateInfo.pNext = nullptr;
	descriptorSetLayoutBindingFlagsCreateInfo.bindingCount = static_cast<uint32_t>(descriptorBindingFlags.size());
	descriptorSetLayoutBindingFlagsCreateInfo.pBindingFlags = descriptorBindingFlags.data();

	std::array<VkDescriptorSetLayoutBinding, 7> descriptorSetLayoutBindings = { perDrawDescriptorSetLayoutBinding, cameraDescriptorSetLayoutBinding, objectsDescriptorSetLayoutBinding, meshesDescriptorSetLayoutBinding, jointTransformsDescriptorSetLayoutBinding, materialsDescriptorSetLayoutBinding, texturesDescriptorSetLayoutBinding };
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.pNext = &descriptorSetLayoutBindingFlagsCreateInfo;
	descriptorSetLayoutCreateInfo.flags = 0;
	descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(descriptorSetLayoutBindings.size());
	descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings.data();
	NTSHENGN_VK_CHECK(vkCreateDescriptorSetLayout(m_device, &descriptorSetLayoutCreateInfo, nullptr, &m_descriptorSetLayout));
}

void GBuffer::createGraphicsPipeline() {
	std::vector<VkFormat> pipelineRenderingColorFormats = { VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R16G16B16A16_SFLOAT };
	VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {};
	pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	pipelineRenderingCreateInfo.pNext = nullptr;
	pipelineRenderingCreateInfo.viewMask = 0;
	pipelineRenderingCreateInfo.colorAttachmentCount = static_cast<uint32_t>(pipelineRenderingColorFormats.size());
	pipelineRenderingCreateInfo.pColorAttachmentFormats = pipelineRenderingColorFormats.data();
	pipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;
	pipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

	const std::string vertexShaderCode = R"GLSL(
		#version 460

		struct PerDrawInfo {
			uint objectID;
		};

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

		layout(std430, set = 0, binding = 0) restrict readonly buffer PerDraw {
			PerDrawInfo info[];
		} perDraw;

		layout(set = 0, binding = 1) uniform Camera {
			mat4 view;
			mat4 projection;
			vec3 position;
		} camera;

		layout(std430, set = 0, binding = 2) restrict readonly buffer Objects {
			ObjectInfo info[];
		} objects;

		layout(std430, set = 0, binding = 3) restrict readonly buffer Meshes {
			MeshInfo info[];
		} meshes;

		layout(set = 0, binding = 4) restrict readonly buffer JointTransforms {
			mat4 matrix[];
		} jointTransforms;

		layout(location = 0) in vec3 position;
		layout(location = 1) in vec3 normal;
		layout(location = 2) in vec2 uv;
		layout(location = 3) in vec3 color;
		layout(location = 4) in vec4 tangent;
		layout(location = 5) in uvec4 joints;
		layout(location = 6) in vec4 weights;

		layout(location = 0) out vec3 outPosition;
		layout(location = 1) out vec2 outUV;
		layout(location = 2) out flat uint outMaterialID;
		layout(location = 3) out mat3 outTBN;

		void main() {
			uint objectID = perDraw.info[gl_DrawID].objectID;

			mat4 skinMatrix = mat4(1.0);
			if (meshes.info[objects.info[objectID].meshID].hasSkin == 1) {
				uint jointTransformOffset = objects.info[objectID].jointTransformOffset;

				skinMatrix = (weights.x * jointTransforms.matrix[jointTransformOffset + joints.x]) +
					(weights.y * jointTransforms.matrix[jointTransformOffset + joints.y]) +
					(weights.z * jointTransforms.matrix[jointTransformOffset + joints.z]) +
					(weights.w * jointTransforms.matrix[jointTransformOffset + joints.w]);
			}
			outPosition = vec3(objects.info[objectID].model * skinMatrix * vec4(position, 1.0));
			outUV = uv;
			outMaterialID = objects.info[objectID].materialID;

			vec3 skinnedNormal = vec3(transpose(inverse(skinMatrix)) * vec4(normal, 0.0));
			vec3 skinnedTangent = vec3(skinMatrix * vec4(tangent.xyz, 0.0));
			vec3 bitangent = cross(skinnedNormal, skinnedTangent) * tangent.w;
			vec3 T = vec3(objects.info[objectID].transposeInverseModel * vec4(skinnedTangent, 0.0));
			vec3 B = vec3(objects.info[objectID].transposeInverseModel * vec4(bitangent, 0.0));
			vec3 N = vec3(objects.info[objectID].transposeInverseModel * vec4(skinnedNormal, 0.0));
			outTBN = mat3(T, B, N);

			gl_Position = camera.projection * camera.view * vec4(outPosition, 1.0);
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
		#extension GL_EXT_nonuniform_qualifier : enable

		const mat4 ditheringThreshold = mat4(
			1.0 / 17.0, 13.0 / 17.0, 4.0 / 17.0, 16.0 / 17.0,
			9.0 / 17.0, 5.0 / 17.0, 12.0 / 17.0, 8.0 / 17.0,
			3.0 / 17.0, 15.0 / 17.0, 2.0 / 17.0, 14.0 / 17.0,
			11.0 / 17.0, 7.0 / 17.0, 10.0 / 17.0, 6.0 / 17.0
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
		};

		layout(set = 0, binding = 5) restrict readonly buffer Materials {
			MaterialInfo info[];
		} materials;

		layout(set = 0, binding = 6) uniform sampler2D textures[];

		layout(location = 0) in vec3 position;
		layout(location = 1) in vec2 uv;
		layout(location = 2) in flat uint materialID;
		layout(location = 3) in mat3 TBN;

		layout(location = 0) out vec4 outGBufferPosition;
		layout(location = 1) out vec4 outGBufferNormal;
		layout(location = 2) out vec4 outGBufferDiffuse;
		layout(location = 3) out vec4 outGBufferMaterial;
		layout(location = 4) out vec4 outGBufferEmissive;

		void main() {
			const MaterialInfo material = materials.info[materialID];
			const vec4 diffuseSample = texture(textures[nonuniformEXT(material.diffuseTextureIndex)], uv);
			if ((diffuseSample.a < material.alphaCutoff) || (diffuseSample.a < ditheringThreshold[int(mod(gl_FragCoord.x, 4.0))][int(mod(gl_FragCoord.y, 4.0))])) {
				discard;
			}

			outGBufferPosition = vec4(position, 1.0);

			const vec3 normalSample = texture(textures[nonuniformEXT(material.normalTextureIndex)], uv).xyz;
			const vec3 n = normalize(TBN * (normalSample * 2.0 - 1.0));
			outGBufferNormal = vec4(n, 0.0);

			outGBufferDiffuse = diffuseSample;
			
			if (material.occlusionTextureIndex == material.roughnessTextureIndex &&
				material.roughnessTextureIndex == material.metalnessTextureIndex) {
				const vec3 materialSample = texture(textures[nonuniformEXT(material.occlusionTextureIndex)], uv).rgb;
				outGBufferMaterial = vec4(materialSample, material.alphaCutoff);
			}
			else {
				const float occlusionSample = texture(textures[nonuniformEXT(material.occlusionTextureIndex)], uv).r;
				const float roughnessSample = texture(textures[nonuniformEXT(material.roughnessTextureIndex)], uv).g;
				const float metalnessSample = texture(textures[nonuniformEXT(material.metalnessTextureIndex)], uv).b;
				outGBufferMaterial = vec4(occlusionSample, roughnessSample, metalnessSample, material.alphaCutoff);
			}

			outGBufferEmissive = texture(textures[nonuniformEXT(material.emissiveTextureIndex)], uv) * material.emissiveFactor;
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
	vertexColorInputAttributeDescription.format = VK_FORMAT_R32G32B32_SFLOAT;
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
	depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
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

	std::array<VkPipelineColorBlendAttachmentState, 5> colorBlendAttachmentStates = { colorBlendAttachmentState, colorBlendAttachmentState, colorBlendAttachmentState, colorBlendAttachmentState, colorBlendAttachmentState };
	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {};
	colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendStateCreateInfo.pNext = nullptr;
	colorBlendStateCreateInfo.flags = 0;
	colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
	colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	colorBlendStateCreateInfo.attachmentCount = static_cast<uint32_t>(colorBlendAttachmentStates.size());
	colorBlendStateCreateInfo.pAttachments = colorBlendAttachmentStates.data();

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

void GBuffer::createDescriptorSets(const std::vector<VkBuffer>& perDrawBuffers,
	const std::vector<HostVisibleVulkanBuffer>& cameraBuffers,
	const std::vector<HostVisibleVulkanBuffer>& objectBuffers,
	VulkanBuffer meshBuffer,
	const std::vector<HostVisibleVulkanBuffer>& jointTransformBuffers,
	const std::vector<HostVisibleVulkanBuffer>& materialBuffers) {
	// Create descriptor pool
  	VkDescriptorPoolSize perDrawDescriptorPoolSize = {};
	perDrawDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	perDrawDescriptorPoolSize.descriptorCount = m_framesInFlight;

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

	VkDescriptorPoolSize texturesDescriptorPoolSize = {};
	texturesDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	texturesDescriptorPoolSize.descriptorCount = 131072 * m_framesInFlight;

	std::array<VkDescriptorPoolSize, 7> descriptorPoolSizes = { perDrawDescriptorPoolSize, cameraDescriptorPoolSize, objectsDescriptorPoolSize, meshesDescriptorPoolSize, jointTransformsDescriptorPoolSize, materialsDescriptorPoolSize, texturesDescriptorPoolSize };
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

		VkDescriptorBufferInfo perDrawDescriptorBufferInfo;
		perDrawDescriptorBufferInfo.buffer = perDrawBuffers[i];
		perDrawDescriptorBufferInfo.offset = 0;
		perDrawDescriptorBufferInfo.range = 32768;

		VkWriteDescriptorSet perDrawDescriptorWriteDescriptorSet = {};
		perDrawDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		perDrawDescriptorWriteDescriptorSet.pNext = nullptr;
		perDrawDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		perDrawDescriptorWriteDescriptorSet.dstBinding = 0;
		perDrawDescriptorWriteDescriptorSet.dstArrayElement = 0;
		perDrawDescriptorWriteDescriptorSet.descriptorCount = 1;
		perDrawDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		perDrawDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		perDrawDescriptorWriteDescriptorSet.pBufferInfo = &perDrawDescriptorBufferInfo;
		perDrawDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;
		writeDescriptorSets.push_back(perDrawDescriptorWriteDescriptorSet);

		VkDescriptorBufferInfo cameraDescriptorBufferInfo;
		cameraDescriptorBufferInfo.buffer = cameraBuffers[i].handle;
		cameraDescriptorBufferInfo.offset = 0;
		cameraDescriptorBufferInfo.range = sizeof(NtshEngn::Math::mat4) * 2 + sizeof(NtshEngn::Math::vec4);

		VkWriteDescriptorSet cameraDescriptorWriteDescriptorSet = {};
		cameraDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		cameraDescriptorWriteDescriptorSet.pNext = nullptr;
		cameraDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		cameraDescriptorWriteDescriptorSet.dstBinding = 1;
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
		objectsDescriptorBufferInfo.range = 262144;

		VkWriteDescriptorSet objectsDescriptorWriteDescriptorSet = {};
		objectsDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		objectsDescriptorWriteDescriptorSet.pNext = nullptr;
		objectsDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		objectsDescriptorWriteDescriptorSet.dstBinding = 2;
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
		meshesDescriptorWriteDescriptorSet.dstBinding = 3;
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
		jointTransformsDescriptorWriteDescriptorSet.dstBinding = 4;
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
		materialsDescriptorWriteDescriptorSet.dstBinding = 5;
		materialsDescriptorWriteDescriptorSet.dstArrayElement = 0;
		materialsDescriptorWriteDescriptorSet.descriptorCount = 1;
		materialsDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		materialsDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		materialsDescriptorWriteDescriptorSet.pBufferInfo = &materialsDescriptorBufferInfo;
		materialsDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;
		writeDescriptorSets.push_back(materialsDescriptorWriteDescriptorSet);

		vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}

	m_descriptorSetsNeedUpdate.resize(m_framesInFlight);
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		m_descriptorSetsNeedUpdate[i] = false;
	}
}