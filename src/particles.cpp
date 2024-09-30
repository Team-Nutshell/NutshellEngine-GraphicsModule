#include "particles.h"
#include <random>

void Particles::init(VkDevice device,
	VkQueue graphicsComputeQueue,
	uint32_t graphicsComputeQueueFamilyIndex,
	VmaAllocator allocator,
	VkFormat drawImageFormat,
	VkCommandPool initializationCommandPool,
	VkCommandBuffer initializationCommandBuffer,
	VkFence initializationFence,
	VkViewport viewport,
	VkRect2D scissor,
	uint32_t framesInFlight,
	const std::vector<HostVisibleVulkanBuffer>& cameraBuffers,
	PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR,
	PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR,
	PFN_vkCmdPipelineBarrier2KHR vkCmdPipelineBarrier2KHR) {
	m_device = device;
	m_graphicsComputeQueue = graphicsComputeQueue;
	m_graphicsComputeQueueFamilyIndex = graphicsComputeQueueFamilyIndex;
	m_initializationCommandPool = initializationCommandPool;
	m_initializationCommandBuffer = initializationCommandBuffer;
	m_initializationFence = initializationFence;
	m_allocator = allocator;
	m_viewport = viewport;
	m_scissor = scissor;
	m_framesInFlight = framesInFlight;
	m_vkCmdBeginRenderingKHR = vkCmdBeginRenderingKHR;
	m_vkCmdEndRenderingKHR = vkCmdEndRenderingKHR;
	m_vkCmdPipelineBarrier2KHR = vkCmdPipelineBarrier2KHR;

	createBuffers();
	createComputeResources();
	createGraphicsResources(drawImageFormat, cameraBuffers);
}

