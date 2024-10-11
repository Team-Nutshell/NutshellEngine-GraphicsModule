#pragma once
#include "common.h"
#include "../Common/job_system/ntshengn_job_system_interface.h"

typedef std::array<NtshEngn::Math::vec4, 6> Frustum;

struct FrustumCullingObject {
	NtshEngn::Math::vec4 position;
	NtshEngn::Math::mat4 rotation;
	NtshEngn::Math::vec4 scale;
	NtshEngn::Math::vec4 aabbMin;
	NtshEngn::Math::vec4 aabbMax;
};

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
		const std::vector<FrustumCullingInfo>& frustumCullingInfo,
		const std::unordered_map<NtshEngn::Entity, InternalObject>& objects,
		const std::vector<InternalMesh>& meshes);

	VkDescriptorSetLayout getDescriptorSet1Layout();

private:
	void createBuffers();

	void createDescriptorSetLayouts();
	void createDescriptorSet0Layout();
	void createDescriptorSet1Layout();

	void createComputePipeline();

	void createDescriptorSets();

	Frustum calculateFrustumPlanes(const NtshEngn::Math::mat4& viewProj);

	bool intersect(const Frustum& frustum, const NtshEngn::Math::vec3& aabbMin, const NtshEngn::Math::vec3& aabbMax);

private:
	std::vector<HostVisibleVulkanBuffer> m_inDrawIndirectBuffers;
	std::vector<HostVisibleVulkanBuffer> m_inPerDrawBuffers;
	std::vector<HostVisibleVulkanBuffer> m_frustumBuffers;
	std::vector<HostVisibleVulkanBuffer> m_frustumCullingObjectBuffers;

	VkDescriptorSetLayout m_descriptorSet0Layout;
	VkDescriptorSetLayout m_descriptorSet1Layout;

	VkPipeline m_computePipeline;
	VkPipelineLayout m_computePipelineLayout;

	VkDescriptorPool m_descriptorPool;
	std::vector<VkDescriptorSet> m_descriptorSets;

	VkDevice m_device;
	VkQueue m_computeQueue;
	uint32_t m_computeQueueFamilyIndex;
	VmaAllocator m_allocator;
	uint32_t m_framesInFlight;

	PFN_vkCmdPipelineBarrier2KHR m_vkCmdPipelineBarrier2KHR;

	NtshEngn::JobSystemInterface* m_jobSystem;
	NtshEngn::ECSInterface* m_ecs;
};