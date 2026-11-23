#pragma once
#include "common.h"
#include <queue>

class UI {
public:
	void init(VkDevice device, VkQueue graphicsQueue, uint32_t graphicsQueueFamilyIndex, VmaAllocator allocator, VkCommandPool initializationCommandPool, VkCommandBuffer initializationCommandBuffer, VkFence initializationFence, VkFormat drawImageFormat, VkViewport viewport, VkRect2D scissor, uint32_t framesInFlight, PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR, PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR, PFN_vkCmdPipelineBarrier2KHR vkCmdPipelineBarrier2KHR);
	void destroy();

	void draw(VkCommandBuffer commandBuffer, uint32_t currentFrameInFlight, VkImageView drawImageView);

	void textDescriptorSetNeedsUpdate(uint32_t frameInFlight);
	void updateTextDescriptorSet(uint32_t frameInFlight, const std::vector<VkDescriptorImageInfo>& texturesDescriptorImageInfos);
	void imageDescriptorSetNeedsUpdate(uint32_t frameInFlight);
	void updateImageDescriptorSet(uint32_t frameInFlight, const std::vector<VkDescriptorImageInfo>& texturesDescriptorImageInfos);

	void onResize(uint32_t width, uint32_t height);

	void clearQueues();

	void setNearestSamplerKey(const std::string& nearestSamplerKey);
	std::string getNearestSamplerKey();
	void setLinearSamplerKey(const std::string& linearSamplerKey);
	std::string getLinearSamplerKey();

	void drawText(const InternalFont& font, uint32_t currentFrameInFlight, const std::wstring& text, NtshEngn::AnchorPoint anchorPoint, NtshEngn::CoordinateType coordinateType, const NtshEngn::Math::vec2& position, float rotation, const NtshEngn::Math::vec2& scale, const NtshEngn::Math::vec4& color);
	void drawLine(NtshEngn::CoordinateType coordinateType, const NtshEngn::Math::vec2& start, const NtshEngn::Math::vec2& end, const NtshEngn::Math::vec4& color);
	void drawRectangle(NtshEngn::CoordinateType coordinateType, const NtshEngn::Math::vec2& position, const NtshEngn::Math::vec2& size, const NtshEngn::Math::vec4& color);
	void drawImage(uint32_t textureIndex, const NtshEngn::Math::vec2& imageSize, NtshEngn::AnchorPoint anchorPoint, NtshEngn::CoordinateType coordinateType, const NtshEngn::Math::vec2& position, float rotation, const NtshEngn::Math::vec2& scale, const NtshEngn::Math::vec4& color);

private:
	void createTextResources(VkFormat drawImageFormat);
	void createLineResources(VkFormat drawImageFormat);
	void createRectangleResources(VkFormat drawImageFormat);
	void createImageResources(VkFormat drawImageFormat);

private:
	std::string m_nearestSamplerKey;
	std::string m_linearSamplerKey;

	std::vector<HostVisibleVulkanBuffer> m_textBuffers;
	VkDescriptorSetLayout m_textDescriptorSetLayout;
	VkDescriptorPool m_textDescriptorPool;
	std::vector<VkDescriptorSet> m_textDescriptorSets;
	std::vector<bool> m_textDescriptorSetsNeedUpdate;
	VkPipeline m_textGraphicsPipeline;
	VkPipelineLayout m_textGraphicsPipelineLayout;

	VkPipeline m_lineGraphicsPipeline;
	VkPipelineLayout m_lineGraphicsPipelineLayout;

	VkPipeline m_rectangleGraphicsPipeline;
	VkPipelineLayout m_rectangleGraphicsPipelineLayout;

	VkDescriptorSetLayout m_imageDescriptorSetLayout;
	VkDescriptorPool m_imageDescriptorPool;
	std::vector<VkDescriptorSet> m_imageDescriptorSets;
	std::vector<bool> m_imageDescriptorSetsNeedUpdate;
	VkPipeline m_imageGraphicsPipeline;
	VkPipelineLayout m_imageGraphicsPipelineLayout;

	std::queue<UIElement> m_elements;

	std::queue<InternalUIText> m_texts;
	uint32_t m_textBufferOffset = 0;

	std::queue<InternalUILine> m_lines;

	std::queue<InternalUIRectangle> m_rectangles;

	std::queue<InternalUIImage> m_images;

	VkDevice m_device;
	VkQueue m_graphicsQueue;
	uint32_t m_graphicsQueueFamilyIndex;
	VmaAllocator m_allocator;
	VkCommandPool m_initializationCommandPool;
	VkCommandBuffer m_initializationCommandBuffer;
	VkFence m_initializationFence;
	VkViewport m_viewport;
	VkRect2D m_scissor;
	uint32_t m_framesInFlight;

	PFN_vkCmdBeginRenderingKHR m_vkCmdBeginRenderingKHR;
	PFN_vkCmdEndRenderingKHR m_vkCmdEndRenderingKHR;
	PFN_vkCmdPipelineBarrier2KHR m_vkCmdPipelineBarrier2KHR;
};