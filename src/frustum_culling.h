#pragma once
#include "common.h"
#include "../Common/job_system/ntshengn_job_system.h"

#if FRUSTUM_CULLING_TYPE == FRUSTUM_CULLING_GPU
struct FrustumCullingObject {
	NtshEngn::Math::vec4 position;
	NtshEngn::Math::mat4 rotation;
	NtshEngn::Math::vec4 scale;
	NtshEngn::Math::vec4 aabbMin;
	NtshEngn::Math::vec4 aabbMax;
};
#endif

class FrustumCulling {
public:
	void init(VkDevice device,
		VkQueue computeQueue,
		uint32_t computeQueueFamilyIndex,
		VmaAllocator allocator,
		uint32_t framesInFlight,
		PFN_vkCmdPipelineBarrier2KHR vkCmdPipelineBarrier2KHR,
		NtshEngn::JobSystem* jobSystem,
		NtshEngn::ECS* ecs);
	void destroy();

	uint32_t culling(VkCommandBuffer commandBuffer,
		uint32_t currentFrameInFlight,
		const NtshEngn::Math::mat4& cameraView,
		const NtshEngn::Math::mat4& cameraProjection,
		const std::unordered_map<NtshEngn::Entity, InternalObject>& objects,
		const std::vector<InternalMesh>& meshes);

	VulkanBuffer& getDrawIndirectBuffer(uint32_t frameInFlight);
	std::vector<VkBuffer> getPerDrawBuffers();

private:
	void createBuffers();

#if FRUSTUM_CULLING_TYPE == FRUSTUM_CULLING_GPU
	void createDescriptorSetLayout();

	void createComputePipeline();

	void createDescriptorSets();
#endif

private:
	std::vector<HostVisibleVulkanBuffer> m_drawIndirectBuffers;
	std::vector<HostVisibleVulkanBuffer> m_perDrawBuffers;

#if FRUSTUM_CULLING_TYPE == FRUSTUM_CULLING_GPU
	std::vector<HostVisibleVulkanBuffer> m_gpuFrustumCullingBuffers;
	VulkanBuffer m_gpuDrawIndirectBuffer;
	VulkanBuffer m_gpuPerDrawBuffer;

	VkDescriptorSetLayout m_descriptorSetLayout;

	VkPipeline m_computePipeline;
	VkPipelineLayout m_computePipelineLayout;

	VkDescriptorPool m_descriptorPool;
	std::vector<VkDescriptorSet> m_descriptorSets;
#endif

	VkDevice m_device;
	VkQueue m_computeQueue;
	uint32_t m_computeQueueFamilyIndex;
	VmaAllocator m_allocator;
	uint32_t m_framesInFlight;

	PFN_vkCmdPipelineBarrier2KHR m_vkCmdPipelineBarrier2KHR;

	NtshEngn::JobSystem* m_jobSystem;
	NtshEngn::ECS* m_ecs;
};