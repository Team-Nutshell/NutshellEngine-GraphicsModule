#pragma once
#include "common.h"
#include "../Common/job_system/ntshengn_job_system_interface.h"

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
		NtshEngn::JobSystemInterface* jobSystem,
		NtshEngn::ECSInterface* ecs);
	void destroy();

	uint32_t cull(VkCommandBuffer commandBuffer,
		uint32_t currentFrameInFlight,
		const NtshEngn::Math::mat4& cameraViewProj,
		const std::vector<NtshEngn::Math::mat4>& lightViewProjs,
		const std::unordered_map<NtshEngn::Entity, InternalObject>& objects,
		const std::vector<InternalMesh>& meshes);

	VulkanBuffer& getCameraDrawIndirectBuffer(uint32_t frameInFlight);
	std::vector<VkBuffer> getCameraPerDrawBuffers();

private:
	void createBuffers();

#if FRUSTUM_CULLING_TYPE == FRUSTUM_CULLING_GPU
	void createDescriptorSetLayout();

	void createComputePipeline();

	void createDescriptorSets();
#endif

	std::array<NtshEngn::Math::vec4, 6> calculateFrustumPlanes(const NtshEngn::Math::mat4& viewProj);

	bool intersect(const std::array<NtshEngn::Math::vec4, 6>& frustum, const NtshEngn::Math::vec3& aabbMin, const NtshEngn::Math::vec3& aabbMax);

private:
	std::vector<HostVisibleVulkanBuffer> m_cameraDrawIndirectBuffers;
	std::vector<HostVisibleVulkanBuffer> m_cameraPerDrawBuffers;

#if FRUSTUM_CULLING_TYPE == FRUSTUM_CULLING_GPU
	std::vector<HostVisibleVulkanBuffer> m_gpuFrustumCullingBuffers;
	VulkanBuffer m_gpuCameraDrawIndirectBuffer;
	VulkanBuffer m_gpuCameraPerDrawBuffer;

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

	NtshEngn::JobSystemInterface* m_jobSystem;
	NtshEngn::ECSInterface* m_ecs;
};