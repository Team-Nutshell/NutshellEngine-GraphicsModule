#pragma once
#include "common.h"

class ToneMapping {
public:
	void init(VkDevice device,
		VkQueue graphicsQueue,
		uint32_t graphicsQueueFamilyIndex,
		VmaAllocator allocator,
		VkCommandPool initializationCommandPool,
		VkCommandBuffer initializationCommandBuffer,
		VkFence initializationFence,
		VkFormat drawImageFormat,
		VkViewport viewport,
		VkRect2D scissor,
		VkImageView compositingImageView,
		PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR,
		PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR,
		PFN_vkCmdPipelineBarrier2KHR vkCmdPipelineBarrier2KHR);
	void destroy();

	void draw(VkCommandBuffer commandBuffer);

	void onResize(uint32_t width, uint32_t height, VkFormat drawImageFormat, VkImageView compositingImageView);

	void updateShadowDescriptorSets(uint32_t frameInFlight, const std::vector<VulkanImage> shadowMaps);

	VulkanImage& getImage();

private:
	void createImage(uint32_t width, uint32_t height, VkFormat drawImageFormat);

	void createDescriptorSetLayout();

	void createGraphicsPipeline(VkFormat drawImageFormat);

	void createSampler();

	void createDescriptorSet();

	void updateDescriptorSet(VkImageView compositingImageView);

private:
	VulkanImage m_image;
	VkSampler m_sampler;
	VkDescriptorSetLayout m_descriptorSetLayout;
	VkDescriptorPool m_descriptorPool;
	VkDescriptorSet m_descriptorSet;

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

	PFN_vkCmdBeginRenderingKHR m_vkCmdBeginRenderingKHR;
	PFN_vkCmdEndRenderingKHR m_vkCmdEndRenderingKHR;
	PFN_vkCmdPipelineBarrier2KHR m_vkCmdPipelineBarrier2KHR;
};