void Particles::destroy() {
	vkDestroyDescriptorPool(m_device, m_graphicsDescriptorPool, nullptr);
	vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_graphicsPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(m_device, m_graphicsDescriptorSetLayout, nullptr);

	vkDestroyDescriptorPool(m_device, m_computeDescriptorPool, nullptr);
	vkDestroyPipeline(m_device, m_computePipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_computePipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(m_device, m_computeDescriptorSetLayout, nullptr);

	vmaDestroyBuffer(m_allocator, m_drawIndirectBuffer.handle, m_drawIndirectBuffer.allocation);
	for (size_t i = 0; i < m_framesInFlight; i++) {
		vmaDestroyBuffer(m_allocator, m_stagingBuffers[i].handle, m_stagingBuffers[i].allocation);
	}
	for (size_t i = 0; i < m_particleBuffers.size(); i++) {
		vmaDestroyBuffer(m_allocator, m_particleBuffers[i].handle, m_particleBuffers[i].allocation);
	}
}

void Particles::draw(VkCommandBuffer commandBuffer, VkImage drawImage, VkImageView drawImageView, VkImage depthImage, VkImageView depthImageView, uint32_t currentFrameInFlight, float dt) {
	// Synchronization before particles
	VkImageMemoryBarrier2 colorAttachmentBeforeParticlesImageMemoryBarrier = {};
	colorAttachmentBeforeParticlesImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	colorAttachmentBeforeParticlesImageMemoryBarrier.pNext = nullptr;
	colorAttachmentBeforeParticlesImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	colorAttachmentBeforeParticlesImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	colorAttachmentBeforeParticlesImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
	colorAttachmentBeforeParticlesImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
	colorAttachmentBeforeParticlesImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachmentBeforeParticlesImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachmentBeforeParticlesImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	colorAttachmentBeforeParticlesImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	colorAttachmentBeforeParticlesImageMemoryBarrier.image = drawImage;
	colorAttachmentBeforeParticlesImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	colorAttachmentBeforeParticlesImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	colorAttachmentBeforeParticlesImageMemoryBarrier.subresourceRange.levelCount = 1;
	colorAttachmentBeforeParticlesImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	colorAttachmentBeforeParticlesImageMemoryBarrier.subresourceRange.layerCount = 1;

	VkImageMemoryBarrier2 depthAttachmentBeforeParticlesImageMemoryBarrier = {};
	depthAttachmentBeforeParticlesImageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
	depthAttachmentBeforeParticlesImageMemoryBarrier.pNext = nullptr;
	depthAttachmentBeforeParticlesImageMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
	depthAttachmentBeforeParticlesImageMemoryBarrier.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	depthAttachmentBeforeParticlesImageMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
	depthAttachmentBeforeParticlesImageMemoryBarrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	depthAttachmentBeforeParticlesImageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depthAttachmentBeforeParticlesImageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	depthAttachmentBeforeParticlesImageMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	depthAttachmentBeforeParticlesImageMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	depthAttachmentBeforeParticlesImageMemoryBarrier.image = depthImage;
	depthAttachmentBeforeParticlesImageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	depthAttachmentBeforeParticlesImageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	depthAttachmentBeforeParticlesImageMemoryBarrier.subresourceRange.levelCount = 1;
	depthAttachmentBeforeParticlesImageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	depthAttachmentBeforeParticlesImageMemoryBarrier.subresourceRange.layerCount = 1;

	std::array<VkImageMemoryBarrier2, 2> beforeParticlesImageMemoryBarriers = { colorAttachmentBeforeParticlesImageMemoryBarrier, depthAttachmentBeforeParticlesImageMemoryBarrier };
	VkDependencyInfo beforeParticlesDependencyInfo = {};
	beforeParticlesDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	beforeParticlesDependencyInfo.pNext = nullptr;
	beforeParticlesDependencyInfo.dependencyFlags = 0;
	beforeParticlesDependencyInfo.memoryBarrierCount = 0;
	beforeParticlesDependencyInfo.pMemoryBarriers = nullptr;
	beforeParticlesDependencyInfo.bufferMemoryBarrierCount = 0;
	beforeParticlesDependencyInfo.pBufferMemoryBarriers = nullptr;
	beforeParticlesDependencyInfo.imageMemoryBarrierCount = static_cast<uint32_t>(beforeParticlesImageMemoryBarriers.size());
	beforeParticlesDependencyInfo.pImageMemoryBarriers = beforeParticlesImageMemoryBarriers.data();
	m_vkCmdPipelineBarrier2KHR(commandBuffer, &beforeParticlesDependencyInfo);

	// Update particle buffer
	bool particleBufferUpdated = false;
	if (m_particleBuffersNeedUpdate[currentFrameInFlight]) {
		VkBufferMemoryBarrier2 beforeParticleUpdateBufferMemoryBarrier = {};
		beforeParticleUpdateBufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
		beforeParticleUpdateBufferMemoryBarrier.pNext = nullptr;
		beforeParticleUpdateBufferMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
		beforeParticleUpdateBufferMemoryBarrier.srcAccessMask = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
		beforeParticleUpdateBufferMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
		beforeParticleUpdateBufferMemoryBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		beforeParticleUpdateBufferMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
		beforeParticleUpdateBufferMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
		beforeParticleUpdateBufferMemoryBarrier.buffer = m_particleBuffers[m_inParticleBufferCurrentIndex].handle;
		beforeParticleUpdateBufferMemoryBarrier.offset = 0;
		beforeParticleUpdateBufferMemoryBarrier.size = m_maxParticlesNumber * sizeof(Particle);

		VkDependencyInfo beforeParticleUpdateBufferBufferDependencyInfo = {};
		beforeParticleUpdateBufferBufferDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
		beforeParticleUpdateBufferBufferDependencyInfo.pNext = nullptr;
		beforeParticleUpdateBufferBufferDependencyInfo.dependencyFlags = 0;
		beforeParticleUpdateBufferBufferDependencyInfo.memoryBarrierCount = 0;
		beforeParticleUpdateBufferBufferDependencyInfo.pMemoryBarriers = nullptr;
		beforeParticleUpdateBufferBufferDependencyInfo.bufferMemoryBarrierCount = 1;
		beforeParticleUpdateBufferBufferDependencyInfo.pBufferMemoryBarriers = &beforeParticleUpdateBufferMemoryBarrier;
		beforeParticleUpdateBufferBufferDependencyInfo.imageMemoryBarrierCount = 0;
		beforeParticleUpdateBufferBufferDependencyInfo.pImageMemoryBarriers = nullptr;
		m_vkCmdPipelineBarrier2KHR(commandBuffer, &beforeParticleUpdateBufferBufferDependencyInfo);

		// Copy particles staging buffer
		VkBufferCopy particleStagingBufferCopy = {};
		particleStagingBufferCopy.srcOffset = 0;
		particleStagingBufferCopy.dstOffset = (m_maxParticlesNumber * sizeof(Particle)) - m_currentParticleHostSize;
		particleStagingBufferCopy.size = m_currentParticleHostSize;
		vkCmdCopyBuffer(commandBuffer, m_stagingBuffers[currentFrameInFlight].handle, m_particleBuffers[m_inParticleBufferCurrentIndex].handle, 1, &particleStagingBufferCopy);

		m_currentParticleHostSize = 0;

		m_particleBuffersNeedUpdate[currentFrameInFlight] = false;
		particleBufferUpdated = true;
	}

	// Synchronize before fill particle draw indirect buffer
	VkBufferMemoryBarrier2 beforeFillParticleDrawIndirectBufferMemoryBarrier = {};
	beforeFillParticleDrawIndirectBufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	beforeFillParticleDrawIndirectBufferMemoryBarrier.pNext = nullptr;
	beforeFillParticleDrawIndirectBufferMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
	beforeFillParticleDrawIndirectBufferMemoryBarrier.srcAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
	beforeFillParticleDrawIndirectBufferMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	beforeFillParticleDrawIndirectBufferMemoryBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	beforeFillParticleDrawIndirectBufferMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	beforeFillParticleDrawIndirectBufferMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	beforeFillParticleDrawIndirectBufferMemoryBarrier.buffer = m_drawIndirectBuffer.handle;
	beforeFillParticleDrawIndirectBufferMemoryBarrier.offset = 0;
	beforeFillParticleDrawIndirectBufferMemoryBarrier.size = sizeof(uint32_t);

	VkDependencyInfo beforeFillParticleDrawIndirectBufferDependencyInfo = {};
	beforeFillParticleDrawIndirectBufferDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	beforeFillParticleDrawIndirectBufferDependencyInfo.pNext = nullptr;
	beforeFillParticleDrawIndirectBufferDependencyInfo.dependencyFlags = 0;
	beforeFillParticleDrawIndirectBufferDependencyInfo.memoryBarrierCount = 0;
	beforeFillParticleDrawIndirectBufferDependencyInfo.pMemoryBarriers = nullptr;
	beforeFillParticleDrawIndirectBufferDependencyInfo.bufferMemoryBarrierCount = 1;
	beforeFillParticleDrawIndirectBufferDependencyInfo.pBufferMemoryBarriers = &beforeFillParticleDrawIndirectBufferMemoryBarrier;
	beforeFillParticleDrawIndirectBufferDependencyInfo.imageMemoryBarrierCount = 0;
	beforeFillParticleDrawIndirectBufferDependencyInfo.pImageMemoryBarriers = nullptr;
	m_vkCmdPipelineBarrier2KHR(commandBuffer, &beforeFillParticleDrawIndirectBufferDependencyInfo);

	// Fill draw indirect particle buffer
	vkCmdFillBuffer(commandBuffer, m_drawIndirectBuffer.handle, 0, sizeof(uint32_t), 0);

	// Synchronize before particle compute
	VkBufferMemoryBarrier2 beforeDispatchParticleDrawIndirectBufferMemoryBarrier = {};
	beforeDispatchParticleDrawIndirectBufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	beforeDispatchParticleDrawIndirectBufferMemoryBarrier.pNext = nullptr;
	beforeDispatchParticleDrawIndirectBufferMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	beforeDispatchParticleDrawIndirectBufferMemoryBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	beforeDispatchParticleDrawIndirectBufferMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	beforeDispatchParticleDrawIndirectBufferMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	beforeDispatchParticleDrawIndirectBufferMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	beforeDispatchParticleDrawIndirectBufferMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	beforeDispatchParticleDrawIndirectBufferMemoryBarrier.buffer = m_drawIndirectBuffer.handle;
	beforeDispatchParticleDrawIndirectBufferMemoryBarrier.offset = 0;
	beforeDispatchParticleDrawIndirectBufferMemoryBarrier.size = sizeof(uint32_t);

	VkBufferMemoryBarrier2 drawToParticleComputeBufferMemoryBarrier = {};
	drawToParticleComputeBufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	drawToParticleComputeBufferMemoryBarrier.pNext = nullptr;
	if (particleBufferUpdated) {
		drawToParticleComputeBufferMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		drawToParticleComputeBufferMemoryBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	}
	else {
		drawToParticleComputeBufferMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
		drawToParticleComputeBufferMemoryBarrier.srcAccessMask = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
	}
	drawToParticleComputeBufferMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	drawToParticleComputeBufferMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
	drawToParticleComputeBufferMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	drawToParticleComputeBufferMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	drawToParticleComputeBufferMemoryBarrier.buffer = m_particleBuffers[m_inParticleBufferCurrentIndex].handle;
	drawToParticleComputeBufferMemoryBarrier.offset = 0;
	drawToParticleComputeBufferMemoryBarrier.size = m_maxParticlesNumber * sizeof(Particle);

	VkBufferMemoryBarrier2 computeBufferToComputeBufferMemoryBarrier = {};
	computeBufferToComputeBufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	computeBufferToComputeBufferMemoryBarrier.pNext = nullptr;
	computeBufferToComputeBufferMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	computeBufferToComputeBufferMemoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
	computeBufferToComputeBufferMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	computeBufferToComputeBufferMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	computeBufferToComputeBufferMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	computeBufferToComputeBufferMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	computeBufferToComputeBufferMemoryBarrier.buffer = m_particleBuffers[(m_inParticleBufferCurrentIndex + 1) % 2].handle;
	computeBufferToComputeBufferMemoryBarrier.offset = 0;
	computeBufferToComputeBufferMemoryBarrier.size = m_maxParticlesNumber * sizeof(Particle);

	std::array<VkBufferMemoryBarrier2, 3> drawToParticleComputeBufferMemoryBarriers = { beforeDispatchParticleDrawIndirectBufferMemoryBarrier, drawToParticleComputeBufferMemoryBarrier, computeBufferToComputeBufferMemoryBarrier };
	VkDependencyInfo drawToParticleComputeBufferDependencyInfo = {};
	drawToParticleComputeBufferDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	drawToParticleComputeBufferDependencyInfo.pNext = nullptr;
	drawToParticleComputeBufferDependencyInfo.dependencyFlags = 0;
	drawToParticleComputeBufferDependencyInfo.memoryBarrierCount = 0;
	drawToParticleComputeBufferDependencyInfo.pMemoryBarriers = nullptr;
	drawToParticleComputeBufferDependencyInfo.bufferMemoryBarrierCount = static_cast<uint32_t>(drawToParticleComputeBufferMemoryBarriers.size());
	drawToParticleComputeBufferDependencyInfo.pBufferMemoryBarriers = drawToParticleComputeBufferMemoryBarriers.data();
	drawToParticleComputeBufferDependencyInfo.imageMemoryBarrierCount = 0;
	drawToParticleComputeBufferDependencyInfo.pImageMemoryBarriers = nullptr;
	m_vkCmdPipelineBarrier2KHR(commandBuffer, &drawToParticleComputeBufferDependencyInfo);

	// Dispatch particles
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipeline);

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipelineLayout, 0, 1, &m_computeDescriptorSets[m_inParticleBufferCurrentIndex], 0, nullptr);

	vkCmdPushConstants(commandBuffer, m_computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(float), &dt);

	vkCmdDispatch(commandBuffer, ((m_maxParticlesNumber + 64 - 1) / 64), 1, 1);

	// Synchronize before fill in particle
	VkBufferMemoryBarrier2 beforeFillInParticleBufferMemoryBarrier = {};
	beforeFillInParticleBufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	beforeFillInParticleBufferMemoryBarrier.pNext = nullptr;
	beforeFillInParticleBufferMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	beforeFillInParticleBufferMemoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
	beforeFillInParticleBufferMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	beforeFillInParticleBufferMemoryBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	beforeFillInParticleBufferMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	beforeFillInParticleBufferMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	beforeFillInParticleBufferMemoryBarrier.buffer = m_particleBuffers[m_inParticleBufferCurrentIndex].handle;
	beforeFillInParticleBufferMemoryBarrier.offset = 0;
	beforeFillInParticleBufferMemoryBarrier.size = m_maxParticlesNumber * sizeof(Particle);

	VkDependencyInfo beforeFillInParticleBufferDependencyInfo = {};
	beforeFillInParticleBufferDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	beforeFillInParticleBufferDependencyInfo.pNext = nullptr;
	beforeFillInParticleBufferDependencyInfo.dependencyFlags = 0;
	beforeFillInParticleBufferDependencyInfo.memoryBarrierCount = 0;
	beforeFillInParticleBufferDependencyInfo.pMemoryBarriers = nullptr;
	beforeFillInParticleBufferDependencyInfo.bufferMemoryBarrierCount = 1;
	beforeFillInParticleBufferDependencyInfo.pBufferMemoryBarriers = &beforeFillInParticleBufferMemoryBarrier;
	beforeFillInParticleBufferDependencyInfo.imageMemoryBarrierCount = 0;
	beforeFillInParticleBufferDependencyInfo.pImageMemoryBarriers = nullptr;
	m_vkCmdPipelineBarrier2KHR(commandBuffer, &beforeFillInParticleBufferDependencyInfo);

	// Fill in particle with 0
	vkCmdFillBuffer(commandBuffer, m_particleBuffers[m_inParticleBufferCurrentIndex].handle, 0, m_maxParticlesNumber * sizeof(Particle), 0);

	// Synchronize before particle draw
	VkBufferMemoryBarrier2 beforeDrawIndirectBufferMemoryBarrier = {};
	beforeDrawIndirectBufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	beforeDrawIndirectBufferMemoryBarrier.pNext = nullptr;
	beforeDrawIndirectBufferMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	beforeDrawIndirectBufferMemoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	beforeDrawIndirectBufferMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
	beforeDrawIndirectBufferMemoryBarrier.dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
	beforeDrawIndirectBufferMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	beforeDrawIndirectBufferMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	beforeDrawIndirectBufferMemoryBarrier.buffer = m_drawIndirectBuffer.handle;
	beforeDrawIndirectBufferMemoryBarrier.offset = 0;
	beforeDrawIndirectBufferMemoryBarrier.size = sizeof(uint32_t);

	VkBufferMemoryBarrier2 fillParticleBufferBeforeComputeBufferMemoryBarrier = {};
	fillParticleBufferBeforeComputeBufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	fillParticleBufferBeforeComputeBufferMemoryBarrier.pNext = nullptr;
	fillParticleBufferBeforeComputeBufferMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	fillParticleBufferBeforeComputeBufferMemoryBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	fillParticleBufferBeforeComputeBufferMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	fillParticleBufferBeforeComputeBufferMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
	fillParticleBufferBeforeComputeBufferMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	fillParticleBufferBeforeComputeBufferMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	fillParticleBufferBeforeComputeBufferMemoryBarrier.buffer = m_particleBuffers[m_inParticleBufferCurrentIndex].handle;
	fillParticleBufferBeforeComputeBufferMemoryBarrier.offset = 0;
	fillParticleBufferBeforeComputeBufferMemoryBarrier.size = m_maxParticlesNumber * sizeof(Particle);

	VkBufferMemoryBarrier2 particleComputeToDrawBufferMemoryBarrier = {};
	particleComputeToDrawBufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	particleComputeToDrawBufferMemoryBarrier.pNext = nullptr;
	particleComputeToDrawBufferMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	particleComputeToDrawBufferMemoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	particleComputeToDrawBufferMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_INPUT_BIT;
	particleComputeToDrawBufferMemoryBarrier.dstAccessMask = VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
	particleComputeToDrawBufferMemoryBarrier.srcQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	particleComputeToDrawBufferMemoryBarrier.dstQueueFamilyIndex = m_graphicsComputeQueueFamilyIndex;
	particleComputeToDrawBufferMemoryBarrier.buffer = m_particleBuffers[(m_inParticleBufferCurrentIndex + 1) % 2].handle;
	particleComputeToDrawBufferMemoryBarrier.offset = 0;
	particleComputeToDrawBufferMemoryBarrier.size = m_maxParticlesNumber * sizeof(Particle);

	std::array<VkBufferMemoryBarrier2, 3> particleComputeToDrawBufferMemoryBarriers = { beforeDrawIndirectBufferMemoryBarrier, fillParticleBufferBeforeComputeBufferMemoryBarrier, particleComputeToDrawBufferMemoryBarrier };
	VkDependencyInfo particleComputeToDrawDependencyInfo = {};
	particleComputeToDrawDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	particleComputeToDrawDependencyInfo.pNext = nullptr;
	particleComputeToDrawDependencyInfo.dependencyFlags = 0;
	particleComputeToDrawDependencyInfo.memoryBarrierCount = 0;
	particleComputeToDrawDependencyInfo.pMemoryBarriers = nullptr;
	particleComputeToDrawDependencyInfo.bufferMemoryBarrierCount = static_cast<uint32_t>(particleComputeToDrawBufferMemoryBarriers.size());
	particleComputeToDrawDependencyInfo.pBufferMemoryBarriers = particleComputeToDrawBufferMemoryBarriers.data();
	particleComputeToDrawDependencyInfo.imageMemoryBarrierCount = 0;
	particleComputeToDrawDependencyInfo.pImageMemoryBarriers = nullptr;
	m_vkCmdPipelineBarrier2KHR(commandBuffer, &particleComputeToDrawDependencyInfo);

	// Draw particles
	VkRenderingAttachmentInfo particleRenderingColorAttachmentInfo = {};
	particleRenderingColorAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	particleRenderingColorAttachmentInfo.pNext = nullptr;
	particleRenderingColorAttachmentInfo.imageView = drawImageView;
	particleRenderingColorAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	particleRenderingColorAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
	particleRenderingColorAttachmentInfo.resolveImageView = VK_NULL_HANDLE;
	particleRenderingColorAttachmentInfo.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	particleRenderingColorAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	particleRenderingColorAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	particleRenderingColorAttachmentInfo.clearValue.color = { 0.0f, 0.0f, 0.0f, 0.0f };

	VkRenderingAttachmentInfo particleRenderingDepthAttachmentInfo = {};
	particleRenderingDepthAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	particleRenderingDepthAttachmentInfo.pNext = nullptr;
	particleRenderingDepthAttachmentInfo.imageView = depthImageView;
	particleRenderingDepthAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	particleRenderingDepthAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
	particleRenderingDepthAttachmentInfo.resolveImageView = VK_NULL_HANDLE;
	particleRenderingDepthAttachmentInfo.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	particleRenderingDepthAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	particleRenderingDepthAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	particleRenderingDepthAttachmentInfo.clearValue.depthStencil = { 1.0f, 0 };

	VkRenderingInfo particleRenderingInfo = {};
	particleRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	particleRenderingInfo.pNext = nullptr;
	particleRenderingInfo.flags = 0;
	particleRenderingInfo.renderArea = m_scissor;
	particleRenderingInfo.layerCount = 1;
	particleRenderingInfo.viewMask = 0;
	particleRenderingInfo.colorAttachmentCount = 1;
	particleRenderingInfo.pColorAttachments = &particleRenderingColorAttachmentInfo;
	particleRenderingInfo.pDepthAttachment = &particleRenderingDepthAttachmentInfo;
	particleRenderingInfo.pStencilAttachment = nullptr;
	m_vkCmdBeginRenderingKHR(commandBuffer, &particleRenderingInfo);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);
	vkCmdSetViewport(commandBuffer, 0, 1, &m_viewport);
	vkCmdSetScissor(commandBuffer, 0, 1, &m_scissor);

	VkDeviceSize vertexBufferOffset = 0;
	vkCmdBindVertexBuffers(commandBuffer, 0, 1, &m_particleBuffers[(m_inParticleBufferCurrentIndex + 1) % 2].handle, &vertexBufferOffset);

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipelineLayout, 0, 1, &m_graphicsDescriptorSets[currentFrameInFlight], 0, nullptr);

	vkCmdDrawIndirect(commandBuffer, m_drawIndirectBuffer.handle, 0, 1, 0);

	m_vkCmdEndRenderingKHR(commandBuffer);

	m_inParticleBufferCurrentIndex = (m_inParticleBufferCurrentIndex + 1) % 2;
}

