#pragma once
#include "common.h"
#include "../Common/job_system/ntshengn_job_system.h"

class FrustumCulling {
public:
	void init(VkDevice device,
		VkQueue computeQueue,
		uint32_t computeQueueFamilyIndex,
		VmaAllocator allocator,
		VkFence initializationFence,
		uint32_t framesInFlight,
		PFN_vkCmdPipelineBarrier2KHR vkCmdPipelineBarrier2KHR,
		NtshEngn::JobSystem* jobSystem,
		NtshEngn::ECS* ecs);
	void destroy();

	uint32_t culling(uint32_t currentFrameInFlight,
		const NtshEngn::Math::mat4& cameraView,
		const NtshEngn::Math::mat4& cameraProjection,
		const std::set<NtshEngn::Entity>& entities,
		const std::unordered_map<NtshEngn::Entity, InternalObject>& objects,
		const std::vector<InternalMesh>& meshes);

	VulkanBuffer& getDrawIndirectBuffer(uint32_t frameInFlight);
	std::vector<VulkanBuffer> getPerDrawBuffers();

private:
	void createBuffers();

	void createDescriptorSetLayout();

	void createComputePipeline();

private:
	std::vector<VulkanBuffer> m_drawIndirectBuffers;
	std::vector<VulkanBuffer> m_perDrawBuffers;

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

	NtshEngn::JobSystem* m_jobSystem;
	NtshEngn::ECS* m_ecs;
};