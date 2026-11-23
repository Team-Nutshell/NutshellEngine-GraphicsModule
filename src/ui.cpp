#include "ui.h"

void UI::init(VkDevice device, VkQueue graphicsQueue, uint32_t graphicsQueueFamilyIndex, VmaAllocator allocator, VkCommandPool initializationCommandPool, VkCommandBuffer initializationCommandBuffer, VkFence initializationFence, VkFormat drawImageFormat, VkViewport viewport, VkRect2D scissor, uint32_t framesInFlight, PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHR, PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHR, PFN_vkCmdPipelineBarrier2KHR vkCmdPipelineBarrier2KHR) {
	m_device = device;
	m_graphicsQueue = graphicsQueue;
	m_graphicsQueueFamilyIndex = graphicsQueueFamilyIndex;
	m_allocator = allocator;
	m_initializationCommandPool = initializationCommandPool;
	m_initializationCommandBuffer = initializationCommandBuffer;
	m_initializationFence = initializationFence;
	m_viewport = viewport;
	m_scissor = scissor;
	m_framesInFlight = framesInFlight;
	m_vkCmdBeginRenderingKHR = vkCmdBeginRenderingKHR;
	m_vkCmdEndRenderingKHR = vkCmdEndRenderingKHR;
	m_vkCmdPipelineBarrier2KHR = vkCmdPipelineBarrier2KHR;

	createTextResources(drawImageFormat);
	createLineResources(drawImageFormat);
	createRectangleResources(drawImageFormat);
	createImageResources(drawImageFormat);
}

void UI::destroy() {
	vkDestroyDescriptorPool(m_device, m_imageDescriptorPool, nullptr);
	vkDestroyPipeline(m_device, m_imageGraphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_imageGraphicsPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(m_device, m_imageDescriptorSetLayout, nullptr);

	vkDestroyPipeline(m_device, m_rectangleGraphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_rectangleGraphicsPipelineLayout, nullptr);

	vkDestroyPipeline(m_device, m_lineGraphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_lineGraphicsPipelineLayout, nullptr);

	vkDestroyDescriptorPool(m_device, m_textDescriptorPool, nullptr);
	vkDestroyPipeline(m_device, m_textGraphicsPipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_textGraphicsPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(m_device, m_textDescriptorSetLayout, nullptr);
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		m_textBuffers[i].destroy(m_allocator);
	}
}

void UI::draw(VkCommandBuffer commandBuffer, uint32_t currentFrameInFlight, VkImageView drawImageView) {
	if (!m_elements.empty()) {
		VkRenderingAttachmentInfo renderingSwapchainAttachmentInfo = {};
		renderingSwapchainAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		renderingSwapchainAttachmentInfo.pNext = nullptr;
		renderingSwapchainAttachmentInfo.imageView = drawImageView;
		renderingSwapchainAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
		renderingSwapchainAttachmentInfo.resolveMode = VK_RESOLVE_MODE_NONE;
		renderingSwapchainAttachmentInfo.resolveImageView = VK_NULL_HANDLE;
		renderingSwapchainAttachmentInfo.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		renderingSwapchainAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
		renderingSwapchainAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		renderingSwapchainAttachmentInfo.clearValue.color = { 0.0f, 0.0f, 0.0f, 0.0f };

		VkRenderingInfo uiRenderingInfo = {};
		uiRenderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
		uiRenderingInfo.pNext = nullptr;
		uiRenderingInfo.flags = 0;
		uiRenderingInfo.renderArea = m_scissor;
		uiRenderingInfo.layerCount = 1;
		uiRenderingInfo.viewMask = 0;
		uiRenderingInfo.colorAttachmentCount = 1;
		uiRenderingInfo.pColorAttachments = &renderingSwapchainAttachmentInfo;
		uiRenderingInfo.pDepthAttachment = nullptr;
		uiRenderingInfo.pStencilAttachment = nullptr;
		m_vkCmdBeginRenderingKHR(commandBuffer, &uiRenderingInfo);

		while (!m_elements.empty()) {
			const UIElement uiElement = m_elements.front();
			if (uiElement == UIElement::Text) {
				const InternalUIText& uiText = m_texts.front();

				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_textGraphicsPipeline);
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_textGraphicsPipelineLayout, 0, 1, &m_textDescriptorSets[currentFrameInFlight], 0, nullptr);
				vkCmdPushConstants(commandBuffer, m_textGraphicsPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(uint32_t), &uiText.bufferOffset);
				vkCmdPushConstants(commandBuffer, m_textGraphicsPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(NtshEngn::Math::vec4), sizeof(NtshEngn::Math::vec4) + (sizeof(uint32_t) * 2), &uiText.color);

				vkCmdDraw(commandBuffer, uiText.charactersCount * 6, 1, 0, 0);

				m_texts.pop();
			}
			else if (uiElement == UIElement::Line) {
				const InternalUILine& uiLine = m_lines.front();

				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_lineGraphicsPipeline);
				vkCmdPushConstants(commandBuffer, m_lineGraphicsPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(NtshEngn::Math::vec2) + sizeof(NtshEngn::Math::vec2), &uiLine.positions);
				vkCmdPushConstants(commandBuffer, m_lineGraphicsPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(NtshEngn::Math::vec2) + sizeof(NtshEngn::Math::vec2), sizeof(NtshEngn::Math::vec4), &uiLine.color);

				vkCmdDraw(commandBuffer, 2, 1, 0, 0);

				m_lines.pop();
			}
			else if (uiElement == UIElement::Rectangle) {
				const InternalUIRectangle& uiRectangle = m_rectangles.front();

				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_rectangleGraphicsPipeline);
				vkCmdPushConstants(commandBuffer, m_rectangleGraphicsPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(NtshEngn::Math::vec2) + sizeof(NtshEngn::Math::vec2), &uiRectangle.positions);
				vkCmdPushConstants(commandBuffer, m_rectangleGraphicsPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, sizeof(NtshEngn::Math::vec2) + sizeof(NtshEngn::Math::vec2), sizeof(NtshEngn::Math::vec4), &uiRectangle.color);

				vkCmdDraw(commandBuffer, 6, 1, 0, 0);

				m_rectangles.pop();
			}
			else if (uiElement == UIElement::Image) {
				const InternalUIImage& uiImage = m_images.front();

				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_imageGraphicsPipeline);
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_imageGraphicsPipelineLayout, 0, 1, &m_imageDescriptorSets[currentFrameInFlight], 0, nullptr);
				vkCmdPushConstants(commandBuffer, m_imageGraphicsPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, 5 * sizeof(NtshEngn::Math::vec2), &uiImage.v0);
				vkCmdPushConstants(commandBuffer, m_imageGraphicsPipelineLayout, VK_SHADER_STAGE_FRAGMENT_BIT, 5 * sizeof(NtshEngn::Math::vec2) + sizeof(NtshEngn::Math::vec2), sizeof(NtshEngn::Math::vec4) + sizeof(uint32_t), &uiImage.color);

				vkCmdDraw(commandBuffer, 6, 1, 0, 0);

				m_images.pop();
			}

			m_elements.pop();
		}

		m_vkCmdEndRenderingKHR(commandBuffer);
	}

	m_textBufferOffset = 0;
}

void UI::textDescriptorSetNeedsUpdate(uint32_t frameInFlight) {
	m_textDescriptorSetsNeedUpdate[frameInFlight] = true;
}

void UI::updateTextDescriptorSet(uint32_t frameInFlight, const std::vector<VkDescriptorImageInfo>& texturesDescriptorImageInfos) {
	if (!m_textDescriptorSetsNeedUpdate[frameInFlight]) {
		return;
	}

	VkWriteDescriptorSet texturesDescriptorWriteDescriptorSet = {};
	texturesDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	texturesDescriptorWriteDescriptorSet.pNext = nullptr;
	texturesDescriptorWriteDescriptorSet.dstSet = m_textDescriptorSets[frameInFlight];
	texturesDescriptorWriteDescriptorSet.dstBinding = 1;
	texturesDescriptorWriteDescriptorSet.dstArrayElement = 0;
	texturesDescriptorWriteDescriptorSet.descriptorCount = static_cast<uint32_t>(texturesDescriptorImageInfos.size());
	texturesDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	texturesDescriptorWriteDescriptorSet.pImageInfo = texturesDescriptorImageInfos.data();
	texturesDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
	texturesDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

	vkUpdateDescriptorSets(m_device, 1, &texturesDescriptorWriteDescriptorSet, 0, nullptr);

	m_textDescriptorSetsNeedUpdate[frameInFlight] = false;
}

void UI::imageDescriptorSetNeedsUpdate(uint32_t frameInFlight) {
	m_imageDescriptorSetsNeedUpdate[frameInFlight] = true;
}

