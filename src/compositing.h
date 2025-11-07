#pragma once
#include "common.h"

class Compositing {
public:
	void init(VkDevice device,
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
		PFN_vkCmdPipelineBarrier2KHR vkCmdPipelineBarrier2KHR);
	void destroy();

	void draw(VkCommandBuffer commandBuffer,
		uint32_t currentFrameInFlight,
		const NtshEngn::Math::vec4& backgroundColor);

	void onResize(uint32_t width, uint32_t height, VkImageView gBufferPositionView, VkImageView gBufferNormalView, VkImageView gBufferDiffuseView, VkImageView gBufferMaterialView, VkImageView gBufferEmissiveView, VkImageView ssaoImageView);

	void shadowDescriptorSetNeedsUpdate(uint32_t frameInFlight);
	void updateShadowDescriptorSets(uint32_t frameInFlight, const std::vector<VulkanImage> shadowMaps);

	VulkanImage& getImage();
	VkFormat getImageFormat();

private:
	void createImage(uint32_t width, uint32_t height);

	void createDescriptorSetLayout();

	void createGraphicsPipeline();

	void createSamplers();

	void createDescriptorSets(const std::vector<HostVisibleVulkanBuffer>& cameraBuffers, const std::vector<HostVisibleVulkanBuffer>& lightBuffers, const std::vector<HostVisibleVulkanBuffer>& shadowSceneBuffers);

	void updateDescriptorSets(VkImageView gBufferPositionView, VkImageView gBufferNormalView, VkImageView gBufferDiffuseView, VkImageView gBufferMaterialView, VkImageView gBufferEmissiveView, VkImageView ssaoView);

private:
	VulkanImage m_image;
	VkFormat m_imageFormat;
	VkSampler m_sampler;
	VkSampler m_shadowSampler;
	VkDescriptorSetLayout m_descriptorSetLayout;
	VkDescriptorPool m_descriptorPool;
	std::vector<VkDescriptorSet> m_descriptorSets;
	std::vector<bool> m_descriptorSetsShadowNeedUpdate;

	VkPipeline m_graphicsPipeline;
	VkPipelineLayout m_graphicsPipelineLayout;

	VkDevice m_device;
	VkQueue m_graphicsQueue;
	uint32_t m_graphicsQueueFamilyIndex;
	VmaAllocator m_allocator;
	VkCommandPool m_initializationCommandPool;
	VkCommandBuffer m_initializationCommandBuffer;
	VkFence m_initializationFence;
	VkViewport m_viewport;
	VkRect2D m_scissor;
	uint32_t m_framesInFlight;

	PFN_vkCmdBeginRenderingKHR m_vkCmdBeginRenderingKHR;
	PFN_vkCmdEndRenderingKHR m_vkCmdEndRenderingKHR;
	PFN_vkCmdPipelineBarrier2KHR m_vkCmdPipelineBarrier2KHR;
};