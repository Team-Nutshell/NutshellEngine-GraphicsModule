#pragma once
#include "common.h"

class SSAO {
public:
	void init(VkDevice device,
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
		const std::vector<VulkanBuffer>& cameraBuffers,
		PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR,
		PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR,
		PFN_vkCmdPipelineBarrier2KHR vkCmdPipelineBarrier2KHR);
	void destroy();

	void draw(VkCommandBuffer commandBuffer, uint32_t currentFrameInFlight);

	void onResize(uint32_t width,
		uint32_t height,
		VkImageView positionImageView,
		VkImageView normalImageView);
	
	VulkanImage& getSSAO();

private:
	void createImagesAndBuffer(uint32_t width, uint32_t height);
	void createSSAOImages(uint32_t width, uint32_t height);
	void destroySSAOImages();

	void createImageSamplers();

	void createDescriptorSetLayouts();

	void createGraphicsPipelines();
	void createSSAOGraphicsPipeline();
	void createSSAOBlurGraphicsPipeline();

	void createDescriptorSets(const std::vector<VulkanBuffer>& cameraBuffers);
	void createSSAODescriptorSets(const std::vector<VulkanBuffer>& cameraBuffers);
	void createSSAOBlurDescriptorSet();

	void updateDescriptorSets(VkImageView positionImageView, VkImageView normalImageView);
	void updateSSAODescriptorSet(VkImageView positionImageView, VkImageView normalImageView);
	void updateSSAOBlurDescriptorSet();

private:
	VulkanImage m_ssaoImage;
	VulkanImage m_ssaoBlurImage;
	VulkanImage m_randomImage;
	VulkanBuffer m_randomSampleBuffer;

	VkDescriptorSetLayout m_ssaoDescriptorSetLayout;
	VkPipeline m_ssaoGraphicsPipeline;
	VkPipelineLayout m_ssaoGraphicsPipelineLayout;
	VkDescriptorPool m_ssaoDescriptorPool;
	std::vector<VkDescriptorSet> m_ssaoDescriptorSets;

	VkDescriptorSetLayout m_ssaoBlurDescriptorSetLayout;
	VkPipeline m_ssaoBlurGraphicsPipeline;
	VkPipelineLayout m_ssaoBlurGraphicsPipelineLayout;
	VkDescriptorPool m_ssaoBlurDescriptorPool;
	VkDescriptorSet m_ssaoBlurDescriptorSet;

	VkSampler m_nearestSampler;
	VkSampler m_repeatSampler;

	VkDevice m_device;
	VmaAllocator m_allocator;
	VkQueue m_graphicsQueue;
	uint32_t m_graphicsQueueFamilyIndex;
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