void UI::updateImageDescriptorSet(uint32_t frameInFlight, const std::vector<VkDescriptorImageInfo>& texturesDescriptorImageInfos) {
	if (!m_imageDescriptorSetsNeedUpdate[frameInFlight]) {
		return;
	}

	VkWriteDescriptorSet texturesDescriptorWriteDescriptorSet = {};
	texturesDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	texturesDescriptorWriteDescriptorSet.pNext = nullptr;
	texturesDescriptorWriteDescriptorSet.dstSet = m_imageDescriptorSets[frameInFlight];
	texturesDescriptorWriteDescriptorSet.dstBinding = 0;
	texturesDescriptorWriteDescriptorSet.dstArrayElement = 0;
	texturesDescriptorWriteDescriptorSet.descriptorCount = static_cast<uint32_t>(texturesDescriptorImageInfos.size());
	texturesDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	texturesDescriptorWriteDescriptorSet.pImageInfo = texturesDescriptorImageInfos.data();
	texturesDescriptorWriteDescriptorSet.pBufferInfo = nullptr;
	texturesDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

	vkUpdateDescriptorSets(m_device, 1, &texturesDescriptorWriteDescriptorSet, 0, nullptr);

	m_imageDescriptorSetsNeedUpdate[frameInFlight] = false;
}

void UI::clearQueues() {
	m_elements = std::queue<UIElement>();
	m_texts = std::queue<InternalUIText>();
	m_lines = std::queue<InternalUILine>();
	m_rectangles = std::queue<InternalUIRectangle>();
	m_images = std::queue<InternalUIImage>();
	m_textBufferOffset = 0;
}

void UI::setNearestSamplerKey(const std::string& nearestSamplerKey) {
	m_nearestSamplerKey = nearestSamplerKey;
}

std::string UI::getNearestSamplerKey() {
	return m_nearestSamplerKey;
}

void UI::setLinearSamplerKey(const std::string& linearSamplerKey) {
	m_linearSamplerKey = linearSamplerKey;
}

std::string UI::getLinearSamplerKey() {
	return m_linearSamplerKey;
}

void UI::drawText(const InternalFont& font, uint32_t currentFrameInFlight, const std::wstring& text, NtshEngn::AnchorPoint anchorPoint, NtshEngn::CoordinateType coordinateType, const NtshEngn::Math::vec2& position, float rotation, const NtshEngn::Math::vec2& scale, const NtshEngn::Math::vec4& color) {
	NtshEngn::Math::vec2 finalPosition;
	if (coordinateType == NtshEngn::CoordinateType::Pixel) {
		finalPosition = position;
	}
	else {
		finalPosition = NtshEngn::Math::vec2(position.x * m_viewport.width, position.y * m_viewport.height);
	}

	NtshEngn::Math::vec2 min = NtshEngn::Math::vec2(0.0f, std::min(font.topLeft.y, font.bottomRight.y));
	NtshEngn::Math::vec2 max = NtshEngn::Math::vec2(std::max(font.topLeft.x, font.bottomRight.x), std::max(font.topLeft.y, font.bottomRight.y));
	float positionAdvance = 0.0f;
	float positionHeight = 0.0f;
	std::vector<NtshEngn::Math::vec2> positionsAndUVs;
	for (const wchar_t& c : text) {
		if (c == '\n') {
			max.x = std::max(max.x, positionAdvance);
			positionAdvance = 0.0f;
			positionHeight += font.height;

			continue;
		}

		if (font.glyphs.find(c) != font.glyphs.end()) {
			const NtshEngn::FontGlyph& glyph = font.glyphs.at(c);
			const NtshEngn::Math::vec2 topLeft = glyph.positionTopLeft + NtshEngn::Math::vec2(positionAdvance, positionHeight);
			const NtshEngn::Math::vec2 bottomRight = glyph.positionBottomRight + NtshEngn::Math::vec2(positionAdvance, positionHeight);
			positionsAndUVs.push_back(topLeft);
			positionsAndUVs.push_back(glyph.uvTopLeft);
			positionsAndUVs.push_back(bottomRight);
			positionsAndUVs.push_back(glyph.uvBottomRight);

			positionAdvance += glyph.positionAdvance;
		}
	}
	max.x = std::max(max.x, positionAdvance);
	max.y += positionHeight;

	const NtshEngn::Math::vec2 middle = (min + max) / 2.0f;
	NtshEngn::Math::vec2 positionOffset;
	if (anchorPoint == NtshEngn::AnchorPoint::TopLeft) {
		positionOffset.x = -min.x;
		positionOffset.y = -min.y;
	}
	else if (anchorPoint == NtshEngn::AnchorPoint::TopRight) {
		positionOffset.x = -max.x;
		positionOffset.y = -min.y;
	}
	else if (anchorPoint == NtshEngn::AnchorPoint::BottomLeft) {
		positionOffset.x = -min.x;
		positionOffset.y = -max.y;
	}
	else if (anchorPoint == NtshEngn::AnchorPoint::BottomRight) {
		positionOffset.x = -max.x;
		positionOffset.y = -max.y;
	}
	else if (anchorPoint == NtshEngn::AnchorPoint::TopCenter) {
		positionOffset.x = -middle.x;
		positionOffset.y = -min.y;
	}
	else if (anchorPoint == NtshEngn::AnchorPoint::BottomCenter) {
		positionOffset.x = -middle.x;
		positionOffset.y = -max.y;
	}
	else if (anchorPoint == NtshEngn::AnchorPoint::LeftCenter) {
		positionOffset.x = -min.x;
		positionOffset.y = -middle.y;
	}
	else if (anchorPoint == NtshEngn::AnchorPoint::RightCenter) {
		positionOffset.x = -max.x;
		positionOffset.y = -middle.y;
	}
	else {
		positionOffset.x = -middle.x;
		positionOffset.y = -middle.y;
	}

	NtshEngn::Math::mat3 transform = NtshEngn::Math::translate(finalPosition) * NtshEngn::Math::rotate(rotation) * NtshEngn::Math::scale(scale) * NtshEngn::Math::translate(positionOffset);
	std::vector<NtshEngn::Math::vec4> vertices(positionsAndUVs.size());
	for (size_t i = 0; i < positionsAndUVs.size(); i += 4) {
		const NtshEngn::Math::vec2& topLeftPosition = positionsAndUVs[i + 0];
		const NtshEngn::Math::vec2& topLeftUV = positionsAndUVs[i + 1];
		const NtshEngn::Math::vec2& bottomRightPosition = positionsAndUVs[i + 2];
		const NtshEngn::Math::vec2& bottomRightUV = positionsAndUVs[i + 3];
		vertices[i + 0] = NtshEngn::Math::vec4(NtshEngn::Math::vec2(transform * NtshEngn::Math::vec3(bottomRightPosition.x, topLeftPosition.y, 1.0f)), NtshEngn::Math::vec2(bottomRightUV.x, topLeftUV.y));
		vertices[i + 0].x = (vertices[i + 0].x / m_viewport.width) * 2.0f - 1.0f;
		vertices[i + 0].y = (vertices[i + 0].y / m_viewport.height) * 2.0f - 1.0f;
		vertices[i + 1] = NtshEngn::Math::vec4(NtshEngn::Math::vec2(transform * NtshEngn::Math::vec3(topLeftPosition.x, topLeftPosition.y, 1.0f)), NtshEngn::Math::vec2(topLeftUV.x, topLeftUV.y));
		vertices[i + 1].x = (vertices[i + 1].x / m_viewport.width) * 2.0f - 1.0f;
		vertices[i + 1].y = (vertices[i + 1].y / m_viewport.height) * 2.0f - 1.0f;
		vertices[i + 2] = NtshEngn::Math::vec4(NtshEngn::Math::vec2(transform * NtshEngn::Math::vec3(topLeftPosition.x, bottomRightPosition.y, 1.0f)), NtshEngn::Math::vec2(topLeftUV.x, bottomRightUV.y));
		vertices[i + 2].x = (vertices[i + 2].x / m_viewport.width) * 2.0f - 1.0f;
		vertices[i + 2].y = (vertices[i + 2].y / m_viewport.height) * 2.0f - 1.0f;
		vertices[i + 3] = NtshEngn::Math::vec4(NtshEngn::Math::vec2(transform * NtshEngn::Math::vec3(bottomRightPosition.x, bottomRightPosition.y, 1.0f)), NtshEngn::Math::vec2(bottomRightUV.x, bottomRightUV.y));
		vertices[i + 3].x = (vertices[i + 3].x / m_viewport.width) * 2.0f - 1.0f;
		vertices[i + 3].y = (vertices[i + 3].y / m_viewport.height) * 2.0f - 1.0f;
	}

	size_t offset = m_textBufferOffset * sizeof(NtshEngn::Math::vec4) * 4;
	memcpy(reinterpret_cast<uint8_t*>(m_textBuffers[currentFrameInFlight].address) + offset, vertices.data(), sizeof(NtshEngn::Math::vec4) * vertices.size());

	InternalUIText uiText;
	uiText.color = color;
	uiText.fontTextureIndex = font.fontTextureIndex;
	uiText.fontType = font.type;
	uiText.charactersCount = static_cast<uint32_t>(vertices.size() / 4);
	uiText.bufferOffset = m_textBufferOffset;
	m_texts.push(uiText);

	m_textBufferOffset += static_cast<uint32_t>(uiText.charactersCount);

	m_elements.push(UIElement::Text);
}

