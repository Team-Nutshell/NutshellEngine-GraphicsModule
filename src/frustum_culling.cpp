#include "frustum_culling.h"
#include <array>
#include <mutex>

void FrustumCulling::init(VkDevice device,
	VkQueue computeQueue,
	uint32_t computeQueueFamilyIndex,
	VmaAllocator allocator,
	uint32_t framesInFlight,
	PFN_vkCmdPipelineBarrier2KHR vkCmdPipelineBarrier2KHR,
	NtshEngn::ECSInterface* ecs) {
	m_device = device;
	m_computeQueue = computeQueue;
	m_computeQueueFamilyIndex = computeQueueFamilyIndex;
	m_allocator = allocator;
	m_framesInFlight = framesInFlight;
	m_vkCmdPipelineBarrier2KHR = vkCmdPipelineBarrier2KHR;
	m_ecs = ecs;

	createBuffers();
	createDescriptorSetLayout();
	createComputePipeline();
	createDescriptorSets();
}

void FrustumCulling::destroy() {
	vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);

	vkDestroyPipeline(m_device, m_computePipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_computePipelineLayout, nullptr);

	vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		m_frustumCullingObjectBuffers[i].destroy(m_allocator);
	}

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		m_frustumCullingInfoBuffers[i].destroy(m_allocator);
	}

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		m_inPerDrawBuffers[i].destroy(m_allocator);
	}

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		m_inDrawIndirectBuffers[i].destroy(m_allocator);
	}
}