void Particles::onResize(uint32_t width, uint32_t height) {
	m_viewport.width = static_cast<float>(width);
	m_viewport.height = static_cast<float>(height);
	m_scissor.extent.width = width;
	m_scissor.extent.height = height;
}

void Particles::emitParticles(const NtshEngn::ParticleEmitter& particleEmitter, uint32_t currentFrameInFlight) {
	std::random_device r;
	std::default_random_engine randomEngine(r());
	std::uniform_real_distribution<float> randomDistribution(0.0f, 1.0f);

	std::vector<Particle> particles(particleEmitter.number);
	for (Particle& particle : particles) {
		particle.position = NtshEngn::Math::vec3(NtshEngn::Math::lerp(particleEmitter.positionRange[0].x, particleEmitter.positionRange[1].x, randomDistribution(randomEngine)),
			NtshEngn::Math::lerp(particleEmitter.positionRange[0].y, particleEmitter.positionRange[1].y, randomDistribution(randomEngine)),
			NtshEngn::Math::lerp(particleEmitter.positionRange[0].z, particleEmitter.positionRange[1].z, randomDistribution(randomEngine)));
		particle.size = NtshEngn::Math::lerp(particleEmitter.sizeRange[0], particleEmitter.sizeRange[1], randomDistribution(randomEngine));
		particle.color = NtshEngn::Math::vec4(NtshEngn::Math::lerp(particleEmitter.colorRange[0].x, particleEmitter.colorRange[1].x, randomDistribution(randomEngine)),
			NtshEngn::Math::lerp(particleEmitter.colorRange[0].y, particleEmitter.colorRange[1].y, randomDistribution(randomEngine)),
			NtshEngn::Math::lerp(particleEmitter.colorRange[0].z, particleEmitter.colorRange[1].y, randomDistribution(randomEngine)),
			NtshEngn::Math::lerp(particleEmitter.colorRange[0].w, particleEmitter.colorRange[1].w, randomDistribution(randomEngine)));
		NtshEngn::Math::vec3 rotation = NtshEngn::Math::vec3(NtshEngn::Math::lerp(particleEmitter.rotationRange[0].x, particleEmitter.rotationRange[1].x, randomDistribution(randomEngine)),
			NtshEngn::Math::lerp(particleEmitter.rotationRange[0].y, particleEmitter.rotationRange[1].y, randomDistribution(randomEngine)),
			NtshEngn::Math::lerp(particleEmitter.rotationRange[0].z, particleEmitter.rotationRange[1].z, randomDistribution(randomEngine)));
		const NtshEngn::Math::vec3 baseDirection = NtshEngn::Math::normalize(particleEmitter.baseDirection);
		const float baseDirectionYaw = std::atan2(baseDirection.z, baseDirection.x);
		const float baseDirectionPitch = -std::asin(baseDirection.y);
		particle.direction = NtshEngn::Math::normalize(NtshEngn::Math::vec3(
			std::cos(baseDirectionPitch + rotation.x) * std::cos(baseDirectionYaw + rotation.y),
			-std::sin(baseDirectionPitch + rotation.x),
			std::cos(baseDirectionPitch + rotation.x) * std::sin(baseDirectionYaw + rotation.y)
		));
		particle.speed = NtshEngn::Math::lerp(particleEmitter.speedRange[0], particleEmitter.speedRange[1], randomDistribution(randomEngine));
		particle.duration = NtshEngn::Math::lerp(particleEmitter.durationRange[0], particleEmitter.durationRange[1], randomDistribution(randomEngine));
	}

	size_t size = particles.size() * sizeof(Particle);
	memcpy(reinterpret_cast<uint8_t*>(m_stagingBuffers[currentFrameInFlight].address) + m_currentParticleHostSize, particles.data(), size);

	m_currentParticleHostSize += size;

	m_particleBuffersNeedUpdate[currentFrameInFlight] = true;
}