void UI::drawLine(NtshEngn::CoordinateType coordinateType, const NtshEngn::Math::vec2& start, const NtshEngn::Math::vec2& end, const NtshEngn::Math::vec4& color) {
	NtshEngn::Math::vec2 finalStart;
	NtshEngn::Math::vec2 finalEnd;
	if (coordinateType == NtshEngn::CoordinateType::Pixel) {
		finalStart = start;
		finalEnd = end;
	}
	else {
		finalStart = NtshEngn::Math::vec2(start.x * m_viewport.width, start.y * m_viewport.height);
		finalEnd = NtshEngn::Math::vec2(end.x * m_viewport.width, end.y * m_viewport.height);;
	}

	InternalUILine uiLine;
	uiLine.positions = NtshEngn::Math::vec4((finalStart.x / m_viewport.width) * 2.0f - 1.0f, (finalStart.y / m_viewport.height) * 2.0f - 1.0f, (finalEnd.x / m_viewport.width) * 2.0f - 1.0f, (finalEnd.y / m_viewport.height) * 2.0f - 1.0f);
	uiLine.color = color;
	m_lines.push(uiLine);

	m_elements.push(UIElement::Line);
}

void UI::drawRectangle(NtshEngn::CoordinateType coordinateType, const NtshEngn::Math::vec2& position, const NtshEngn::Math::vec2& size, const NtshEngn::Math::vec4& color) {
	NtshEngn::Math::vec2 finalPosition;
	NtshEngn::Math::vec2 finalSize;
	if (coordinateType == NtshEngn::CoordinateType::Pixel) {
		finalPosition = position;
		finalSize = size;
	}
	else {
		finalPosition = NtshEngn::Math::vec2(position.x * m_viewport.width, position.y * m_viewport.height);
		finalSize = NtshEngn::Math::vec2(size.x * m_viewport.width, size.y * m_viewport.height);
	}

	InternalUIRectangle uiRectangle;
	const NtshEngn::Math::vec2 topLeft = NtshEngn::Math::vec2((finalPosition.x / m_viewport.width) * 2.0f - 1.0f, (finalPosition.y / m_viewport.height) * 2.0f - 1.0f);
	const NtshEngn::Math::vec2 bottomRight = NtshEngn::Math::vec2(((finalPosition.x + finalSize.x) / m_viewport.width) * 2.0f - 1.0f, ((finalPosition.y + finalSize.y) / m_viewport.height) * 2.0f - 1.0f);
	uiRectangle.positions = NtshEngn::Math::vec4(topLeft, bottomRight);
	uiRectangle.color = color;
	m_rectangles.push(uiRectangle);

	m_elements.push(UIElement::Rectangle);
}

void UI::drawImage(uint32_t textureIndex, const NtshEngn::Math::vec2& imageSize, NtshEngn::AnchorPoint anchorPoint, NtshEngn::CoordinateType coordinateType, const NtshEngn::Math::vec2& position, float rotation, const NtshEngn::Math::vec2& scale, const NtshEngn::Math::vec4& color) {
	NtshEngn::Math::vec2 finalPosition;
	if (coordinateType == NtshEngn::CoordinateType::Pixel) {
		finalPosition = position;
	}
	else {
		finalPosition = NtshEngn::Math::vec2(position.x * m_viewport.width, position.y * m_viewport.height);
	}

	const NtshEngn::Math::vec2 halfTextureSizes = imageSize / 2.0;
	NtshEngn::Math::vec2 offset;
	if (anchorPoint == NtshEngn::AnchorPoint::TopLeft) {
		offset.x = halfTextureSizes.x;
		offset.y = halfTextureSizes.y;
	}
	else if (anchorPoint == NtshEngn::AnchorPoint::TopRight) {
		offset.x = -halfTextureSizes.x;
		offset.y = halfTextureSizes.y;
	}
	else if (anchorPoint == NtshEngn::AnchorPoint::BottomLeft) {
		offset.x = halfTextureSizes.x;
		offset.y = -halfTextureSizes.y;
	}
	else if (anchorPoint == NtshEngn::AnchorPoint::BottomRight) {
		offset.x = -halfTextureSizes.x;
		offset.y = -halfTextureSizes.y;
	}
	else if (anchorPoint == NtshEngn::AnchorPoint::TopCenter) {
		offset.x = 0.0f;
		offset.y = halfTextureSizes.y;
	}
	else if (anchorPoint == NtshEngn::AnchorPoint::BottomCenter) {
		offset.x = 0.0f;
		offset.y = -halfTextureSizes.y;
	}
	else if (anchorPoint == NtshEngn::AnchorPoint::LeftCenter) {
		offset.x = halfTextureSizes.x;
		offset.y = 0.0f;
	}
	else if (anchorPoint == NtshEngn::AnchorPoint::RightCenter) {
		offset.x = -halfTextureSizes.x;
		offset.y = 0.0f;
	}
	else {
		offset.x = 0.0f;
		offset.y = 0.0f;
	}

	const NtshEngn::Math::mat3 transform = NtshEngn::Math::translate(finalPosition) * NtshEngn::Math::rotate(rotation) * NtshEngn::Math::scale(NtshEngn::Math::vec2(std::abs(scale.x), std::abs(scale.y))) * NtshEngn::Math::translate(offset);

	InternalUIImage uiImage;
	uiImage.textureIndex = textureIndex;

	const float x = (imageSize.x) / 2.0f;
	const float y = (imageSize.y) / 2.0f;
	const NtshEngn::Math::vec2 v0 = NtshEngn::Math::vec2(transform * NtshEngn::Math::vec3(x, -y, 1.0f));
	const NtshEngn::Math::vec2 v1 = NtshEngn::Math::vec2(transform * NtshEngn::Math::vec3(-x, -y, 1.0f));
	const NtshEngn::Math::vec2 v2 = NtshEngn::Math::vec2(transform * NtshEngn::Math::vec3(-x, y, 1.0f));
	const NtshEngn::Math::vec2 v3 = NtshEngn::Math::vec2(transform * NtshEngn::Math::vec3(x, y, 1.0f));

	uiImage.v0 = NtshEngn::Math::vec2((v0.x / m_viewport.width) * 2.0f - 1.0f, (v0.y / m_viewport.height) * 2.0f - 1.0f);
	uiImage.v1 = NtshEngn::Math::vec2((v1.x / m_viewport.width) * 2.0f - 1.0f, (v1.y / m_viewport.height) * 2.0f - 1.0f);
	uiImage.v2 = NtshEngn::Math::vec2((v2.x / m_viewport.width) * 2.0f - 1.0f, (v2.y / m_viewport.height) * 2.0f - 1.0f);
	uiImage.v3 = NtshEngn::Math::vec2((v3.x / m_viewport.width) * 2.0f - 1.0f, (v3.y / m_viewport.height) * 2.0f - 1.0f);
	uiImage.reverseUV.x = (scale.x >= 0.0f) ? 0.0f : 1.0f;
	uiImage.reverseUV.y = (scale.y >= 0.0f) ? 0.0f : 1.0f;
	uiImage.color = color;
	m_images.push(uiImage);

	m_elements.push(UIElement::Image);
}