uint32_t FrustumCulling::cull(VkCommandBuffer commandBuffer,
	uint32_t currentFrameInFlight,
	const std::vector<FrustumCullingInfo>& frustumCullingInfos,
	const std::unordered_map<NtshEngn::Entity, InternalObject>& objects,
	const std::vector<InternalMesh>& meshes) {
	std::vector<VkDrawIndexedIndirectCommand> drawIndirectCommands;
	std::vector<uint32_t> perDraw;

	std::vector<InternalFrustumCullingInfo> internalFrustumCullingInfos;
	for (size_t i = 0; i < frustumCullingInfos.size(); i++) {
		InternalFrustumCullingInfo internalFrustumCullingInfo;
		internalFrustumCullingInfo.frustum = calculateFrustumPlanes(frustumCullingInfos[i].viewProj);
		internalFrustumCullingInfo.bufferAddress = frustumCullingInfos[i].drawIndirectBuffer.bufferDeviceAddress;

		internalFrustumCullingInfos.push_back(internalFrustumCullingInfo);
	}

	std::vector<FrustumCullingObject> frustumCullingObjects;
	for (auto& it : objects) {
		const InternalObject& object = it.second;
		if (object.meshID == 0) {
			continue;
		}

		const InternalMesh& mesh = meshes[object.meshID];

		VkDrawIndexedIndirectCommand drawIndirectCommand;
		drawIndirectCommand.indexCount = mesh.indexCount;
		drawIndirectCommand.instanceCount = 1;
		drawIndirectCommand.firstIndex = mesh.firstIndex;
		drawIndirectCommand.vertexOffset = mesh.vertexOffset;
		drawIndirectCommand.firstInstance = 0;
		drawIndirectCommands.push_back(drawIndirectCommand);
		perDraw.push_back(object.index);

		const NtshEngn::Transform& entityTransform = m_ecs->getComponent<NtshEngn::Transform>(it.first);

		FrustumCullingObject frustumCullingObject;
		frustumCullingObject.position = NtshEngn::Math::vec4(entityTransform.position, 0.0f);
		frustumCullingObject.rotation = NtshEngn::Math::rotate(entityTransform.rotation.x, NtshEngn::Math::vec3(1.0f, 0.0f, 0.0f)) *
			NtshEngn::Math::rotate(entityTransform.rotation.y, NtshEngn::Math::vec3(0.0f, 1.0f, 0.0f)) *
			NtshEngn::Math::rotate(entityTransform.rotation.z, NtshEngn::Math::vec3(0.0f, 0.0f, 1.0f));
		frustumCullingObject.scale = NtshEngn::Math::vec4(entityTransform.scale, 0.0f);
		frustumCullingObject.aabbMin = NtshEngn::Math::vec4(mesh.aabbMin, 0.0f);
		frustumCullingObject.aabbMax = NtshEngn::Math::vec4(mesh.aabbMax, 0.0f);
		frustumCullingObjects.push_back(frustumCullingObject);
	}

	uint32_t drawIndirectCount = static_cast<uint32_t>(drawIndirectCommands.size());
	memcpy(m_inDrawIndirectBuffers[currentFrameInFlight].address, &drawIndirectCount, sizeof(uint32_t));
	memcpy(reinterpret_cast<char*>(m_inDrawIndirectBuffers[currentFrameInFlight].address) + sizeof(uint32_t), drawIndirectCommands.data(), sizeof(VkDrawIndexedIndirectCommand) * drawIndirectCommands.size());
	memcpy(m_inPerDrawBuffers[currentFrameInFlight].address, perDraw.data(), sizeof(uint32_t) * perDraw.size());

	uint32_t frustumCount = static_cast<uint32_t>(internalFrustumCullingInfos.size());
	memcpy(m_frustumCullingInfoBuffers[currentFrameInFlight].address, &frustumCount, sizeof(uint32_t));
	memcpy(reinterpret_cast<char*>(m_frustumCullingInfoBuffers[currentFrameInFlight].address) + (sizeof(uint32_t) * 4), internalFrustumCullingInfos.data(), sizeof(InternalFrustumCullingInfo) * internalFrustumCullingInfos.size());
	memcpy(m_frustumCullingObjectBuffers[currentFrameInFlight].address, frustumCullingObjects.data(), sizeof(FrustumCullingObject) * frustumCullingObjects.size());

	std::vector<VkBufferMemoryBarrier2> beforeFillBufferMemoryBarriers;
	for (size_t i = 0; i < frustumCullingInfos.size(); i++) {
		VkBufferMemoryBarrier2 beforeFillBufferDrawCountMemoryBarrier = {};
		beforeFillBufferDrawCountMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
		beforeFillBufferDrawCountMemoryBarrier.pNext = nullptr;
		beforeFillBufferDrawCountMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
		beforeFillBufferDrawCountMemoryBarrier.srcAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
		beforeFillBufferDrawCountMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		beforeFillBufferDrawCountMemoryBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		beforeFillBufferDrawCountMemoryBarrier.srcQueueFamilyIndex = m_computeQueueFamilyIndex;
		beforeFillBufferDrawCountMemoryBarrier.dstQueueFamilyIndex = m_computeQueueFamilyIndex;
		beforeFillBufferDrawCountMemoryBarrier.buffer = frustumCullingInfos[i].drawIndirectBuffer.handle;
		beforeFillBufferDrawCountMemoryBarrier.offset = 0;
		beforeFillBufferDrawCountMemoryBarrier.size = sizeof(uint32_t);
		beforeFillBufferMemoryBarriers.push_back(beforeFillBufferDrawCountMemoryBarrier);
	}

	VkDependencyInfo beforeFillBufferDependencyInfo = {};
	beforeFillBufferDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	beforeFillBufferDependencyInfo.pNext = nullptr;
	beforeFillBufferDependencyInfo.dependencyFlags = 0;
	beforeFillBufferDependencyInfo.memoryBarrierCount = 0;
	beforeFillBufferDependencyInfo.pMemoryBarriers = nullptr;
	beforeFillBufferDependencyInfo.bufferMemoryBarrierCount = static_cast<uint32_t>(beforeFillBufferMemoryBarriers.size());
	beforeFillBufferDependencyInfo.pBufferMemoryBarriers = beforeFillBufferMemoryBarriers.data();
	beforeFillBufferDependencyInfo.imageMemoryBarrierCount = 0;
	beforeFillBufferDependencyInfo.pImageMemoryBarriers = nullptr;
	m_vkCmdPipelineBarrier2KHR(commandBuffer, &beforeFillBufferDependencyInfo);

	for (size_t i = 0; i < frustumCullingInfos.size(); i++) {
		vkCmdFillBuffer(commandBuffer, frustumCullingInfos[i].drawIndirectBuffer.handle, 0, sizeof(uint32_t), 0);
	}

	std::vector<VkBufferMemoryBarrier2> beforeDispatchBufferMemoryBarriers;
	for (size_t i = 0; i < frustumCullingInfos.size(); i++) {
		VkBufferMemoryBarrier2 beforeDispatchDrawIndirectCountBufferMemoryBarrier = {};
		beforeDispatchDrawIndirectCountBufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
		beforeDispatchDrawIndirectCountBufferMemoryBarrier.pNext = nullptr;
		beforeDispatchDrawIndirectCountBufferMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
		beforeDispatchDrawIndirectCountBufferMemoryBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		beforeDispatchDrawIndirectCountBufferMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
		beforeDispatchDrawIndirectCountBufferMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
		beforeDispatchDrawIndirectCountBufferMemoryBarrier.srcQueueFamilyIndex = m_computeQueueFamilyIndex;
		beforeDispatchDrawIndirectCountBufferMemoryBarrier.dstQueueFamilyIndex = m_computeQueueFamilyIndex;
		beforeDispatchDrawIndirectCountBufferMemoryBarrier.buffer = frustumCullingInfos[i].drawIndirectBuffer.handle;
		beforeDispatchDrawIndirectCountBufferMemoryBarrier.offset = 0;
		beforeDispatchDrawIndirectCountBufferMemoryBarrier.size = sizeof(uint32_t);
		beforeDispatchBufferMemoryBarriers.push_back(beforeDispatchDrawIndirectCountBufferMemoryBarrier);

		VkBufferMemoryBarrier2 beforeDispatchDrawIndirectBufferMemoryBarrier = {};
		beforeDispatchDrawIndirectBufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
		beforeDispatchDrawIndirectBufferMemoryBarrier.pNext = nullptr;
		beforeDispatchDrawIndirectBufferMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
		beforeDispatchDrawIndirectBufferMemoryBarrier.srcAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
		beforeDispatchDrawIndirectBufferMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
		beforeDispatchDrawIndirectBufferMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
		beforeDispatchDrawIndirectBufferMemoryBarrier.srcQueueFamilyIndex = m_computeQueueFamilyIndex;
		beforeDispatchDrawIndirectBufferMemoryBarrier.dstQueueFamilyIndex = m_computeQueueFamilyIndex;
		beforeDispatchDrawIndirectBufferMemoryBarrier.buffer = frustumCullingInfos[i].drawIndirectBuffer.handle;
		beforeDispatchDrawIndirectBufferMemoryBarrier.offset = sizeof(uint32_t);
		beforeDispatchDrawIndirectBufferMemoryBarrier.size = DRAW_INDIRECT_MAX_ENTITIES_SIZE - sizeof(uint32_t);
		beforeDispatchBufferMemoryBarriers.push_back(beforeDispatchDrawIndirectBufferMemoryBarrier);

		VkBufferMemoryBarrier2 beforeDispatchPerDrawBufferMemoryBarrier = {};
		beforeDispatchPerDrawBufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
		beforeDispatchPerDrawBufferMemoryBarrier.pNext = nullptr;
		beforeDispatchPerDrawBufferMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
		beforeDispatchPerDrawBufferMemoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
		beforeDispatchPerDrawBufferMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
		beforeDispatchPerDrawBufferMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
		beforeDispatchPerDrawBufferMemoryBarrier.srcQueueFamilyIndex = m_computeQueueFamilyIndex;
		beforeDispatchPerDrawBufferMemoryBarrier.dstQueueFamilyIndex = m_computeQueueFamilyIndex;
		beforeDispatchPerDrawBufferMemoryBarrier.buffer = frustumCullingInfos[i].drawIndirectBuffer.handle;
		beforeDispatchPerDrawBufferMemoryBarrier.offset = DRAW_INDIRECT_MAX_ENTITIES_SIZE;
		beforeDispatchPerDrawBufferMemoryBarrier.size = PER_DRAW_MAX_ENTITIES_SIZE;
		beforeDispatchBufferMemoryBarriers.push_back(beforeDispatchPerDrawBufferMemoryBarrier);
	}

	VkDependencyInfo beforeDispatchDependencyInfo = {};
	beforeDispatchDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	beforeDispatchDependencyInfo.pNext = nullptr;
	beforeDispatchDependencyInfo.dependencyFlags = 0;
	beforeDispatchDependencyInfo.memoryBarrierCount = 0;
	beforeDispatchDependencyInfo.pMemoryBarriers = nullptr;
	beforeDispatchDependencyInfo.bufferMemoryBarrierCount = static_cast<uint32_t>(beforeDispatchBufferMemoryBarriers.size());
	beforeDispatchDependencyInfo.pBufferMemoryBarriers = beforeDispatchBufferMemoryBarriers.data();
	beforeDispatchDependencyInfo.imageMemoryBarrierCount = 0;
	beforeDispatchDependencyInfo.pImageMemoryBarriers = nullptr;
	m_vkCmdPipelineBarrier2KHR(commandBuffer, &beforeDispatchDependencyInfo);

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, m_computePipelineLayout, 0, 1, &m_descriptorSets[currentFrameInFlight], 0, nullptr);

	vkCmdDispatch(commandBuffer, ((drawIndirectCount + 8 - 1) / 8), ((frustumCount + 8 - 1) / 8), 1);

	std::vector<VkBufferMemoryBarrier2> indirectDrawBufferMemoryBarriers;
	for (size_t i = 0; i < frustumCullingInfos.size(); i++) {
		VkBufferMemoryBarrier2 drawIndirectBufferMemoryBarrier = {};
		drawIndirectBufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
		drawIndirectBufferMemoryBarrier.pNext = nullptr;
		drawIndirectBufferMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
		drawIndirectBufferMemoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
		drawIndirectBufferMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
		drawIndirectBufferMemoryBarrier.dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
		drawIndirectBufferMemoryBarrier.srcQueueFamilyIndex = m_computeQueueFamilyIndex;
		drawIndirectBufferMemoryBarrier.dstQueueFamilyIndex = m_computeQueueFamilyIndex;
		drawIndirectBufferMemoryBarrier.buffer = frustumCullingInfos[i].drawIndirectBuffer.handle;
		drawIndirectBufferMemoryBarrier.offset = 0;
		drawIndirectBufferMemoryBarrier.size = DRAW_INDIRECT_MAX_ENTITIES_SIZE;
		indirectDrawBufferMemoryBarriers.push_back(drawIndirectBufferMemoryBarrier);

		VkBufferMemoryBarrier2 perDrawBufferMemoryBarrier = {};
		perDrawBufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
		perDrawBufferMemoryBarrier.pNext = nullptr;
		perDrawBufferMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
		perDrawBufferMemoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
		perDrawBufferMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
		perDrawBufferMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
		perDrawBufferMemoryBarrier.srcQueueFamilyIndex = m_computeQueueFamilyIndex;
		perDrawBufferMemoryBarrier.dstQueueFamilyIndex = m_computeQueueFamilyIndex;
		perDrawBufferMemoryBarrier.buffer = frustumCullingInfos[i].drawIndirectBuffer.handle;
		perDrawBufferMemoryBarrier.offset = DRAW_INDIRECT_MAX_ENTITIES_SIZE;
		perDrawBufferMemoryBarrier.size = PER_DRAW_MAX_ENTITIES_SIZE;
		indirectDrawBufferMemoryBarriers.push_back(perDrawBufferMemoryBarrier);
	}

	VkDependencyInfo indirectDrawDependencyInfo = {};
	indirectDrawDependencyInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
	indirectDrawDependencyInfo.pNext = nullptr;
	indirectDrawDependencyInfo.dependencyFlags = 0;
	indirectDrawDependencyInfo.memoryBarrierCount = 0;
	indirectDrawDependencyInfo.pMemoryBarriers = nullptr;
	indirectDrawDependencyInfo.bufferMemoryBarrierCount = static_cast<uint32_t>(indirectDrawBufferMemoryBarriers.size());
	indirectDrawDependencyInfo.pBufferMemoryBarriers = indirectDrawBufferMemoryBarriers.data();
	indirectDrawDependencyInfo.imageMemoryBarrierCount = 0;
	indirectDrawDependencyInfo.pImageMemoryBarriers = nullptr;
	m_vkCmdPipelineBarrier2KHR(commandBuffer, &indirectDrawDependencyInfo);

	return drawIndirectCount;
}

