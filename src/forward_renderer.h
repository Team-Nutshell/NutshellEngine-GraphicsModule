#pragma once
#include "common.h"

class ForwardRenderer {
public:
	void init(VkDevice device, VkQueue graphicsQueue, uint32_t graphicsQueueFamilyIndex, VkViewport viewport, VkRect2D scissor, uint32_t framesInFlight, const std::vector<HostVisibleVulkanBuffer>& cameraBuffers, const std::vector<HostVisibleVulkanBuffer>& lightBuffers, const std::vector<HostVisibleVulkanBuffer>& objectBuffers, VulkanBuffer meshBuffer, const std::vector<HostVisibleVulkanBuffer>& jointTransformBuffers, const std::vector<HostVisibleVulkanBuffer>& materialBuffers, const std::vector<HostVisibleVulkanBuffer>& shadowSceneBuffers, PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR, PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR, PFN_vkCmdPipelineBarrier2KHR vkCmdPipelineBarrier2KHR);
	void destroy();

	void draw(float dt, VkCommandBuffer commandBuffer, uint32_t currentFrameInFlight, const std::vector<InternalObject>& objects, const std::vector<InternalMesh>& meshes, const NtshEngn::Math::vec3& cameraPosition, const VulkanImage& colorImage, const VulkanImage& depthImage);

	void onResize(uint32_t width, uint32_t height);

	void descriptorSetNeedsUpdate(uint32_t frameInFlight);
	void updateDescriptorSets(uint32_t frameInFlight, const std::vector<InternalTexture>& textures, const std::vector<VkImageView>& textureImageViews, const std::unordered_map<std::string, VkSampler>& textureSamplers);

	void shadowDescriptorSetNeedsUpdate(uint32_t frameInFlight);
	void updateShadowDescriptorSets(uint32_t frameInFlight, const std::vector<VulkanImage>& shadowMaps, VkSampler shadowMapSampler);

	bool createGraphicsPipelineFromFragmentShader(const std::string& fragmentShader);

private:
	void createDescriptorSetLayout();

	void createDescriptorSets(const std::vector<HostVisibleVulkanBuffer>& cameraBuffers, const std::vector<HostVisibleVulkanBuffer>& lightBuffers, const std::vector<HostVisibleVulkanBuffer>& objectBuffers, VulkanBuffer meshBuffer, const std::vector<HostVisibleVulkanBuffer>& jointTransformBuffers, const std::vector<HostVisibleVulkanBuffer>& materialBuffers, const std::vector<HostVisibleVulkanBuffer>& shadowSceneBuffers);

private:
	VkShaderModule m_customVertexShaderModule = VK_NULL_HANDLE;
	VkPipelineLayout m_customGraphicsPipelineLayout = VK_NULL_HANDLE;
	std::unordered_map<std::string, VkPipeline> m_customGraphicsPipelines;

	float m_time = 0.0f;

	VkDescriptorSetLayout m_descriptorSetLayout;
	VkDescriptorPool m_descriptorPool;
	std::vector<VkDescriptorSet> m_descriptorSets;
	std::vector<bool> m_descriptorSetsNeedUpdate;
	std::vector<bool> m_descriptorSetsShadowNeedUpdate;

	VkDevice m_device;
	VkQueue m_graphicsQueue;
	uint32_t m_graphicsQueueFamilyIndex;
	VkViewport m_viewport;
	VkRect2D m_scissor;
	uint32_t m_framesInFlight;

	PFN_vkCmdBeginRenderingKHR m_vkCmdBeginRenderingKHR;
	PFN_vkCmdEndRenderingKHR m_vkCmdEndRenderingKHR;
	PFN_vkCmdPipelineBarrier2KHR m_vkCmdPipelineBarrier2KHR;
};