void Particles::createBuffers() {
	// Create particle buffer
	VkBufferCreateInfo particleBufferCreateInfo = {};
	particleBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	particleBufferCreateInfo.pNext = nullptr;
	particleBufferCreateInfo.flags = 0;
	particleBufferCreateInfo.size = m_maxParticlesNumber * sizeof(Particle);
	particleBufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	particleBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	particleBufferCreateInfo.queueFamilyIndexCount = 1;
	particleBufferCreateInfo.pQueueFamilyIndices = &m_graphicsComputeQueueFamilyIndex;

	VmaAllocationInfo particleBufferAllocationInfo;

	VmaAllocationCreateInfo particleBufferAllocationCreateInfo = {};
	particleBufferAllocationCreateInfo.flags = 0;
	particleBufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &particleBufferCreateInfo, &particleBufferAllocationCreateInfo, &m_particleBuffers[0].handle, &m_particleBuffers[0].allocation, &particleBufferAllocationInfo));
	NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &particleBufferCreateInfo, &particleBufferAllocationCreateInfo, &m_particleBuffers[1].handle, &m_particleBuffers[1].allocation, &particleBufferAllocationInfo));

	// Create staging buffers
	m_stagingBuffers.resize(m_framesInFlight);
	VkBufferCreateInfo stagingBufferCreateInfo = {};
	stagingBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	stagingBufferCreateInfo.pNext = nullptr;
	stagingBufferCreateInfo.flags = 0;
	stagingBufferCreateInfo.size = m_maxParticlesNumber * sizeof(Particle);
	stagingBufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	stagingBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	stagingBufferCreateInfo.queueFamilyIndexCount = 1;
	stagingBufferCreateInfo.pQueueFamilyIndices = &m_graphicsComputeQueueFamilyIndex;

	VmaAllocationInfo stagingBufferAllocationInfo;

	VmaAllocationCreateInfo stagingBufferAllocationCreateInfo = {};
	stagingBufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
	stagingBufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &stagingBufferCreateInfo, &stagingBufferAllocationCreateInfo, &m_stagingBuffers[i].handle, &m_stagingBuffers[i].allocation, &stagingBufferAllocationInfo));
		m_stagingBuffers[i].address = stagingBufferAllocationInfo.pMappedData;
	}

	m_particleBuffersNeedUpdate.resize(m_framesInFlight);
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		m_particleBuffersNeedUpdate[i] = false;
	}

	// Create draw indirect buffer
	VkBufferCreateInfo drawIndirectBufferCreateInfo = {};
	drawIndirectBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	drawIndirectBufferCreateInfo.pNext = nullptr;
	drawIndirectBufferCreateInfo.flags = 0;
	drawIndirectBufferCreateInfo.size = 4 * sizeof(uint32_t);
	drawIndirectBufferCreateInfo.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	drawIndirectBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	drawIndirectBufferCreateInfo.queueFamilyIndexCount = 1;
	drawIndirectBufferCreateInfo.pQueueFamilyIndices = &m_graphicsComputeQueueFamilyIndex;

	VmaAllocationInfo particleDrawIndirectBufferAllocationInfo;

	VmaAllocationCreateInfo particleDrawIndirectBufferAllocationCreateInfo = {};
	particleDrawIndirectBufferAllocationCreateInfo.flags = 0;
	particleDrawIndirectBufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &drawIndirectBufferCreateInfo, &particleDrawIndirectBufferAllocationCreateInfo, &m_drawIndirectBuffer.handle, &m_drawIndirectBuffer.allocation, &particleDrawIndirectBufferAllocationInfo));

	// Fill draw indirect buffer
	NTSHENGN_VK_CHECK(vkResetCommandPool(m_device, m_initializationCommandPool, 0));

	VkCommandBufferBeginInfo fillDrawIndirectBufferBeginInfo = {};
	fillDrawIndirectBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	fillDrawIndirectBufferBeginInfo.pNext = nullptr;
	fillDrawIndirectBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	fillDrawIndirectBufferBeginInfo.pInheritanceInfo = nullptr;
	NTSHENGN_VK_CHECK(vkBeginCommandBuffer(m_initializationCommandBuffer, &fillDrawIndirectBufferBeginInfo));

	std::vector<uint32_t> drawIndirectData = { 1, 0, 0 };
	vkCmdUpdateBuffer(m_initializationCommandBuffer, m_drawIndirectBuffer.handle, sizeof(uint32_t), 3 * sizeof(uint32_t), drawIndirectData.data());

	NTSHENGN_VK_CHECK(vkEndCommandBuffer(m_initializationCommandBuffer));

	VkSubmitInfo fillDrawIndirectBufferSubmitInfo = {};
	fillDrawIndirectBufferSubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	fillDrawIndirectBufferSubmitInfo.pNext = nullptr;
	fillDrawIndirectBufferSubmitInfo.waitSemaphoreCount = 0;
	fillDrawIndirectBufferSubmitInfo.pWaitSemaphores = nullptr;
	fillDrawIndirectBufferSubmitInfo.pWaitDstStageMask = nullptr;
	fillDrawIndirectBufferSubmitInfo.commandBufferCount = 1;
	fillDrawIndirectBufferSubmitInfo.pCommandBuffers = &m_initializationCommandBuffer;
	fillDrawIndirectBufferSubmitInfo.signalSemaphoreCount = 0;
	fillDrawIndirectBufferSubmitInfo.pSignalSemaphores = nullptr;
	NTSHENGN_VK_CHECK(vkQueueSubmit(m_graphicsComputeQueue, 1, &fillDrawIndirectBufferSubmitInfo, m_initializationFence));
	NTSHENGN_VK_CHECK(vkWaitForFences(m_device, 1, &m_initializationFence, VK_TRUE, std::numeric_limits<uint64_t>::max()));
	NTSHENGN_VK_CHECK(vkResetFences(m_device, 1, &m_initializationFence));
}