void FrustumCulling::createBuffers() {
	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.pNext = nullptr;
	bufferCreateInfo.flags = 0;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bufferCreateInfo.queueFamilyIndexCount = 1;
	bufferCreateInfo.pQueueFamilyIndices = &m_computeQueueFamilyIndex;

	VmaAllocationInfo allocationInfo;

	VmaAllocationCreateInfo bufferAllocationCreateInfo = {};
	bufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
	bufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;

	// Create input draw indirect buffers
	m_inDrawIndirectBuffers.resize(m_framesInFlight);
	bufferCreateInfo.size = DRAW_INDIRECT_MAX_ENTITIES_SIZE;
	bufferCreateInfo.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &bufferCreateInfo, &bufferAllocationCreateInfo, &m_inDrawIndirectBuffers[i].handle, &m_inDrawIndirectBuffers[i].allocation, &allocationInfo));
		m_inDrawIndirectBuffers[i].address = allocationInfo.pMappedData;
	}

	// Create input per draw buffers
	m_inPerDrawBuffers.resize(m_framesInFlight);
	bufferCreateInfo.size = PER_DRAW_MAX_ENTITIES_SIZE;
	bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &bufferCreateInfo, &bufferAllocationCreateInfo, &m_inPerDrawBuffers[i].handle, &m_inPerDrawBuffers[i].allocation, &allocationInfo));
		m_inPerDrawBuffers[i].address = allocationInfo.pMappedData;
	}

	// Create frustum culling info buffers
	m_frustumCullingInfoBuffers.resize(m_framesInFlight);
	bufferCreateInfo.size = 65536;
	bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &bufferCreateInfo, &bufferAllocationCreateInfo, &m_frustumCullingInfoBuffers[i].handle, &m_frustumCullingInfoBuffers[i].allocation, &allocationInfo));
		m_frustumCullingInfoBuffers[i].address = allocationInfo.pMappedData;
	}

	// Create frustum culling object buffers
	m_frustumCullingObjectBuffers.resize(m_framesInFlight);
	bufferCreateInfo.size = sizeof(FrustumCullingObject) * NTSHENGN_MAX_ENTITIES;
	bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &bufferCreateInfo, &bufferAllocationCreateInfo, &m_frustumCullingObjectBuffers[i].handle, &m_frustumCullingObjectBuffers[i].allocation, &allocationInfo));
		m_frustumCullingObjectBuffers[i].address = allocationInfo.pMappedData;
	}
}

