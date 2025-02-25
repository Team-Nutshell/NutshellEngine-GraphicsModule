#pragma once
#include "common.h"

typedef std::array<NtshEngn::Math::vec4, 6> Frustum;

struct InternalFrustumCullingInfo {
	Frustum frustum;
	uint64_t bufferAddress;
	NtshEngn::Math::vec2 padding;
};

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
		NtshEngn::ECSInterface* ecs);
	void destroy();

	uint32_t cull(VkCommandBuffer commandBuffer,
		uint32_t currentFrameInFlight,
		const std::vector<FrustumCullingInfo>& frustumCullingInfos,
		const std::unordered_map<NtshEngn::Entity, InternalObject>& objects,
		const std::vector<InternalMesh>& meshes);

private:
	void createBuffers();

	void createDescriptorSetLayout();

	void createComputePipeline();

	void createDescriptorSets();

	Frustum calculateFrustumPlanes(const NtshEngn::Math::mat4& viewProj);

private:
	std::vector<HostVisibleVulkanBuffer> m_inDrawIndirectBuffers;
	std::vector<HostVisibleVulkanBuffer> m_inPerDrawBuffers;
	std::vector<HostVisibleVulkanBuffer> m_frustumCullingInfoBuffers;
	std::vector<HostVisibleVulkanBuffer> m_frustumCullingObjectBuffers;

	VkDescriptorSetLayout m_descriptorSetLayout;

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

	NtshEngn::ECSInterface* m_ecs;
};