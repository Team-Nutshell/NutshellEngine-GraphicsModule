#pragma once
#include "common.h"

class GBuffer {
public:
	void init(VkDevice device,
		VkQueue graphicsQueue,
		uint32_t graphicsQueueFamilyIndex,
		VmaAllocator allocator,
		VkFence initializationFence,
		VkViewport viewport,
		VkRect2D scissor,
		uint32_t framesInFlight,
		const std::vector<VkBuffer>& cameraBuffers,
		const std::vector<VkBuffer>& objectBuffers,
		const std::vector<VkBuffer>& materialBuffers,
		PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR,
		PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR,
		PFN_vkCmdPipelineBarrier2KHR vkCmdPipelineBarrier2KHR
	);
	void destroy();

	void draw(VkCommandBuffer commandBuffer,
		uint32_t currentFrameInFlight,
		const std::unordered_map<NtshEngn::Entity, InternalObject>& objects,
		const std::vector<InternalMesh>& meshes,
		VkBuffer vertexBuffer,
		VkBuffer indexBuffer
	);
	void descriptorSetNeedsUpdate(uint32_t frameInFlight);
	void updateDescriptorSets(uint32_t frameInFlight,
		const std::vector<InternalTexture>& textures,
		const std::vector<VkImageView>& textureImageViews,
		const std::unordered_map<std::string, VkSampler>& textureSamplers
	);

	void onResize(uint32_t width, uint32_t height);

	VulkanImage& getPosition();
	VulkanImage& getNormal();
	VulkanImage& getDiffuse();
	VulkanImage& getMaterial();
	VulkanImage& getEmissive();

private:
	void createImages(uint32_t width, uint32_t height);
	void destroyImages();

	void createDescriptorSetLayout();

	void createGraphicsPipeline();

	void createDescriptorSets(const std::vector<VkBuffer>& cameraBuffers,
		const std::vector<VkBuffer>& objectBuffers,
		const std::vector<VkBuffer>& materialBuffers
	);

private:
	VulkanImage m_position;
	VulkanImage m_normal;
	VulkanImage m_diffuse;
	VulkanImage m_material;
	VulkanImage m_emissive;
	VulkanImage m_depth;

	VkDescriptorSetLayout m_descriptorSetLayout;

	VkPipeline m_graphicsPipeline;
	VkPipelineLayout m_graphicsPipelineLayout;

	VkDescriptorPool m_descriptorPool;
	std::vector<VkDescriptorSet> m_descriptorSets;
	std::vector<bool> m_descriptorSetsNeedUpdate;

	VkDevice m_device;
	VkQueue m_graphicsQueue;
	uint32_t m_graphicsQueueFamilyIndex;
	VmaAllocator m_allocator;
	VkFence m_initializationFence;
	VkViewport m_viewport;
	VkRect2D m_scissor;
	uint32_t m_framesInFlight;

	PFN_vkCmdBeginRenderingKHR m_vkCmdBeginRenderingKHR;
	PFN_vkCmdEndRenderingKHR m_vkCmdEndRenderingKHR;
	PFN_vkCmdPipelineBarrier2KHR m_vkCmdPipelineBarrier2KHR;
};