void FrustumCulling::createDescriptorSetLayout() {
	VkDescriptorSetLayoutBinding frustumDescriptorSetLayoutBinding = {};
	frustumDescriptorSetLayoutBinding.binding = 0;
	frustumDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	frustumDescriptorSetLayoutBinding.descriptorCount = 1;
	frustumDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	frustumDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding frustumCullingObjectDescriptorSetLayoutBinding = {};
	frustumCullingObjectDescriptorSetLayoutBinding.binding = 1;
	frustumCullingObjectDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	frustumCullingObjectDescriptorSetLayoutBinding.descriptorCount = 1;
	frustumCullingObjectDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	frustumCullingObjectDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding inDrawIndirectDescriptorSetLayoutBinding = {};
	inDrawIndirectDescriptorSetLayoutBinding.binding = 2;
	inDrawIndirectDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	inDrawIndirectDescriptorSetLayoutBinding.descriptorCount = 1;
	inDrawIndirectDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	inDrawIndirectDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding inPerDrawDescriptorSetLayoutBinding = {};
	inPerDrawDescriptorSetLayoutBinding.binding = 3;
	inPerDrawDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	inPerDrawDescriptorSetLayoutBinding.descriptorCount = 1;
	inPerDrawDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	inPerDrawDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	std::array<VkDescriptorSetLayoutBinding, 4> descriptorSetLayoutBindings = { frustumDescriptorSetLayoutBinding, frustumCullingObjectDescriptorSetLayoutBinding, inDrawIndirectDescriptorSetLayoutBinding, inPerDrawDescriptorSetLayoutBinding };
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.pNext = nullptr;
	descriptorSetLayoutCreateInfo.flags = 0;
	descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(descriptorSetLayoutBindings.size());
	descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings.data();
	NTSHENGN_VK_CHECK(vkCreateDescriptorSetLayout(m_device, &descriptorSetLayoutCreateInfo, nullptr, &m_descriptorSetLayout));
}