void UI::createTextResources(VkFormat drawImageFormat) {
	// Create text buffers
	m_textBuffers.resize(m_framesInFlight);
	VkBufferCreateInfo uiTextBufferCreateInfo = {};
	uiTextBufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	uiTextBufferCreateInfo.pNext = nullptr;
	uiTextBufferCreateInfo.flags = 0;
	uiTextBufferCreateInfo.size = 65536;
	uiTextBufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	uiTextBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	uiTextBufferCreateInfo.queueFamilyIndexCount = 1;
	uiTextBufferCreateInfo.pQueueFamilyIndices = &m_graphicsQueueFamilyIndex;

	VmaAllocationInfo bufferAllocationInfo;

	VmaAllocationCreateInfo bufferAllocationCreateInfo = {};
	bufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
	bufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &uiTextBufferCreateInfo, &bufferAllocationCreateInfo, &m_textBuffers[i].handle, &m_textBuffers[i].allocation, &bufferAllocationInfo));
		m_textBuffers[i].address = bufferAllocationInfo.pMappedData;
	}

	// Create descriptor set layout
	VkDescriptorSetLayoutBinding textBufferDescriptorSetLayoutBinding = {};
	textBufferDescriptorSetLayoutBinding.binding = 0;
	textBufferDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	textBufferDescriptorSetLayoutBinding.descriptorCount = 1;
	textBufferDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	textBufferDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding fontsDescriptorSetLayoutBinding = {};
	fontsDescriptorSetLayoutBinding.binding = 1;
	fontsDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	fontsDescriptorSetLayoutBinding.descriptorCount = 131072;
	fontsDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	fontsDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	std::array<VkDescriptorBindingFlags, 2> descriptorBindingFlags = { 0, VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT };
	VkDescriptorSetLayoutBindingFlagsCreateInfo descriptorSetLayoutBindingFlagsCreateInfo = {};
	descriptorSetLayoutBindingFlagsCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	descriptorSetLayoutBindingFlagsCreateInfo.pNext = nullptr;
	descriptorSetLayoutBindingFlagsCreateInfo.bindingCount = static_cast<uint32_t>(descriptorBindingFlags.size());
	descriptorSetLayoutBindingFlagsCreateInfo.pBindingFlags = descriptorBindingFlags.data();

	std::array<VkDescriptorSetLayoutBinding, 2> descriptorSetLayoutBindings = { textBufferDescriptorSetLayoutBinding, fontsDescriptorSetLayoutBinding };
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.pNext = &descriptorSetLayoutBindingFlagsCreateInfo;
	descriptorSetLayoutCreateInfo.flags = 0;
	descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(descriptorSetLayoutBindings.size());
	descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings.data();
	NTSHENGN_VK_CHECK(vkCreateDescriptorSetLayout(m_device, &descriptorSetLayoutCreateInfo, nullptr, &m_textDescriptorSetLayout));

	// Create graphics pipeline
	VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {};
	pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	pipelineRenderingCreateInfo.pNext = nullptr;
	pipelineRenderingCreateInfo.viewMask = 0;
	pipelineRenderingCreateInfo.colorAttachmentCount = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &drawImageFormat;
	pipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
	pipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

	const std::string vertexShaderCode = R"GLSL(
		#version 460

		struct CharacterInfo {
			vec4 v0;
			vec4 v1;
			vec4 v2;
			vec4 v3;
		};

		layout(std430, set = 0, binding = 0) restrict readonly buffer Text {
			CharacterInfo char[];
		} text;

		layout(push_constant) uniform TextBufferInfo {
			uint bufferOffset;
		} tBI;

		layout(location = 0) out vec2 outUv;

		void main() {
			const uint charIndex = tBI.bufferOffset + uint(floor(gl_VertexIndex / 6));
			const uint vertexID = uint(mod(float(gl_VertexIndex), 6.0));
			if ((vertexID == 0) || (vertexID == 3)) {
				gl_Position = vec4(text.char[charIndex].v0.xy, 0.0, 1.0);
				outUv = text.char[charIndex].v0.zw;
			}
			else if (vertexID == 1) {
				gl_Position = vec4(text.char[charIndex].v1.xy, 0.0, 1.0);
				outUv = text.char[charIndex].v1.zw;
			}
			else if ((vertexID == 2) || (vertexID == 4)) {
				gl_Position = vec4(text.char[charIndex].v2.xy, 0.0, 1.0);
				outUv = text.char[charIndex].v2.zw;
			}
			else if (vertexID == 5) {
				gl_Position = vec4(text.char[charIndex].v3.xy, 0.0, 1.0);
				outUv = text.char[charIndex].v3.zw;
			}
		}
	)GLSL";
	const std::vector<uint32_t> vertexShaderSpv = compileShader(vertexShaderCode, ShaderType::Vertex);

	VkShaderModule vertexShaderModule;
	VkShaderModuleCreateInfo vertexShaderModuleCreateInfo = {};
	vertexShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertexShaderModuleCreateInfo.pNext = nullptr;
	vertexShaderModuleCreateInfo.flags = 0;
	vertexShaderModuleCreateInfo.codeSize = vertexShaderSpv.size() * sizeof(uint32_t);
	vertexShaderModuleCreateInfo.pCode = vertexShaderSpv.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &vertexShaderModuleCreateInfo, nullptr, &vertexShaderModule));

	VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo = {};
	vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderStageCreateInfo.pNext = nullptr;
	vertexShaderStageCreateInfo.flags = 0;
	vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexShaderStageCreateInfo.module = vertexShaderModule;
	vertexShaderStageCreateInfo.pName = "main";
	vertexShaderStageCreateInfo.pSpecializationInfo = nullptr;

	const std::string fragmentShaderCode = R"GLSL(
		#version 460
		#extension GL_EXT_nonuniform_qualifier : enable

		layout(set = 0, binding = 1) uniform sampler2D textures[];

		layout(push_constant) uniform TextInfo {
			layout(offset = 16) vec4 color;
			uint fontTextureIndex;
			uint fontType;
		} tI;

		layout(location = 0) in vec2 uv;

		layout(location = 0) out vec4 outColor;

		void main() {
			float alpha = texture(textures[nonuniformEXT(tI.fontTextureIndex)], uv).r;
			if (tI.fontType == 1) { // SDF
				const float smoothing = 1.0 / 64.0;
				alpha = smoothstep(0.5 - smoothing, 0.5 + smoothing, alpha);
			}

			outColor = vec4(1.0, 1.0, 1.0, alpha) * tI.color;
		}
	)GLSL";
	const std::vector<uint32_t> fragmentShaderSpv = compileShader(fragmentShaderCode, ShaderType::Fragment);

	VkShaderModule fragmentShaderModule;
	VkShaderModuleCreateInfo fragmentShaderModuleCreateInfo = {};
	fragmentShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	fragmentShaderModuleCreateInfo.pNext = nullptr;
	fragmentShaderModuleCreateInfo.flags = 0;
	fragmentShaderModuleCreateInfo.codeSize = fragmentShaderSpv.size() * sizeof(uint32_t);
	fragmentShaderModuleCreateInfo.pCode = fragmentShaderSpv.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &fragmentShaderModuleCreateInfo, nullptr, &fragmentShaderModule));

	VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo = {};
	fragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderStageCreateInfo.pNext = nullptr;
	fragmentShaderStageCreateInfo.flags = 0;
	fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentShaderStageCreateInfo.module = fragmentShaderModule;
	fragmentShaderStageCreateInfo.pName = "main";
	fragmentShaderStageCreateInfo.pSpecializationInfo = nullptr;

	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStageCreateInfos = { vertexShaderStageCreateInfo, fragmentShaderStageCreateInfo };

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
	vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateCreateInfo.pNext = nullptr;
	vertexInputStateCreateInfo.flags = 0;
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexBindingDescriptions = nullptr;
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = nullptr;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {};
	inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyStateCreateInfo.pNext = nullptr;
	inputAssemblyStateCreateInfo.flags = 0;
	inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.pNext = nullptr;
	viewportStateCreateInfo.flags = 0;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &m_viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &m_scissor;

	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {};
	rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateCreateInfo.pNext = nullptr;
	rasterizationStateCreateInfo.flags = 0;
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_NONE;
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
	rasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
	rasterizationStateCreateInfo.depthBiasClamp = 0.0f;
	rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;
	rasterizationStateCreateInfo.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {};
	multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleStateCreateInfo.pNext = nullptr;
	multisampleStateCreateInfo.flags = 0;
	multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
	multisampleStateCreateInfo.minSampleShading = 0.0f;
	multisampleStateCreateInfo.pSampleMask = nullptr;
	multisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
	multisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;

	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {};
	depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilStateCreateInfo.pNext = nullptr;
	depthStencilStateCreateInfo.flags = 0;
	depthStencilStateCreateInfo.depthTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.depthWriteEnable = VK_FALSE;
	depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_NEVER;
	depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.front = {};
	depthStencilStateCreateInfo.back = {};
	depthStencilStateCreateInfo.minDepthBounds = 0.0f;
	depthStencilStateCreateInfo.maxDepthBounds = 1.0f;

	VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
	colorBlendAttachmentState.blendEnable = VK_TRUE;
	colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {};
	colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendStateCreateInfo.pNext = nullptr;
	colorBlendStateCreateInfo.flags = 0;
	colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
	colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	colorBlendStateCreateInfo.attachmentCount = 1;
	colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;

	std::array<VkDynamicState, 2> dynamicStates = { VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_VIEWPORT };
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.pNext = nullptr;
	dynamicStateCreateInfo.flags = 0;
	dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

	VkPushConstantRange vertexPushConstantRange = {};
	vertexPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	vertexPushConstantRange.offset = 0;
	vertexPushConstantRange.size = sizeof(uint32_t);

	VkPushConstantRange fragmentPushConstantRange = {};
	fragmentPushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentPushConstantRange.offset = sizeof(NtshEngn::Math::vec4);
	fragmentPushConstantRange.size = sizeof(NtshEngn::Math::vec4) + (sizeof(uint32_t) * 2);

	std::array<VkPushConstantRange, 2> pushConstantRanges = { vertexPushConstantRange, fragmentPushConstantRange };
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.pNext = nullptr;
	pipelineLayoutCreateInfo.flags = 0;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &m_textDescriptorSetLayout;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 2;
	pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges.data();
	NTSHENGN_VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_textGraphicsPipelineLayout));

	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
	graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPipelineCreateInfo.pNext = &pipelineRenderingCreateInfo;
	graphicsPipelineCreateInfo.flags = 0;
	graphicsPipelineCreateInfo.stageCount = 2;
	graphicsPipelineCreateInfo.pStages = shaderStageCreateInfos.data();
	graphicsPipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
	graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
	graphicsPipelineCreateInfo.pTessellationState = nullptr;
	graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	graphicsPipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
	graphicsPipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
	graphicsPipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	graphicsPipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	graphicsPipelineCreateInfo.layout = m_textGraphicsPipelineLayout;
	graphicsPipelineCreateInfo.renderPass = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.subpass = 0;
	graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.basePipelineIndex = 0;
	NTSHENGN_VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &m_textGraphicsPipeline));

	vkDestroyShaderModule(m_device, vertexShaderModule, nullptr);
	vkDestroyShaderModule(m_device, fragmentShaderModule, nullptr);

	// Create descriptor pool
	VkDescriptorPoolSize textBufferDescriptorPoolSize = {};
	textBufferDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	textBufferDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize fontsDescriptorPoolSize = {};
	fontsDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	fontsDescriptorPoolSize.descriptorCount = 131072 * m_framesInFlight;

	std::array<VkDescriptorPoolSize, 2> descriptorPoolSizes = { textBufferDescriptorPoolSize, fontsDescriptorPoolSize };
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext = nullptr;
	descriptorPoolCreateInfo.flags = 0;
	descriptorPoolCreateInfo.maxSets = m_framesInFlight;
	descriptorPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size());
	descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes.data();
	NTSHENGN_VK_CHECK(vkCreateDescriptorPool(m_device, &descriptorPoolCreateInfo, nullptr, &m_textDescriptorPool));

	// Allocate descriptor sets
	m_textDescriptorSets.resize(m_framesInFlight);
	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
	descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetAllocateInfo.pNext = nullptr;
	descriptorSetAllocateInfo.descriptorPool = m_textDescriptorPool;
	descriptorSetAllocateInfo.descriptorSetCount = 1;
	descriptorSetAllocateInfo.pSetLayouts = &m_textDescriptorSetLayout;
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		NTSHENGN_VK_CHECK(vkAllocateDescriptorSets(m_device, &descriptorSetAllocateInfo, &m_textDescriptorSets[i]));
	}

	// Update descriptor sets
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		VkDescriptorBufferInfo textDescriptorBufferInfo;
		textDescriptorBufferInfo.buffer = m_textBuffers[i].handle;
		textDescriptorBufferInfo.offset = 0;
		textDescriptorBufferInfo.range = 65536;

		VkWriteDescriptorSet textDescriptorWriteDescriptorSet = {};
		textDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		textDescriptorWriteDescriptorSet.pNext = nullptr;
		textDescriptorWriteDescriptorSet.dstSet = m_textDescriptorSets[i];
		textDescriptorWriteDescriptorSet.dstBinding = 0;
		textDescriptorWriteDescriptorSet.dstArrayElement = 0;
		textDescriptorWriteDescriptorSet.descriptorCount = 1;
		textDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		textDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		textDescriptorWriteDescriptorSet.pBufferInfo = &textDescriptorBufferInfo;
		textDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;

		vkUpdateDescriptorSets(m_device, 1, &textDescriptorWriteDescriptorSet, 0, nullptr);
	}

	m_textDescriptorSetsNeedUpdate.resize(m_framesInFlight);
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		m_textDescriptorSetsNeedUpdate[i] = false;
	}
}

