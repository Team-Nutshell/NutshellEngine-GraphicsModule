#pragma once
#include "common.h"

class SSAO {
public:
	void init(VkDevice device, VkQueue graphicsQueue, uint32_t graphicsQueueFamilyIndex, VmaAllocator allocator, VkCommandPool initializationCommandPool, VkCommandBuffer initializationCommandBuffer, VkFence initializationFence, VkImageView depthImageView, VkViewport viewport, VkRect2D scissor, uint32_t framesInFlight, const std::vector<HostVisibleVulkanBuffer>& cameraBuffers, PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR, PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR, PFN_vkCmdPipelineBarrier2KHR vkCmdPipelineBarrier2KHR);
	void destroy();

	void draw(VkCommandBuffer commandBuffer, uint32_t currentFrameInFlight);

	void onResize(uint32_t width, uint32_t height, VkImageView depthImageView);
	
	VulkanImage& getImage();

private:
	void createImagesAndBuffer(uint32_t width, uint32_t height);
	void createSSAOImages(uint32_t width, uint32_t height);
	void destroySSAOImages();

	void createSamplers();

	void createDescriptorSetLayouts();

	void createGraphicsPipelines();
	void createPositionAndNormalFromDepthGraphicsPipeline();
	void createSSAOGraphicsPipeline();
	void createSSAOBlurGraphicsPipeline();

	void createDescriptorSets(const std::vector<HostVisibleVulkanBuffer>& cameraBuffers);
	void createPositionAndNormalFromDepthDescriptorSets(const std::vector<HostVisibleVulkanBuffer>& cameraBuffers);
	void createSSAODescriptorSets(const std::vector<HostVisibleVulkanBuffer>& cameraBuffers);
	void createSSAOBlurDescriptorSet();

	void updateDescriptorSets(VkImageView depthImageView);
	void updatePositionAndNormalFromDepthDescriptorSet(VkImageView depthImageView);
	void updateSSAODescriptorSet();
	void updateSSAOBlurDescriptorSet();

private:
	VulkanImage m_positionFromDepthImage;
	VulkanImage m_normalFromDepthImage;
	VulkanImage m_ssaoImage;
	VulkanImage m_ssaoBlurImage;
	VulkanImage m_randomImage;
	VulkanBuffer m_randomSampleBuffer;

	VkDescriptorSetLayout m_positionAndNormalFromDepthDescriptorSetLayout;
	VkPipeline m_positionAndNormalFromDepthGraphicsPipeline;
	VkPipelineLayout m_positionAndNormalFromDepthGraphicsPipelineLayout;
	VkDescriptorPool m_positionAndNormalFromDepthDescriptorPool;
	std::vector<VkDescriptorSet> m_positionAndNormalFromDepthDescriptorSets;

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
	VkSampler m_linearSampler;
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
	VkViewport m_downscaledViewport;
	VkRect2D m_downscaledScissor;
	uint32_t m_framesInFlight;

	PFN_vkCmdBeginRenderingKHR m_vkCmdBeginRenderingKHR;
	PFN_vkCmdEndRenderingKHR m_vkCmdEndRenderingKHR;
	PFN_vkCmdPipelineBarrier2KHR m_vkCmdPipelineBarrier2KHR;
};