void FrustumCulling::createComputePipeline() {
	std::string computeShaderCode = R"GLSL(
		#version 460
		#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
		#extension GL_EXT_buffer_reference2 : require

		#define DRAW_INDIRECT_MAX_ENTITIES_SIZE )GLSL";
	computeShaderCode += std::to_string(DRAW_INDIRECT_MAX_ENTITIES_SIZE);
	computeShaderCode +=  R"GLSL(

		layout(local_size_x = 8, local_size_y = 8) in;

		struct FrustumCullingInfo {
			vec4 planes[6];
			uint64_t address;
		};

		struct ObjectInfo {
			vec3 position;
			mat4 rotation;
			vec3 scale;
			vec3 aabbMin;
			vec3 aabbMax;
		};

		layout(set = 0, binding = 0) restrict readonly buffer FrustumCulling {
			uint count;
			FrustumCullingInfo info[];
		} frustumCulling;

		layout(set = 0, binding = 1) restrict readonly buffer Objects {
			ObjectInfo info[];
		} objects;

		struct DrawIndirect {
			uint indexCount;
			uint instanceCount;
			uint firstIndex;
			int vertexOffset;
			uint firstInstance;
		};

		layout(set = 0, binding = 2) restrict readonly buffer InDrawIndirect {
			uint drawCount;
			DrawIndirect commands[];
		} inDrawIndirect;

		struct PerDrawInfo {
			uint objectID;
		};

		layout(set = 0, binding = 3) restrict readonly buffer InPerDraw {
			PerDrawInfo info[];
		} inPerDraw;

		layout(buffer_reference) buffer DrawIndirectBuffer {
			uint drawCount;
			DrawIndirect commands[];
		};

		layout(buffer_reference) buffer PerDrawBuffer {
			PerDrawInfo info[];
		};

		bool intersect(FrustumCullingInfo frustumCullingInfo, vec3 aabbMin, vec3 aabbMax) {
			const vec3 mmm = vec3(aabbMin.x, aabbMin.y, aabbMin.z);
			const vec3 Mmm = vec3(aabbMax.x, aabbMin.y, aabbMin.z);
			const vec3 mMm = vec3(aabbMin.x, aabbMax.y, aabbMin.z);
			const vec3 MMm = vec3(aabbMax.x, aabbMax.y, aabbMin.z);
			const vec3 mmM = vec3(aabbMin.x, aabbMin.y, aabbMax.z);
			const vec3 MmM = vec3(aabbMax.x, aabbMin.y, aabbMax.z);
			const vec3 mMM = vec3(aabbMin.x, aabbMax.y, aabbMax.z);
			const vec3 MMM = vec3(aabbMax.x, aabbMax.y, aabbMax.z);
			for (uint i = 0; i < 6; i++) {
				if (((dot(frustumCullingInfo.planes[i].xyz, mmm) + frustumCullingInfo.planes[i].w) <= 0.0f)
					&& ((dot(frustumCullingInfo.planes[i].xyz, Mmm) + frustumCullingInfo.planes[i].w) <= 0.0f)
					&& ((dot(frustumCullingInfo.planes[i].xyz, mMm) + frustumCullingInfo.planes[i].w) <= 0.0f)
					&& ((dot(frustumCullingInfo.planes[i].xyz, MMm) + frustumCullingInfo.planes[i].w) <= 0.0f)
					&& ((dot(frustumCullingInfo.planes[i].xyz, mmM) + frustumCullingInfo.planes[i].w) <= 0.0f)
					&& ((dot(frustumCullingInfo.planes[i].xyz, MmM) + frustumCullingInfo.planes[i].w) <= 0.0f)
					&& ((dot(frustumCullingInfo.planes[i].xyz, mMM) + frustumCullingInfo.planes[i].w) <= 0.0f)
					&& ((dot(frustumCullingInfo.planes[i].xyz, MMM) + frustumCullingInfo.planes[i].w) <= 0.0f)) {
					return false;
				}
			}

			return true;
		}

		void main() {
			uint objectIndex = gl_GlobalInvocationID.x;
			if (objectIndex >= inDrawIndirect.drawCount) {
				return;
			}

			uint frustumIndex = gl_GlobalInvocationID.y;
			if (frustumIndex >= frustumCulling.count) {
				return;
			}

			FrustumCullingInfo frustumCullingInfo = frustumCulling.info[frustumIndex];

			vec3 aabbMin = objects.info[objectIndex].aabbMin;
			vec3 aabbMax = objects.info[objectIndex].aabbMax;

			vec3 newAABBMin = objects.info[objectIndex].position;
			vec3 newAABBMax = objects.info[objectIndex].position;

			float a;
			float b;

			for (uint i = 0; i < 3; i++) {
				for (uint j = 0; j < 3; j++) {
					a = objects.info[objectIndex].rotation[j][i] * aabbMin[j] * abs(objects.info[objectIndex].scale[j]);
					b = objects.info[objectIndex].rotation[j][i] * aabbMax[j] * abs(objects.info[objectIndex].scale[j]);

					newAABBMin[i] += (a < b) ? a : b;
					newAABBMax[i] += (a < b) ? b : a;
				}
			}

			aabbMin = newAABBMin;
			aabbMax = newAABBMax;

			if (intersect(frustumCullingInfo, aabbMin, aabbMax)) {
				uint drawIndex = atomicAdd(DrawIndirectBuffer(frustumCullingInfo.address).drawCount, 1);

				DrawIndirectBuffer(frustumCullingInfo.address).commands[drawIndex] = inDrawIndirect.commands[objectIndex];
				PerDrawBuffer(frustumCullingInfo.address + DRAW_INDIRECT_MAX_ENTITIES_SIZE).info[drawIndex] = inPerDraw.info[objectIndex];
			}
		}
	)GLSL";
	const std::vector<uint32_t> computeShaderSpv = compileShader(computeShaderCode, ShaderType::Compute);

	VkShaderModule computeShaderModule;
	VkShaderModuleCreateInfo computeShaderModuleCreateInfo = {};
	computeShaderModuleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	computeShaderModuleCreateInfo.pNext = nullptr;
	computeShaderModuleCreateInfo.flags = 0;
	computeShaderModuleCreateInfo.codeSize = computeShaderSpv.size() * sizeof(uint32_t);
	computeShaderModuleCreateInfo.pCode = computeShaderSpv.data();
	NTSHENGN_VK_CHECK(vkCreateShaderModule(m_device, &computeShaderModuleCreateInfo, nullptr, &computeShaderModule));

	VkPipelineShaderStageCreateInfo computeShaderStageCreateInfo = {};
	computeShaderStageCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	computeShaderStageCreateInfo.pNext = nullptr;
	computeShaderStageCreateInfo.flags = 0;
	computeShaderStageCreateInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	computeShaderStageCreateInfo.module = computeShaderModule;
	computeShaderStageCreateInfo.pName = "main";
	computeShaderStageCreateInfo.pSpecializationInfo = nullptr;

	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = {};
	pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipelineLayoutCreateInfo.pNext = nullptr;
	pipelineLayoutCreateInfo.flags = 0;
	pipelineLayoutCreateInfo.setLayoutCount = 1;
	pipelineLayoutCreateInfo.pSetLayouts = &m_descriptorSetLayout;
	pipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pipelineLayoutCreateInfo.pPushConstantRanges = nullptr;
	NTSHENGN_VK_CHECK(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_computePipelineLayout));

	VkComputePipelineCreateInfo computePipelineCreateInfo = {};
	computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	computePipelineCreateInfo.pNext = nullptr;
	computePipelineCreateInfo.flags = 0;
	computePipelineCreateInfo.stage = computeShaderStageCreateInfo;
	computePipelineCreateInfo.layout = m_computePipelineLayout;
	computePipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE;
	computePipelineCreateInfo.basePipelineIndex = 0;
	NTSHENGN_VK_CHECK(vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &m_computePipeline));

	vkDestroyShaderModule(m_device, computeShaderModule, nullptr);
}