void Particles::createComputeResources() {
	// Create descriptor set layout
	VkDescriptorSetLayoutBinding inParticleBufferDescriptorSetLayoutBinding = {};
	inParticleBufferDescriptorSetLayoutBinding.binding = 0;
	inParticleBufferDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	inParticleBufferDescriptorSetLayoutBinding.descriptorCount = 1;
	inParticleBufferDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	inParticleBufferDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding outParticleBufferDescriptorSetLayoutBinding = {};
	outParticleBufferDescriptorSetLayoutBinding.binding = 1;
	outParticleBufferDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	outParticleBufferDescriptorSetLayoutBinding.descriptorCount = 1;
	outParticleBufferDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	outParticleBufferDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding outDrawIndirectBufferDescriptorSetLayoutBinding = {};
	outDrawIndirectBufferDescriptorSetLayoutBinding.binding = 2;
	outDrawIndirectBufferDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	outDrawIndirectBufferDescriptorSetLayoutBinding.descriptorCount = 1;
	outDrawIndirectBufferDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	outDrawIndirectBufferDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	std::array<VkDescriptorSetLayoutBinding, 3> descriptorSetLayoutBindings = { inParticleBufferDescriptorSetLayoutBinding, outParticleBufferDescriptorSetLayoutBinding, outDrawIndirectBufferDescriptorSetLayoutBinding };
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.pNext = nullptr;
	descriptorSetLayoutCreateInfo.flags = 0;
	descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(descriptorSetLayoutBindings.size());
	descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings.data();
	NTSHENGN_VK_CHECK(vkCreateDescriptorSetLayout(m_device, &descriptorSetLayoutCreateInfo, nullptr, &m_computeDescriptorSetLayout));

	// Create compute pipeline
	const std::string computeShaderCode = R"GLSL(
		#version 460

		layout(local_size_x = 64) in;

		struct Particle {
			vec3 position;
			float size;
			vec4 color;
			vec3 direction;
			float speed;
			float duration;
		};

		layout(set = 0, binding = 0) restrict readonly buffer InParticles {
			Particle particles[];
		} inParticles;

		layout(set = 0, binding = 1) restrict writeonly buffer OutParticles {
			Particle particles[];
		} outParticles;

		layout(set = 0, binding = 2) buffer OutDrawIndirect {
			uint vertexCount;
		} outDrawIndirect;

		layout(push_constant) uniform DeltaTime {
			float deltaTime;
		} dT;

		void main() {
			uint index = gl_GlobalInvocationID.x;

			Particle inParticle = inParticles.particles[index];

			float newDuration = inParticle.duration - dT.deltaTime;
			if (newDuration >= 0.0) {
				uint particleIndex = atomicAdd(outDrawIndirect.vertexCount, 1);

				outParticles.particles[particleIndex].position = inParticle.position + (inParticle.direction * inParticle.speed * dT.deltaTime);
				outParticles.particles[particleIndex].size = inParticle.size;
				outParticles.particles[particleIndex].color = inParticle.color;
				outParticles.particles[particleIndex].direction = inParticle.direction;
				outParticles.particles[particleIndex].speed = inParticle.speed;
				outParticles.particles[particleIndex].duration = newDuration;
			}
		}
	)GLSL";
	const std::vector<uint32_t> computeShaderSpv = compileShader(computeShaderCode, ShaderType::Compute);

	VkShaderModule computeShaderModule;
	VkShaderModuleCreateInfo computeShaderModuleCreateInfo = {};
	computeShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	computeShaderModuleCreateInfo.pNext = nullptr;
	computeShaderModuleCreateInfo.flags = 0;
	computeShaderModuleCreateInfo.codeSize = computeShaderSpv.size() * sizeof(uint32_t);
	computeShaderModuleCreateInfo.pCode = computeShaderSpv.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &computeShaderModuleCreateInfo, nullptr, &computeShaderModule));

	VkPipelineShaderStageCreateInfo computeShaderStageCreateInfo = {};
	computeShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	computeShaderStageCreateInfo.pNext = nullptr;
	computeShaderStageCreateInfo.flags = 0;
	computeShaderStageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	computeShaderStageCreateInfo.module = computeShaderModule;
	computeShaderStageCreateInfo.pName = "main";
	computeShaderStageCreateInfo.pSpecializationInfo = nullptr;

	VkPushConstantRange pushConstantRange = {};
	pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	pushConstantRange.offset = 0;
	pushConstantRange.size = sizeof(float);

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.pNext = nullptr;
	pipelineLayoutCreateInfo.flags = 0;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &m_computeDescriptorSetLayout;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
	NTSHENGN_VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_computePipelineLayout));

	VkComputePipelineCreateInfo computePipelineCreateInfo = {};
	computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo.pNext = nullptr;
	computePipelineCreateInfo.flags = 0;
	computePipelineCreateInfo.stage = computeShaderStageCreateInfo;
	computePipelineCreateInfo.layout = m_computePipelineLayout;
	computePipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	computePipelineCreateInfo.basePipelineIndex = 0;
	NTSHENGN_VK_CHECK(vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &m_computePipeline));

	vkDestroyShaderModule(m_device, computeShaderModule, nullptr);

	// Create descriptor pool
	VkDescriptorPoolSize inParticleDescriptorPoolSize = {};
	inParticleDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	inParticleDescriptorPoolSize.descriptorCount = 2;

	VkDescriptorPoolSize outParticleDescriptorPoolSize = {};
	outParticleDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	outParticleDescriptorPoolSize.descriptorCount = 2;

	VkDescriptorPoolSize outDrawIndirectDescriptorPoolSize = {};
	outDrawIndirectDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	outDrawIndirectDescriptorPoolSize.descriptorCount = 2;

	std::array<VkDescriptorPoolSize, 3> descriptorPoolSizes = { inParticleDescriptorPoolSize, outParticleDescriptorPoolSize, outDrawIndirectDescriptorPoolSize };
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext = nullptr;
	descriptorPoolCreateInfo.flags = 0;
	descriptorPoolCreateInfo.maxSets = 2;
	descriptorPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size());
	descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes.data();
	NTSHENGN_VK_CHECK(vkCreateDescriptorPool(m_device, &descriptorPoolCreateInfo, nullptr, &m_computeDescriptorPool));

	// Allocate descriptor sets
	for (size_t i = 0; i < m_computeDescriptorSets.size(); i++) {
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
		descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocateInfo.pNext = nullptr;
		descriptorSetAllocateInfo.descriptorPool = m_computeDescriptorPool;
		descriptorSetAllocateInfo.descriptorSetCount = 1;
		descriptorSetAllocateInfo.pSetLayouts = &m_computeDescriptorSetLayout;
		NTSHENGN_VK_CHECK(vkAllocateDescriptorSets(m_device, &descriptorSetAllocateInfo, &m_computeDescriptorSets[i]));
	}

	// Update descriptor sets
	VkDescriptorBufferInfo particle0DescriptorBufferInfo;
	particle0DescriptorBufferInfo.buffer = m_particleBuffers[0].handle;
	particle0DescriptorBufferInfo.offset = 0;
	particle0DescriptorBufferInfo.range = m_maxParticlesNumber * sizeof(Particle);

	VkWriteDescriptorSet inParticle0DescriptorWriteDescriptorSet = {};
	inParticle0DescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	inParticle0DescriptorWriteDescriptorSet.pNext = nullptr;
	inParticle0DescriptorWriteDescriptorSet.dstSet = m_computeDescriptorSets[0];
	inParticle0DescriptorWriteDescriptorSet.dstBinding = 0;
	inParticle0DescriptorWriteDescriptorSet.dstArrayElement = 0;
	inParticle0DescriptorWriteDescriptorSet.descriptorCount = 1;
	inParticle0DescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	inParticle0DescriptorWriteDescriptorSet.pImageInfo = nullptr;
	inParticle0DescriptorWriteDescriptorSet.pBufferInfo = &particle0DescriptorBufferInfo;
	inParticle0DescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

	VkWriteDescriptorSet outParticle0DescriptorWriteDescriptorSet = {};
	outParticle0DescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	outParticle0DescriptorWriteDescriptorSet.pNext = nullptr;
	outParticle0DescriptorWriteDescriptorSet.dstSet = m_computeDescriptorSets[1];
	outParticle0DescriptorWriteDescriptorSet.dstBinding = 1;
	outParticle0DescriptorWriteDescriptorSet.dstArrayElement = 0;
	outParticle0DescriptorWriteDescriptorSet.descriptorCount = 1;
	outParticle0DescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	outParticle0DescriptorWriteDescriptorSet.pImageInfo = nullptr;
	outParticle0DescriptorWriteDescriptorSet.pBufferInfo = &particle0DescriptorBufferInfo;
	outParticle0DescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

	VkDescriptorBufferInfo particle1DescriptorBufferInfo;
	particle1DescriptorBufferInfo.buffer = m_particleBuffers[1].handle;
	particle1DescriptorBufferInfo.offset = 0;
	particle1DescriptorBufferInfo.range = m_maxParticlesNumber * sizeof(Particle);

	VkWriteDescriptorSet inParticle1DescriptorWriteDescriptorSet = {};
	inParticle1DescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	inParticle1DescriptorWriteDescriptorSet.pNext = nullptr;
	inParticle1DescriptorWriteDescriptorSet.dstSet = m_computeDescriptorSets[1];
	inParticle1DescriptorWriteDescriptorSet.dstBinding = 0;
	inParticle1DescriptorWriteDescriptorSet.dstArrayElement = 0;
	inParticle1DescriptorWriteDescriptorSet.descriptorCount = 1;
	inParticle1DescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	inParticle1DescriptorWriteDescriptorSet.pImageInfo = nullptr;
	inParticle1DescriptorWriteDescriptorSet.pBufferInfo = &particle1DescriptorBufferInfo;
	inParticle1DescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

	VkWriteDescriptorSet outParticle1DescriptorWriteDescriptorSet = {};
	outParticle1DescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	outParticle1DescriptorWriteDescriptorSet.pNext = nullptr;
	outParticle1DescriptorWriteDescriptorSet.dstSet = m_computeDescriptorSets[0];
	outParticle1DescriptorWriteDescriptorSet.dstBinding = 1;
	outParticle1DescriptorWriteDescriptorSet.dstArrayElement = 0;
	outParticle1DescriptorWriteDescriptorSet.descriptorCount = 1;
	outParticle1DescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	outParticle1DescriptorWriteDescriptorSet.pImageInfo = nullptr;
	outParticle1DescriptorWriteDescriptorSet.pBufferInfo = &particle1DescriptorBufferInfo;
	outParticle1DescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

	VkDescriptorBufferInfo outDrawIndirectDescriptorBufferInfo;
	outDrawIndirectDescriptorBufferInfo.buffer = m_drawIndirectBuffer.handle;
	outDrawIndirectDescriptorBufferInfo.offset = 0;
	outDrawIndirectDescriptorBufferInfo.range = sizeof(uint32_t);

	VkWriteDescriptorSet outDrawIndirect0DescriptorWriteDescriptorSet = {};
	outDrawIndirect0DescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	outDrawIndirect0DescriptorWriteDescriptorSet.pNext = nullptr;
	outDrawIndirect0DescriptorWriteDescriptorSet.dstSet = m_computeDescriptorSets[0];
	outDrawIndirect0DescriptorWriteDescriptorSet.dstBinding = 2;
	outDrawIndirect0DescriptorWriteDescriptorSet.dstArrayElement = 0;
	outDrawIndirect0DescriptorWriteDescriptorSet.descriptorCount = 1;
	outDrawIndirect0DescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	outDrawIndirect0DescriptorWriteDescriptorSet.pImageInfo = nullptr;
	outDrawIndirect0DescriptorWriteDescriptorSet.pBufferInfo = &outDrawIndirectDescriptorBufferInfo;
	outDrawIndirect0DescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

	VkWriteDescriptorSet outDrawIndirect1DescriptorWriteDescriptorSet = {};
	outDrawIndirect1DescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	outDrawIndirect1DescriptorWriteDescriptorSet.pNext = nullptr;
	outDrawIndirect1DescriptorWriteDescriptorSet.dstSet = m_computeDescriptorSets[1];
	outDrawIndirect1DescriptorWriteDescriptorSet.dstBinding = 2;
	outDrawIndirect1DescriptorWriteDescriptorSet.dstArrayElement = 0;
	outDrawIndirect1DescriptorWriteDescriptorSet.descriptorCount = 1;
	outDrawIndirect1DescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	outDrawIndirect1DescriptorWriteDescriptorSet.pImageInfo = nullptr;
	outDrawIndirect1DescriptorWriteDescriptorSet.pBufferInfo = &outDrawIndirectDescriptorBufferInfo;
	outDrawIndirect1DescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

	std::array<VkWriteDescriptorSet, 6> writeDescriptorSets = { inParticle0DescriptorWriteDescriptorSet, outParticle0DescriptorWriteDescriptorSet, inParticle1DescriptorWriteDescriptorSet, outParticle1DescriptorWriteDescriptorSet, outDrawIndirect0DescriptorWriteDescriptorSet, outDrawIndirect1DescriptorWriteDescriptorSet };
	vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
}

