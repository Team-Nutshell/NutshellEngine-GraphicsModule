#pragma once
#include "common.h"

class FrustumCulling {
public:
	void init(VkDevice device,
		VkQueue computeQueue,
		uint32_t computeQueueFamilyIndex,
		VmaAllocator allocator,
		VkFence initializationFence,
		uint32_t framesInFlight,
		PFN_vkCmdPipelineBarrier2KHR vkCmdPipelineBarrier2KHR
	);
	void destroy();

private:
	void createDescriptorSetLayout();

	void createComputePipeline();

private:
	VkDescriptorSetLayout m_descriptorSetLayout;

	VkPipeline m_computePipeline;
	VkPipelineLayout m_computePipelineLayout;

	VkDescriptorPool m_descriptorPool;
	std::vector<VkDescriptorSet> m_descriptorSets;
	std::vector<bool> m_descriptorSetsNeedUpdate;

	VkDevice m_device;
	VkQueue m_computeQueue;
	uint32_t m_computeQueueFamilyIndex;
	VmaAllocator m_allocator;
	VkFence m_initializationFence;
	uint32_t m_framesInFlight;

	PFN_vkCmdPipelineBarrier2KHR m_vkCmdPipelineBarrier2KHR;
};