void FrustumCulling::createDescriptorSets() {
	// Create descriptor pool
	VkDescriptorPoolSize frustumDescriptorPoolSize = {};
	frustumDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	frustumDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize frustumCullingObjectDescriptorPoolSize = {};
	frustumCullingObjectDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	frustumCullingObjectDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize inDrawIndirectDescriptorPoolSize = {};
	inDrawIndirectDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	inDrawIndirectDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize inPerDrawDescriptorPoolSize = {};
	inPerDrawDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	inPerDrawDescriptorPoolSize.descriptorCount = m_framesInFlight;

	std::array<VkDescriptorPoolSize, 4> descriptorPoolSizes = { frustumDescriptorPoolSize, frustumCullingObjectDescriptorPoolSize, inDrawIndirectDescriptorPoolSize, inPerDrawDescriptorPoolSize };
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = {};
	descriptorPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolCreateInfo.pNext = nullptr;
	descriptorPoolCreateInfo.flags = 0;
	descriptorPoolCreateInfo.maxSets = m_framesInFlight;
	descriptorPoolCreateInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size());
	descriptorPoolCreateInfo.pPoolSizes = descriptorPoolSizes.data();
	NTSHENGN_VK_CHECK(vkCreateDescriptorPool(m_device, &descriptorPoolCreateInfo, nullptr, &m_descriptorPool));

	// Allocate descriptor sets
	m_descriptorSets.resize(m_framesInFlight);
	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
	descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	descriptorSetAllocateInfo.pNext = nullptr;
	descriptorSetAllocateInfo.descriptorPool = m_descriptorPool;
	descriptorSetAllocateInfo.descriptorSetCount = 1;
	descriptorSetAllocateInfo.pSetLayouts = &m_descriptorSetLayout;
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		NTSHENGN_VK_CHECK(vkAllocateDescriptorSets(m_device, &descriptorSetAllocateInfo, &m_descriptorSets[i]));
	}

	// Update descriptor sets
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		std::vector<VkWriteDescriptorSet> writeDescriptorSets;

		VkDescriptorBufferInfo frustumCullingInfoDescriptorBufferInfo;
		frustumCullingInfoDescriptorBufferInfo.buffer = m_frustumCullingInfoBuffers[i].handle;
		frustumCullingInfoDescriptorBufferInfo.offset = 0;
		frustumCullingInfoDescriptorBufferInfo.range = 65536;

		VkWriteDescriptorSet frustumCullingInfoDescriptorWriteDescriptorSet = {};
		frustumCullingInfoDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		frustumCullingInfoDescriptorWriteDescriptorSet.pNext = nullptr;
		frustumCullingInfoDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		frustumCullingInfoDescriptorWriteDescriptorSet.dstBinding = 0;
		frustumCullingInfoDescriptorWriteDescriptorSet.dstArrayElement = 0;
		frustumCullingInfoDescriptorWriteDescriptorSet.descriptorCount = 1;
		frustumCullingInfoDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		frustumCullingInfoDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		frustumCullingInfoDescriptorWriteDescriptorSet.pBufferInfo = &frustumCullingInfoDescriptorBufferInfo;
		frustumCullingInfoDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;
		writeDescriptorSets.push_back(frustumCullingInfoDescriptorWriteDescriptorSet);

		VkDescriptorBufferInfo frustumCullingObjectDescriptorBufferInfo;
		frustumCullingObjectDescriptorBufferInfo.buffer = m_frustumCullingObjectBuffers[i].handle;
		frustumCullingObjectDescriptorBufferInfo.offset = 0;
		frustumCullingObjectDescriptorBufferInfo.range = sizeof(FrustumCullingObject) * NTSHENGN_MAX_ENTITIES;

		VkWriteDescriptorSet frustumCullingObjectDescriptorWriteDescriptorSet = {};
		frustumCullingObjectDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		frustumCullingObjectDescriptorWriteDescriptorSet.pNext = nullptr;
		frustumCullingObjectDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		frustumCullingObjectDescriptorWriteDescriptorSet.dstBinding = 1;
		frustumCullingObjectDescriptorWriteDescriptorSet.dstArrayElement = 0;
		frustumCullingObjectDescriptorWriteDescriptorSet.descriptorCount = 1;
		frustumCullingObjectDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		frustumCullingObjectDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		frustumCullingObjectDescriptorWriteDescriptorSet.pBufferInfo = &frustumCullingObjectDescriptorBufferInfo;
		frustumCullingObjectDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;
		writeDescriptorSets.push_back(frustumCullingObjectDescriptorWriteDescriptorSet);

		VkDescriptorBufferInfo inDrawIndirectDescriptorBufferInfo;
		inDrawIndirectDescriptorBufferInfo.buffer = m_inDrawIndirectBuffers[i].handle;
		inDrawIndirectDescriptorBufferInfo.offset = 0;
		inDrawIndirectDescriptorBufferInfo.range = DRAW_INDIRECT_MAX_ENTITIES_SIZE;

		VkWriteDescriptorSet inDrawIndirectDescriptorWriteDescriptorSet = {};
		inDrawIndirectDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		inDrawIndirectDescriptorWriteDescriptorSet.pNext = nullptr;
		inDrawIndirectDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		inDrawIndirectDescriptorWriteDescriptorSet.dstBinding = 2;
		inDrawIndirectDescriptorWriteDescriptorSet.dstArrayElement = 0;
		inDrawIndirectDescriptorWriteDescriptorSet.descriptorCount = 1;
		inDrawIndirectDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		inDrawIndirectDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		inDrawIndirectDescriptorWriteDescriptorSet.pBufferInfo = &inDrawIndirectDescriptorBufferInfo;
		inDrawIndirectDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;
		writeDescriptorSets.push_back(inDrawIndirectDescriptorWriteDescriptorSet);

		VkDescriptorBufferInfo inPerDrawDescriptorBufferInfo;
		inPerDrawDescriptorBufferInfo.buffer = m_inPerDrawBuffers[i].handle;
		inPerDrawDescriptorBufferInfo.offset = 0;
		inPerDrawDescriptorBufferInfo.range = PER_DRAW_MAX_ENTITIES_SIZE;

		VkWriteDescriptorSet inPerDrawDescriptorWriteDescriptorSet = {};
		inPerDrawDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		inPerDrawDescriptorWriteDescriptorSet.pNext = nullptr;
		inPerDrawDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		inPerDrawDescriptorWriteDescriptorSet.dstBinding = 3;
		inPerDrawDescriptorWriteDescriptorSet.dstArrayElement = 0;
		inPerDrawDescriptorWriteDescriptorSet.descriptorCount = 1;
		inPerDrawDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		inPerDrawDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		inPerDrawDescriptorWriteDescriptorSet.pBufferInfo = &inPerDrawDescriptorBufferInfo;
		inPerDrawDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;
		writeDescriptorSets.push_back(inPerDrawDescriptorWriteDescriptorSet);

		vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}
}