void UI::createLineResources(VkFormat drawImageFormat) {
	// Create graphics pipeline
	VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {};
	pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	pipelineRenderingCreateInfo.pNext = nullptr;
	pipelineRenderingCreateInfo.viewMask = 0;
	pipelineRenderingCreateInfo.colorAttachmentCount = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &drawImageFormat;
	pipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
	pipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

	const std::string vertexShaderCode = R"GLSL(
		#version 460

		layout(push_constant) uniform LinePositionInfo {
			vec2 start;
			vec2 end;
		} lPI;

		void main() {
			if (gl_VertexIndex == 0) {
				gl_Position = vec4(lPI.start, 0.0, 1.0);
			}
			else {
				gl_Position = vec4(lPI.end, 0.0, 1.0);
			}
		}
	)GLSL";
	const std::vector<uint32_t> vertexShaderSpv = compileShader(vertexShaderCode, ShaderType::Vertex);

	VkShaderModule vertexShaderModule;
	VkShaderModuleCreateInfo vertexShaderModuleCreateInfo = {};
	vertexShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertexShaderModuleCreateInfo.pNext = nullptr;
	vertexShaderModuleCreateInfo.flags = 0;
	vertexShaderModuleCreateInfo.codeSize = vertexShaderSpv.size() * sizeof(uint32_t);
	vertexShaderModuleCreateInfo.pCode = vertexShaderSpv.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &vertexShaderModuleCreateInfo, nullptr, &vertexShaderModule));

	VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo = {};
	vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderStageCreateInfo.pNext = nullptr;
	vertexShaderStageCreateInfo.flags = 0;
	vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexShaderStageCreateInfo.module = vertexShaderModule;
	vertexShaderStageCreateInfo.pName = "main";
	vertexShaderStageCreateInfo.pSpecializationInfo = nullptr;

	const std::string fragmentShaderCode = R"GLSL(
		#version 460

		layout(push_constant) uniform LineColorInfo {
			layout(offset = 16) vec4 color;
		} lCI;

		layout(location = 0) out vec4 outColor;

		void main() {
			outColor = lCI.color;
		}
	)GLSL";
	const std::vector<uint32_t> fragmentShaderSpv = compileShader(fragmentShaderCode, ShaderType::Fragment);

	VkShaderModule fragmentShaderModule;
	VkShaderModuleCreateInfo fragmentShaderModuleCreateInfo = {};
	fragmentShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	fragmentShaderModuleCreateInfo.pNext = nullptr;
	fragmentShaderModuleCreateInfo.flags = 0;
	fragmentShaderModuleCreateInfo.codeSize = fragmentShaderSpv.size() * sizeof(uint32_t);
	fragmentShaderModuleCreateInfo.pCode = fragmentShaderSpv.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &fragmentShaderModuleCreateInfo, nullptr, &fragmentShaderModule));

	VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo = {};
	fragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderStageCreateInfo.pNext = nullptr;
	fragmentShaderStageCreateInfo.flags = 0;
	fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentShaderStageCreateInfo.module = fragmentShaderModule;
	fragmentShaderStageCreateInfo.pName = "main";
	fragmentShaderStageCreateInfo.pSpecializationInfo = nullptr;

	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStageCreateInfos = { vertexShaderStageCreateInfo, fragmentShaderStageCreateInfo };

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
	vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateCreateInfo.pNext = nullptr;
	vertexInputStateCreateInfo.flags = 0;
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexBindingDescriptions = nullptr;
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = nullptr;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {};
	inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyStateCreateInfo.pNext = nullptr;
	inputAssemblyStateCreateInfo.flags = 0;
	inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
	inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.pNext = nullptr;
	viewportStateCreateInfo.flags = 0;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &m_viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &m_scissor;

	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {};
	rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateCreateInfo.pNext = nullptr;
	rasterizationStateCreateInfo.flags = 0;
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
	rasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
	rasterizationStateCreateInfo.depthBiasClamp = 0.0f;
	rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;
	rasterizationStateCreateInfo.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {};
	multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleStateCreateInfo.pNext = nullptr;
	multisampleStateCreateInfo.flags = 0;
	multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
	multisampleStateCreateInfo.minSampleShading = 0.0f;
	multisampleStateCreateInfo.pSampleMask = nullptr;
	multisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
	multisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;

	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {};
	depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilStateCreateInfo.pNext = nullptr;
	depthStencilStateCreateInfo.flags = 0;
	depthStencilStateCreateInfo.depthTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.depthWriteEnable = VK_FALSE;
	depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_NEVER;
	depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.front = {};
	depthStencilStateCreateInfo.back = {};
	depthStencilStateCreateInfo.minDepthBounds = 0.0f;
	depthStencilStateCreateInfo.maxDepthBounds = 1.0f;

	VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
	colorBlendAttachmentState.blendEnable = VK_TRUE;
	colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {};
	colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendStateCreateInfo.pNext = nullptr;
	colorBlendStateCreateInfo.flags = 0;
	colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
	colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	colorBlendStateCreateInfo.attachmentCount = 1;
	colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;

	std::array<VkDynamicState, 2> dynamicStates = { VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_VIEWPORT };
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.pNext = nullptr;
	dynamicStateCreateInfo.flags = 0;
	dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

	VkPushConstantRange vertexPushConstantRange = {};
	vertexPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	vertexPushConstantRange.offset = 0;
	vertexPushConstantRange.size = sizeof(NtshEngn::Math::vec2) + sizeof(NtshEngn::Math::vec2);

	VkPushConstantRange fragmentPushConstantRange = {};
	fragmentPushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentPushConstantRange.offset = sizeof(NtshEngn::Math::vec4);
	fragmentPushConstantRange.size = sizeof(NtshEngn::Math::vec4);

	std::array<VkPushConstantRange, 2> pushConstantRanges = { vertexPushConstantRange, fragmentPushConstantRange };
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.pNext = nullptr;
	pipelineLayoutCreateInfo.flags = 0;
	pipelineLayoutCreateInfo.setLayoutCount = 0;
	pipelineLayoutCreateInfo.pSetLayouts = nullptr;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 2;
	pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges.data();
	NTSHENGN_VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_lineGraphicsPipelineLayout));

	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
	graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPipelineCreateInfo.pNext = &pipelineRenderingCreateInfo;
	graphicsPipelineCreateInfo.flags = 0;
	graphicsPipelineCreateInfo.stageCount = 2;
	graphicsPipelineCreateInfo.pStages = shaderStageCreateInfos.data();
	graphicsPipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
	graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
	graphicsPipelineCreateInfo.pTessellationState = nullptr;
	graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	graphicsPipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
	graphicsPipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
	graphicsPipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	graphicsPipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	graphicsPipelineCreateInfo.layout = m_lineGraphicsPipelineLayout;
	graphicsPipelineCreateInfo.renderPass = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.subpass = 0;
	graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.basePipelineIndex = 0;
	NTSHENGN_VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &m_lineGraphicsPipeline));

	vkDestroyShaderModule(m_device, vertexShaderModule, nullptr);
	vkDestroyShaderModule(m_device, fragmentShaderModule, nullptr);
}

