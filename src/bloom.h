#pragma once
#include "common.h"
#include <vector>
#include <utility>

class Bloom {
public:
	void init(VkDevice device,
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
		PFN_vkCmdPipelineBarrier2KHR vkCmdPipelineBarrier2KHR);
	void destroy();

	void draw(VkCommandBuffer commandBuffer, VkImage drawImage, VkImageView drawImageView);

	void onResize(uint32_t width,
		uint32_t height,
		VkImageView drawImageView);

private:
	void createImages(uint32_t width, uint32_t height);
	void destroyImages();

	void createDescriptorSetLayouts();
	void createResizeThresholdDescriptorSetLayout();
	void createBlurDescriptorSetLayout();
	void createBloomDescriptorSetLayout();

	void createGraphicsPipelines(VkFormat drawImageFormat);
	void createResizeThresholdGraphicsPipeline(VkFormat drawImageFormat);
	void createBlurGraphicsPipeline(VkFormat drawImageFormat);
	void createBloomGraphicsPipeline(VkFormat drawImageFormat);

	void createDescriptorSets();
	void createResizeThresholdDescriptorSet();
	void createBlurDescriptorSets();
	void createBloomDescriptorSet();

	void updateDescriptorSets(VkImageView drawImageView);

private:
	VulkanImage m_bloomImage;
	VulkanImage m_blurImage;

	VkDescriptorSetLayout m_resizeThresholdDescriptorSetLayout;

	VkDescriptorSetLayout m_blurDescriptorSetLayout;

	VkDescriptorSetLayout m_bloomDescriptorSetLayout;

	VkPipeline m_resizeThresholdGraphicsPipeline;
	VkPipelineLayout m_resizeThresholdGraphicsPipelineLayout;

	VkPipeline m_blurGraphicsPipeline;
	VkPipelineLayout m_blurGraphicsPipelineLayout;

	VkPipeline m_bloomGraphicsPipeline;
	VkPipelineLayout m_bloomGraphicsPipelineLayout;

	VkDescriptorPool m_resizeThresholdDescriptorPool;
	VkDescriptorSet m_resizeThresholdDescriptorSet;

	VkDescriptorPool m_blurDescriptorPool;
	std::vector<VkDescriptorSet> m_blurDescriptorSets;
	std::vector<VkDescriptorSet> m_blurBackDescriptorSets;

	VkDescriptorPool m_bloomDescriptorPool;
	VkDescriptorSet m_bloomDescriptorSet;

	VkSampler m_nearestSampler;
	VkSampler m_linearSampler;

	VkDevice m_device;
	VkQueue m_graphicsQueue;
	uint32_t m_graphicsQueueFamilyIndex;
	VmaAllocator m_allocator;
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

	uint32_t m_mipLevels;
	std::vector<std::pair<uint32_t, uint32_t>> m_mipSizes;
};