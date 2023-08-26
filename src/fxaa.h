#pragma once
#include "common.h"

class FXAA {
public:
	void init(VkDevice device,
		uint32_t graphicsQueueFamilyIndex,
		VkImageView colorImageView,
		VkFormat drawImageFormat,
		VkViewport viewport,
		VkRect2D scissor,
		PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR,
		PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR,
		PFN_vkCmdPipelineBarrier2KHR vkCmdPipelineBarrier2KHR);
	void destroy();

	void draw(VkCommandBuffer commandBuffer, VkImage drawImage, VkImageView drawImageView);

	void onResize(uint32_t width,
		uint32_t height,
		VkImageView colorImageView);

private:
	void createDescriptorSetLayout();

	void createGraphicsPipeline(VkFormat drawImageFormat);

	void createDescriptorSet();

	void updateDescriptorSet(VkImageView colorImageView);

private:
	VkDescriptorSetLayout m_descriptorSetLayout;

	VkPipeline m_graphicsPipeline;
	VkPipelineLayout m_graphicsPipelineLayout;

	VkDescriptorPool m_descriptorPool;
	VkDescriptorSet m_descriptorSet;

	VkSampler m_sampler;

	VkDevice m_device;
	uint32_t m_graphicsQueueFamilyIndex;
	VkViewport m_viewport;
	VkRect2D m_scissor;
	uint32_t m_framesInFlight;

	PFN_vkCmdBeginRenderingKHR m_vkCmdBeginRenderingKHR;
	PFN_vkCmdEndRenderingKHR m_vkCmdEndRenderingKHR;
	PFN_vkCmdPipelineBarrier2KHR m_vkCmdPipelineBarrier2KHR;
};