void UI::createRectangleResources(VkFormat drawImageFormat) {
	// Create graphics pipeline
	VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {};
	pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	pipelineRenderingCreateInfo.pNext = nullptr;
	pipelineRenderingCreateInfo.viewMask = 0;
	pipelineRenderingCreateInfo.colorAttachmentCount = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &drawImageFormat;
	pipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
	pipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

	const std::string vertexShaderCode = R"GLSL(
		#version 460

		layout(push_constant) uniform RectanglePositionInfo {
			vec2 topLeft;
			vec2 bottomRight;
		} rPI;

		void main() {
			if ((gl_VertexIndex == 0) || (gl_VertexIndex == 3)) {
				gl_Position = vec4(rPI.bottomRight.x, rPI.topLeft.y, 0.0, 1.0);
			}
			else if (gl_VertexIndex == 1) {
				gl_Position = vec4(rPI.topLeft, 0.0, 1.0);
			}
			else if ((gl_VertexIndex == 2) || (gl_VertexIndex == 4)) {
				gl_Position = vec4(rPI.topLeft.x, rPI.bottomRight.y, 0.0, 1.0);
			}
			else if (gl_VertexIndex == 5) {
				gl_Position = vec4(rPI.bottomRight, 0.0, 1.0);
			}
		}
	)GLSL";
	const std::vector<uint32_t> vertexShaderSpv = compileShader(vertexShaderCode, ShaderType::Vertex);

	VkShaderModule vertexShaderModule;
	VkShaderModuleCreateInfo vertexShaderModuleCreateInfo = {};
	vertexShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertexShaderModuleCreateInfo.pNext = nullptr;
	vertexShaderModuleCreateInfo.flags = 0;
	vertexShaderModuleCreateInfo.codeSize = vertexShaderSpv.size() * sizeof(uint32_t);
	vertexShaderModuleCreateInfo.pCode = vertexShaderSpv.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &vertexShaderModuleCreateInfo, nullptr, &vertexShaderModule));

	VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo = {};
	vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderStageCreateInfo.pNext = nullptr;
	vertexShaderStageCreateInfo.flags = 0;
	vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexShaderStageCreateInfo.module = vertexShaderModule;
	vertexShaderStageCreateInfo.pName = "main";
	vertexShaderStageCreateInfo.pSpecializationInfo = nullptr;

	const std::string fragmentShaderCode = R"GLSL(
		#version 460

		layout(push_constant) uniform RectangleColorInfo {
			layout(offset = 16) vec4 color;
		} rCI;

		layout(location = 0) out vec4 outColor;

		void main() {
			outColor = rCI.color;
		}
	)GLSL";
	const std::vector<uint32_t> fragmentShaderSpv = compileShader(fragmentShaderCode, ShaderType::Fragment);

	VkShaderModule fragmentShaderModule;
	VkShaderModuleCreateInfo fragmentShaderModuleCreateInfo = {};
	fragmentShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	fragmentShaderModuleCreateInfo.pNext = nullptr;
	fragmentShaderModuleCreateInfo.flags = 0;
	fragmentShaderModuleCreateInfo.codeSize = fragmentShaderSpv.size() * sizeof(uint32_t);
	fragmentShaderModuleCreateInfo.pCode = fragmentShaderSpv.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &fragmentShaderModuleCreateInfo, nullptr, &fragmentShaderModule));

	VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo = {};
	fragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderStageCreateInfo.pNext = nullptr;
	fragmentShaderStageCreateInfo.flags = 0;
	fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentShaderStageCreateInfo.module = fragmentShaderModule;
	fragmentShaderStageCreateInfo.pName = "main";
	fragmentShaderStageCreateInfo.pSpecializationInfo = nullptr;

	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStageCreateInfos = { vertexShaderStageCreateInfo, fragmentShaderStageCreateInfo };

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
	vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateCreateInfo.pNext = nullptr;
	vertexInputStateCreateInfo.flags = 0;
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexBindingDescriptions = nullptr;
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = nullptr;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {};
	inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyStateCreateInfo.pNext = nullptr;
	inputAssemblyStateCreateInfo.flags = 0;
	inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.pNext = nullptr;
	viewportStateCreateInfo.flags = 0;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &m_viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &m_scissor;

	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {};
	rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateCreateInfo.pNext = nullptr;
	rasterizationStateCreateInfo.flags = 0;
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
	rasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
	rasterizationStateCreateInfo.depthBiasClamp = 0.0f;
	rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;
	rasterizationStateCreateInfo.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {};
	multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleStateCreateInfo.pNext = nullptr;
	multisampleStateCreateInfo.flags = 0;
	multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
	multisampleStateCreateInfo.minSampleShading = 0.0f;
	multisampleStateCreateInfo.pSampleMask = nullptr;
	multisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
	multisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;

	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {};
	depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilStateCreateInfo.pNext = nullptr;
	depthStencilStateCreateInfo.flags = 0;
	depthStencilStateCreateInfo.depthTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.depthWriteEnable = VK_FALSE;
	depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_NEVER;
	depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.front = {};
	depthStencilStateCreateInfo.back = {};
	depthStencilStateCreateInfo.minDepthBounds = 0.0f;
	depthStencilStateCreateInfo.maxDepthBounds = 1.0f;

	VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
	colorBlendAttachmentState.blendEnable = VK_TRUE;
	colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {};
	colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendStateCreateInfo.pNext = nullptr;
	colorBlendStateCreateInfo.flags = 0;
	colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
	colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	colorBlendStateCreateInfo.attachmentCount = 1;
	colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;

	std::array<VkDynamicState, 2> dynamicStates = { VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_VIEWPORT };
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.pNext = nullptr;
	dynamicStateCreateInfo.flags = 0;
	dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

	VkPushConstantRange vertexPushConstantRange = {};
	vertexPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	vertexPushConstantRange.offset = 0;
	vertexPushConstantRange.size = sizeof(NtshEngn::Math::vec2) + sizeof(NtshEngn::Math::vec2);

	VkPushConstantRange fragmentPushConstantRange = {};
	fragmentPushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentPushConstantRange.offset = sizeof(NtshEngn::Math::vec4);
	fragmentPushConstantRange.size = sizeof(NtshEngn::Math::vec4);

	std::array<VkPushConstantRange, 2> pushConstantRanges = { vertexPushConstantRange, fragmentPushConstantRange };
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.pNext = nullptr;
	pipelineLayoutCreateInfo.flags = 0;
	pipelineLayoutCreateInfo.setLayoutCount = 0;
	pipelineLayoutCreateInfo.pSetLayouts = nullptr;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 2;
	pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges.data();
	NTSHENGN_VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_rectangleGraphicsPipelineLayout));

	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
	graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPipelineCreateInfo.pNext = &pipelineRenderingCreateInfo;
	graphicsPipelineCreateInfo.flags = 0;
	graphicsPipelineCreateInfo.stageCount = 2;
	graphicsPipelineCreateInfo.pStages = shaderStageCreateInfos.data();
	graphicsPipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
	graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
	graphicsPipelineCreateInfo.pTessellationState = nullptr;
	graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	graphicsPipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
	graphicsPipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
	graphicsPipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	graphicsPipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	graphicsPipelineCreateInfo.layout = m_rectangleGraphicsPipelineLayout;
	graphicsPipelineCreateInfo.renderPass = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.subpass = 0;
	graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.basePipelineIndex = 0;
	NTSHENGN_VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &m_rectangleGraphicsPipeline));

	vkDestroyShaderModule(m_device, vertexShaderModule, nullptr);
	vkDestroyShaderModule(m_device, fragmentShaderModule, nullptr);
}