void Particles::createGraphicsResources(VkFormat drawImageFormat, const std::vector<HostVisibleVulkanBuffer>& cameraBuffers) {
	// Create descriptor set layout
	VkDescriptorSetLayoutBinding cameraDescriptorSetLayoutBinding = {};
	cameraDescriptorSetLayoutBinding.binding = 0;
	cameraDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	cameraDescriptorSetLayoutBinding.descriptorCount = 1;
	cameraDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	cameraDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.pNext = nullptr;
	descriptorSetLayoutCreateInfo.flags = 0;
	descriptorSetLayoutCreateInfo.bindingCount = 1;
	descriptorSetLayoutCreateInfo.pBindings = &cameraDescriptorSetLayoutBinding;
	NTSHENGN_VK_CHECK(vkCreateDescriptorSetLayout(m_device, &descriptorSetLayoutCreateInfo, nullptr, &m_graphicsDescriptorSetLayout));

	// Create graphics pipeline
	VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {};
	pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	pipelineRenderingCreateInfo.pNext = nullptr;
	pipelineRenderingCreateInfo.viewMask = 0;
	pipelineRenderingCreateInfo.colorAttachmentCount = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &drawImageFormat;
	pipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;
	pipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

	const std::string vertexShaderCode = R"GLSL(
		#version 460

		layout(set = 0, binding = 0) uniform Camera {
			mat4 view;
			mat4 projection;
		} camera;

		layout(location = 0) in vec3 position;
		layout(location = 1) in float size;
		layout(location = 2) in vec4 color;

		layout(location = 0) out vec4 fragColor;

		void main() {
			gl_PointSize = size;
			gl_Position = camera.projection * camera.view * vec4(position, 1.0);
			fragColor = color;
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

		layout(location = 0) in vec4 fragColor;

		layout(location = 0) out vec4 outColor;

		void main() {
			outColor = fragColor;
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
	vertexInputBindingDescription.stride = sizeof(Particle);
	vertexInputBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	VkVertexInputAttributeDescription particlePositionInputAttributeDescription = {};
	particlePositionInputAttributeDescription.location = 0;
	particlePositionInputAttributeDescription.binding = 0;
	particlePositionInputAttributeDescription.format = VK_FORMAT_R32G32B32_SFLOAT;
	particlePositionInputAttributeDescription.offset = 0;

	VkVertexInputAttributeDescription particleSizeInputAttributeDescription = {};
	particleSizeInputAttributeDescription.location = 1;
	particleSizeInputAttributeDescription.binding = 0;
	particleSizeInputAttributeDescription.format = VK_FORMAT_R32_SFLOAT;
	particleSizeInputAttributeDescription.offset = offsetof(Particle, size);

	VkVertexInputAttributeDescription particleColorInputAttributeDescription = {};
	particleColorInputAttributeDescription.location = 2;
	particleColorInputAttributeDescription.binding = 0;
	particleColorInputAttributeDescription.format = VK_FORMAT_R32G32B32A32_SFLOAT;
	particleColorInputAttributeDescription.offset = offsetof(Particle, color);

	std::array<VkVertexInputAttributeDescription, 3> vertexInputAttributeDescriptions = { particlePositionInputAttributeDescription, particleSizeInputAttributeDescription, particleColorInputAttributeDescription };
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
	inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
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
	colorBlendAttachmentState.blendEnable = VK_TRUE;
	colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
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
	pipelineLayoutCreateInfo.pSetLayouts = &m_graphicsDescriptorSetLayout;
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

	// Create descriptor pool
	VkDescriptorPoolSize cameraDescriptorPoolSize = {};
	cameraDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	cameraDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext = nullptr;
	descriptorPoolCreateInfo.flags = 0;
	descriptorPoolCreateInfo.maxSets = m_framesInFlight;
	descriptorPoolCreateInfo.poolSizeCount = 1;
	descriptorPoolCreateInfo.pPoolSizes = &cameraDescriptorPoolSize;
	NTSHENGN_VK_CHECK(vkCreateDescriptorPool(m_device, &descriptorPoolCreateInfo, nullptr, &m_graphicsDescriptorPool));

	// Allocate descriptor sets
	m_graphicsDescriptorSets.resize(m_framesInFlight);
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
		descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocateInfo.pNext = nullptr;
		descriptorSetAllocateInfo.descriptorPool = m_graphicsDescriptorPool;
		descriptorSetAllocateInfo.descriptorSetCount = 1;
		descriptorSetAllocateInfo.pSetLayouts = &m_graphicsDescriptorSetLayout;
		NTSHENGN_VK_CHECK(vkAllocateDescriptorSets(m_device, &descriptorSetAllocateInfo, &m_graphicsDescriptorSets[i]));
	}

	// Update descriptor sets
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		VkDescriptorBufferInfo cameraDescriptorBufferInfo;
		cameraDescriptorBufferInfo.buffer = cameraBuffers[i].handle;
		cameraDescriptorBufferInfo.offset = 0;
		cameraDescriptorBufferInfo.range = sizeof(NtshEngn::Math::mat4) * 2;

		VkWriteDescriptorSet cameraDescriptorWriteDescriptorSet = {};
		cameraDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		cameraDescriptorWriteDescriptorSet.pNext = nullptr;
		cameraDescriptorWriteDescriptorSet.dstSet = m_graphicsDescriptorSets[i];
		cameraDescriptorWriteDescriptorSet.dstBinding = 0;
		cameraDescriptorWriteDescriptorSet.dstArrayElement = 0;
		cameraDescriptorWriteDescriptorSet.descriptorCount = 1;
		cameraDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		cameraDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		cameraDescriptorWriteDescriptorSet.pBufferInfo = &cameraDescriptorBufferInfo;
		cameraDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

		vkUpdateDescriptorSets(m_device, 1, &cameraDescriptorWriteDescriptorSet, 0, nullptr);
	}
}