Frustum FrustumCulling::calculateFrustumPlanes(const NtshEngn::Math::mat4& viewProj) {
	Frustum frustum;
	frustum[0].x = viewProj[0][3] + viewProj[0][0];
	frustum[0].y = viewProj[1][3] + viewProj[1][0];
	frustum[0].z = viewProj[2][3] + viewProj[2][0];
	frustum[0].w = viewProj[3][3] + viewProj[3][0];

	frustum[1].x = viewProj[0][3] - viewProj[0][0];
	frustum[1].y = viewProj[1][3] - viewProj[1][0];
	frustum[1].z = viewProj[2][3] - viewProj[2][0];
	frustum[1].w = viewProj[3][3] - viewProj[3][0];

	frustum[2].x = viewProj[0][3] + viewProj[0][1];
	frustum[2].y = viewProj[1][3] + viewProj[1][1];
	frustum[2].z = viewProj[2][3] + viewProj[2][1];
	frustum[2].w = viewProj[3][3] + viewProj[3][1];

	frustum[3].x = viewProj[0][3] - viewProj[0][1];
	frustum[3].y = viewProj[1][3] - viewProj[1][1];
	frustum[3].z = viewProj[2][3] - viewProj[2][1];
	frustum[3].w = viewProj[3][3] - viewProj[3][1];

	frustum[4].x = viewProj[0][3] + viewProj[0][2];
	frustum[4].y = viewProj[1][3] + viewProj[1][2];
	frustum[4].z = viewProj[2][3] + viewProj[2][2];
	frustum[4].w = viewProj[3][3] + viewProj[3][2];

	frustum[5].x = viewProj[0][3] - viewProj[0][2];
	frustum[5].y = viewProj[1][3] - viewProj[1][2];
	frustum[5].z = viewProj[2][3] - viewProj[2][2];
	frustum[5].w = viewProj[3][3] - viewProj[3][2];

	for (uint8_t i = 0; i < 6; i++) {
		const float magnitude = NtshEngn::Math::vec3(frustum[i]).length();
		frustum[i].x /= magnitude;
		frustum[i].y /= magnitude;
		frustum[i].z /= magnitude;
		frustum[i].w /= magnitude;
	}

	return frustum;
}