void UI::createImageResources(VkFormat drawImageFormat) {
	// Create descriptor set layout
	VkDescriptorSetLayoutBinding uiTexturesDescriptorSetLayoutBinding = {};
	uiTexturesDescriptorSetLayoutBinding.binding = 0;
	uiTexturesDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	uiTexturesDescriptorSetLayoutBinding.descriptorCount = 131072;
	uiTexturesDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	uiTexturesDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorBindingFlags descriptorBindingFlag = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
	VkDescriptorSetLayoutBindingFlagsCreateInfo descriptorSetLayoutBindingFlagsCreateInfo = {};
	descriptorSetLayoutBindingFlagsCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
	descriptorSetLayoutBindingFlagsCreateInfo.pNext = nullptr;
	descriptorSetLayoutBindingFlagsCreateInfo.bindingCount = 1;
	descriptorSetLayoutBindingFlagsCreateInfo.pBindingFlags = &descriptorBindingFlag;

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.pNext = &descriptorSetLayoutBindingFlagsCreateInfo;
	descriptorSetLayoutCreateInfo.flags = 0;
	descriptorSetLayoutCreateInfo.bindingCount = 1;
	descriptorSetLayoutCreateInfo.pBindings = &uiTexturesDescriptorSetLayoutBinding;
	NTSHENGN_VK_CHECK(vkCreateDescriptorSetLayout(m_device, &descriptorSetLayoutCreateInfo, nullptr, &m_imageDescriptorSetLayout));

	// Create graphics pipeline
	VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {};
	pipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
	pipelineRenderingCreateInfo.pNext = nullptr;
	pipelineRenderingCreateInfo.viewMask = 0;
	pipelineRenderingCreateInfo.colorAttachmentCount = 1;
	pipelineRenderingCreateInfo.pColorAttachmentFormats = &drawImageFormat;
	pipelineRenderingCreateInfo.depthAttachmentFormat = VK_FORMAT_UNDEFINED;
	pipelineRenderingCreateInfo.stencilAttachmentFormat = VK_FORMAT_UNDEFINED;

	const std::string vertexShaderCode = R"GLSL(
		#version 460

		layout(push_constant) uniform UITexturePositionInfo {
			vec2 v0;
			vec2 v1;
			vec2 v2;
			vec2 v3;
			vec2 reverseUV;
		} uTPI;

		layout(location = 0) out vec2 outUv;

		void main() {
			if ((gl_VertexIndex == 0) || (gl_VertexIndex == 3)) {
				gl_Position = vec4(uTPI.v0, 0.0, 1.0);
				outUv = vec2(1.0 - uTPI.reverseUV.x, 0.0 + uTPI.reverseUV.y);
			}
			else if (gl_VertexIndex == 1) {
				gl_Position = vec4(uTPI.v1, 0.0, 1.0);
				outUv = vec2(0.0 + uTPI.reverseUV.x, 0.0 + uTPI.reverseUV.y);
			}
			else if ((gl_VertexIndex == 2) || (gl_VertexIndex == 4)) {
				gl_Position = vec4(uTPI.v2, 0.0, 1.0);
				outUv = vec2(0.0 + uTPI.reverseUV.x, 1.0 - uTPI.reverseUV.y);
			}
			else if (gl_VertexIndex == 5) {
				gl_Position = vec4(uTPI.v3, 0.0, 1.0);
				outUv = vec2(1.0 - uTPI.reverseUV.x, 1.0 - uTPI.reverseUV.y);
			}
		}
	)GLSL";
	const std::vector<uint32_t> vertexShaderSpv = compileShader(vertexShaderCode, ShaderType::Vertex);

	VkShaderModule vertexShaderModule;
	VkShaderModuleCreateInfo vertexShaderModuleCreateInfo = {};
	vertexShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	vertexShaderModuleCreateInfo.pNext = nullptr;
	vertexShaderModuleCreateInfo.flags = 0;
	vertexShaderModuleCreateInfo.codeSize = vertexShaderSpv.size() * sizeof(uint32_t);
	vertexShaderModuleCreateInfo.pCode = vertexShaderSpv.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &vertexShaderModuleCreateInfo, nullptr, &vertexShaderModule));

	VkPipelineShaderStageCreateInfo vertexShaderStageCreateInfo = {};
	vertexShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vertexShaderStageCreateInfo.pNext = nullptr;
	vertexShaderStageCreateInfo.flags = 0;
	vertexShaderStageCreateInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vertexShaderStageCreateInfo.module = vertexShaderModule;
	vertexShaderStageCreateInfo.pName = "main";
	vertexShaderStageCreateInfo.pSpecializationInfo = nullptr;

	const std::string fragmentShaderCode = R"GLSL(
		#version 460
		#extension GL_EXT_nonuniform_qualifier : enable

		layout(push_constant) uniform UITextureInfo {
			layout(offset = 48) vec4 color;
			uint textureIndex;
		} uTI;

		layout(set = 0, binding = 0) uniform sampler2D textures[];

		layout(location = 0) in vec2 uv;

		layout(location = 0) out vec4 outColor;

		void main() {
			outColor = texture(textures[nonuniformEXT(uTI.textureIndex)], uv) * uTI.color;
		}
	)GLSL";
	const std::vector<uint32_t> fragmentShaderSpv = compileShader(fragmentShaderCode, ShaderType::Fragment);

	VkShaderModule fragmentShaderModule;
	VkShaderModuleCreateInfo fragmentShaderModuleCreateInfo = {};
	fragmentShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	fragmentShaderModuleCreateInfo.pNext = nullptr;
	fragmentShaderModuleCreateInfo.flags = 0;
	fragmentShaderModuleCreateInfo.codeSize = fragmentShaderSpv.size() * sizeof(uint32_t);
	fragmentShaderModuleCreateInfo.pCode = fragmentShaderSpv.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &fragmentShaderModuleCreateInfo, nullptr, &fragmentShaderModule));

	VkPipelineShaderStageCreateInfo fragmentShaderStageCreateInfo = {};
	fragmentShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	fragmentShaderStageCreateInfo.pNext = nullptr;
	fragmentShaderStageCreateInfo.flags = 0;
	fragmentShaderStageCreateInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentShaderStageCreateInfo.module = fragmentShaderModule;
	fragmentShaderStageCreateInfo.pName = "main";
	fragmentShaderStageCreateInfo.pSpecializationInfo = nullptr;

	std::array<VkPipelineShaderStageCreateInfo, 2> shaderStageCreateInfos = { vertexShaderStageCreateInfo, fragmentShaderStageCreateInfo };

	VkPipelineVertexInputStateCreateInfo vertexInputStateCreateInfo = {};
	vertexInputStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertexInputStateCreateInfo.pNext = nullptr;
	vertexInputStateCreateInfo.flags = 0;
	vertexInputStateCreateInfo.vertexBindingDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexBindingDescriptions = nullptr;
	vertexInputStateCreateInfo.vertexAttributeDescriptionCount = 0;
	vertexInputStateCreateInfo.pVertexAttributeDescriptions = nullptr;

	VkPipelineInputAssemblyStateCreateInfo inputAssemblyStateCreateInfo = {};
	inputAssemblyStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyStateCreateInfo.pNext = nullptr;
	inputAssemblyStateCreateInfo.flags = 0;
	inputAssemblyStateCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	inputAssemblyStateCreateInfo.primitiveRestartEnable = VK_FALSE;

	VkPipelineViewportStateCreateInfo viewportStateCreateInfo = {};
	viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportStateCreateInfo.pNext = nullptr;
	viewportStateCreateInfo.flags = 0;
	viewportStateCreateInfo.viewportCount = 1;
	viewportStateCreateInfo.pViewports = &m_viewport;
	viewportStateCreateInfo.scissorCount = 1;
	viewportStateCreateInfo.pScissors = &m_scissor;

	VkPipelineRasterizationStateCreateInfo rasterizationStateCreateInfo = {};
	rasterizationStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationStateCreateInfo.pNext = nullptr;
	rasterizationStateCreateInfo.flags = 0;
	rasterizationStateCreateInfo.depthClampEnable = VK_FALSE;
	rasterizationStateCreateInfo.rasterizerDiscardEnable = VK_FALSE;
	rasterizationStateCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationStateCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizationStateCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationStateCreateInfo.depthBiasEnable = VK_FALSE;
	rasterizationStateCreateInfo.depthBiasConstantFactor = 0.0f;
	rasterizationStateCreateInfo.depthBiasClamp = 0.0f;
	rasterizationStateCreateInfo.depthBiasSlopeFactor = 0.0f;
	rasterizationStateCreateInfo.lineWidth = 1.0f;

	VkPipelineMultisampleStateCreateInfo multisampleStateCreateInfo = {};
	multisampleStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleStateCreateInfo.pNext = nullptr;
	multisampleStateCreateInfo.flags = 0;
	multisampleStateCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampleStateCreateInfo.sampleShadingEnable = VK_FALSE;
	multisampleStateCreateInfo.minSampleShading = 0.0f;
	multisampleStateCreateInfo.pSampleMask = nullptr;
	multisampleStateCreateInfo.alphaToCoverageEnable = VK_FALSE;
	multisampleStateCreateInfo.alphaToOneEnable = VK_FALSE;

	VkPipelineDepthStencilStateCreateInfo depthStencilStateCreateInfo = {};
	depthStencilStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilStateCreateInfo.pNext = nullptr;
	depthStencilStateCreateInfo.flags = 0;
	depthStencilStateCreateInfo.depthTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.depthWriteEnable = VK_FALSE;
	depthStencilStateCreateInfo.depthCompareOp = VK_COMPARE_OP_NEVER;
	depthStencilStateCreateInfo.depthBoundsTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.stencilTestEnable = VK_FALSE;
	depthStencilStateCreateInfo.front = {};
	depthStencilStateCreateInfo.back = {};
	depthStencilStateCreateInfo.minDepthBounds = 0.0f;
	depthStencilStateCreateInfo.maxDepthBounds = 1.0f;

	VkPipelineColorBlendAttachmentState colorBlendAttachmentState = {};
	colorBlendAttachmentState.blendEnable = VK_TRUE;
	colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;
	colorBlendAttachmentState.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	VkPipelineColorBlendStateCreateInfo colorBlendStateCreateInfo = {};
	colorBlendStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendStateCreateInfo.pNext = nullptr;
	colorBlendStateCreateInfo.flags = 0;
	colorBlendStateCreateInfo.logicOpEnable = VK_FALSE;
	colorBlendStateCreateInfo.logicOp = VK_LOGIC_OP_COPY;
	colorBlendStateCreateInfo.attachmentCount = 1;
	colorBlendStateCreateInfo.pAttachments = &colorBlendAttachmentState;

	std::array<VkDynamicState, 2> dynamicStates = { VK_DYNAMIC_STATE_SCISSOR, VK_DYNAMIC_STATE_VIEWPORT };
	VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo = {};
	dynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicStateCreateInfo.pNext = nullptr;
	dynamicStateCreateInfo.flags = 0;
	dynamicStateCreateInfo.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
	dynamicStateCreateInfo.pDynamicStates = dynamicStates.data();

	VkPushConstantRange vertexPushConstantRange = {};
	vertexPushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	vertexPushConstantRange.offset = 0;
	vertexPushConstantRange.size = 5 * sizeof(NtshEngn::Math::vec2);

	VkPushConstantRange fragmentPushConstantRange = {};
	fragmentPushConstantRange.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	fragmentPushConstantRange.offset = 5 * sizeof(NtshEngn::Math::vec2) + sizeof(NtshEngn::Math::vec2);
	fragmentPushConstantRange.size = sizeof(NtshEngn::Math::vec4) + sizeof(uint32_t);

	std::array<VkPushConstantRange, 2> pushConstantRanges = { vertexPushConstantRange, fragmentPushConstantRange };
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.pNext = nullptr;
	pipelineLayoutCreateInfo.flags = 0;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &m_imageDescriptorSetLayout;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 2;
	pipelineLayoutCreateInfo.pPushConstantRanges = pushConstantRanges.data();
	NTSHENGN_VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_imageGraphicsPipelineLayout));

	VkGraphicsPipelineCreateInfo graphicsPipelineCreateInfo = {};
	graphicsPipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	graphicsPipelineCreateInfo.pNext = &pipelineRenderingCreateInfo;
	graphicsPipelineCreateInfo.flags = 0;
	graphicsPipelineCreateInfo.stageCount = 2;
	graphicsPipelineCreateInfo.pStages = shaderStageCreateInfos.data();
	graphicsPipelineCreateInfo.pVertexInputState = &vertexInputStateCreateInfo;
	graphicsPipelineCreateInfo.pInputAssemblyState = &inputAssemblyStateCreateInfo;
	graphicsPipelineCreateInfo.pTessellationState = nullptr;
	graphicsPipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
	graphicsPipelineCreateInfo.pRasterizationState = &rasterizationStateCreateInfo;
	graphicsPipelineCreateInfo.pMultisampleState = &multisampleStateCreateInfo;
	graphicsPipelineCreateInfo.pDepthStencilState = &depthStencilStateCreateInfo;
	graphicsPipelineCreateInfo.pColorBlendState = &colorBlendStateCreateInfo;
	graphicsPipelineCreateInfo.pDynamicState = &dynamicStateCreateInfo;
	graphicsPipelineCreateInfo.layout = m_imageGraphicsPipelineLayout;
	graphicsPipelineCreateInfo.renderPass = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.subpass = 0;
	graphicsPipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	graphicsPipelineCreateInfo.basePipelineIndex = 0;
	NTSHENGN_VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &graphicsPipelineCreateInfo, nullptr, &m_imageGraphicsPipeline));

	vkDestroyShaderModule(m_device, vertexShaderModule, nullptr);
	vkDestroyShaderModule(m_device, fragmentShaderModule, nullptr);

	// Create descriptor pool
	VkDescriptorPoolSize uiTexturesDescriptorPoolSize = {};
	uiTexturesDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	uiTexturesDescriptorPoolSize.descriptorCount = 131072 * m_framesInFlight;

	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext = nullptr;
	descriptorPoolCreateInfo.flags = 0;
	descriptorPoolCreateInfo.maxSets = m_framesInFlight;
	descriptorPoolCreateInfo.poolSizeCount = 1;
	descriptorPoolCreateInfo.pPoolSizes = &uiTexturesDescriptorPoolSize;
	NTSHENGN_VK_CHECK(vkCreateDescriptorPool(m_device, &descriptorPoolCreateInfo, nullptr, &m_imageDescriptorPool));

	// Allocate descriptor sets
	m_imageDescriptorSets.resize(m_framesInFlight);
	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
	descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetAllocateInfo.pNext = nullptr;
	descriptorSetAllocateInfo.descriptorPool = m_imageDescriptorPool;
	descriptorSetAllocateInfo.descriptorSetCount = 1;
	descriptorSetAllocateInfo.pSetLayouts = &m_imageDescriptorSetLayout;
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		NTSHENGN_VK_CHECK(vkAllocateDescriptorSets(m_device, &descriptorSetAllocateInfo, &m_imageDescriptorSets[i]));
	}

	m_imageDescriptorSetsNeedUpdate.resize(m_framesInFlight);
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		m_imageDescriptorSetsNeedUpdate[i] = false;
	}
}
