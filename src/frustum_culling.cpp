#include "frustum_culling.h"
#include <array>
#include <mutex>

void FrustumCulling::init(VkDevice device,
	VkQueue computeQueue,
	uint32_t computeQueueFamilyIndex,
	VmaAllocator allocator,
	uint32_t framesInFlight,
	PFN_vkCmdPipelineBarrier2KHR vkCmdPipelineBarrier2KHR, 
	NtshEngn::JobSystemInterface* jobSystem,
	NtshEngn::ECSInterface* ecs) {
	m_device = device;
	m_computeQueue = computeQueue;
	m_computeQueueFamilyIndex = computeQueueFamilyIndex;
	m_allocator = allocator;
	m_framesInFlight = framesInFlight;
	m_vkCmdPipelineBarrier2KHR = vkCmdPipelineBarrier2KHR;
	m_jobSystem = jobSystem;
	m_ecs = ecs;

	createBuffers();
#if FRUSTUM_CULLING_TYPE == FRUSTUM_CULLING_GPU
	createDescriptorSetLayout();
	createComputePipeline();
	createDescriptorSets();
#endif
}

void FrustumCulling::destroy() {
#if FRUSTUM_CULLING_TYPE == FRUSTUM_CULLING_GPU
	vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);

	vkDestroyPipeline(m_device, m_computePipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_computePipelineLayout, nullptr);

	vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);

	m_gpuPerDrawBuffer.destroy(m_allocator);
	m_gpuDrawIndirectBuffer.destroy(m_allocator);

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		m_gpuFrustumCullingBuffers[i].destroy(m_allocator);
	}
#endif

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		m_perDrawBuffers[i].destroy(m_allocator);
	}

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		m_drawIndirectBuffers[i].destroy(m_allocator);
	}
}

uint32_t FrustumCulling::culling(VkCommandBuffer commandBuffer,
	uint32_t currentFrameInFlight,
	const NtshEngn::Math::mat4& cameraView,
	const NtshEngn::Math::mat4& cameraProjection,
	const std::unordered_map<NtshEngn::Entity, InternalObject>& objects,
	const std::vector<InternalMesh>& meshes) {
#if FRUSTUM_CULLING_TYPE != FRUSTUM_CULLING_GPU
	NTSHENGN_UNUSED(commandBuffer);
#endif

	std::vector<VkDrawIndexedIndirectCommand> drawIndirectCommands;
	std::vector<uint32_t> perDraw;

#if FRUSTUM_CULLING_TYPE != FRUSTUM_CULLING_DISABLED
	const NtshEngn::Math::mat4 viewProj = cameraProjection * cameraView;

	std::array<NtshEngn::Math::vec4, 6> frustum;
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
#endif

#if FRUSTUM_CULLING_TYPE == FRUSTUM_CULLING_CPU_MULTITHREADED
	std::mutex mutex;

	m_jobSystem->dispatch(static_cast<uint32_t>(objects.size()), (static_cast<uint32_t>(objects.size()) + m_jobSystem->getNumThreads() - 1) / m_jobSystem->getNumThreads(), [this, &drawIndirectCommands, &perDraw, &mutex, &frustum, &objects, &meshes](NtshEngn::JobDispatchArguments args) {
		auto it = objects.begin();
		std::advance(it, args.jobIndex);

		const InternalObject& object = it->second;
		const InternalMesh& mesh = meshes[object.meshID];

		NtshEngn::Math::vec3 aabbMin = mesh.aabbMin;
		NtshEngn::Math::vec3 aabbMax = mesh.aabbMax;

		const NtshEngn::Transform& entityTransform = m_ecs->getComponent<NtshEngn::Transform>(it->first);

		NtshEngn::Math::vec3 newAABBMin = entityTransform.position;
		NtshEngn::Math::vec3 newAABBMax = entityTransform.position;

		float a;
		float b;

		const NtshEngn::Math::mat4 rotationMatrix = NtshEngn::Math::rotate(entityTransform.rotation.x, NtshEngn::Math::vec3(1.0f, 0.0f, 0.0f)) *
			NtshEngn::Math::rotate(entityTransform.rotation.y, NtshEngn::Math::vec3(0.0f, 1.0f, 0.0f)) *
			NtshEngn::Math::rotate(entityTransform.rotation.z, NtshEngn::Math::vec3(0.0f, 0.0f, 1.0f));

		for (uint8_t i = 0; i < 3; i++) {
			for (uint8_t j = 0; j < 3; j++) {
				a = rotationMatrix[j][i] * aabbMin[j] * std::abs(entityTransform.scale[i]);
				b = rotationMatrix[j][i] * aabbMax[j] * std::abs(entityTransform.scale[i]);

				newAABBMin[i] += (a < b) ? a : b;
				newAABBMax[i] += (a < b) ? b : a;
			}
		}

		aabbMin = newAABBMin;
		aabbMax = newAABBMax;

		for (uint8_t i = 0; i < 6; i++) {
			if (((frustum[i].x * aabbMin.x + frustum[i].y * aabbMin.y + frustum[i].z * aabbMin.z + frustum[i].w) <= 0.0f)
				&& ((frustum[i].x * aabbMax.x + frustum[i].y * aabbMin.y + frustum[i].z * aabbMin.z + frustum[i].w) <= 0.0f)
				&& ((frustum[i].x * aabbMin.x + frustum[i].y * aabbMax.y + frustum[i].z * aabbMin.z + frustum[i].w) <= 0.0f)
				&& ((frustum[i].x * aabbMax.x + frustum[i].y * aabbMax.y + frustum[i].z * aabbMin.z + frustum[i].w) <= 0.0f)
				&& ((frustum[i].x * aabbMin.x + frustum[i].y * aabbMin.y + frustum[i].z * aabbMax.z + frustum[i].w) <= 0.0f)
				&& ((frustum[i].x * aabbMax.x + frustum[i].y * aabbMin.y + frustum[i].z * aabbMax.z + frustum[i].w) <= 0.0f)
				&& ((frustum[i].x * aabbMin.x + frustum[i].y * aabbMax.y + frustum[i].z * aabbMax.z + frustum[i].w) <= 0.0f)
				&& ((frustum[i].x * aabbMax.x + frustum[i].y * aabbMax.y + frustum[i].z * aabbMax.z + frustum[i].w) <= 0.0f)) {
				return;
			}
		}

		VkDrawIndexedIndirectCommand drawIndirectCommand;
		drawIndirectCommand.indexCount = mesh.indexCount;
		drawIndirectCommand.instanceCount = 1;
		drawIndirectCommand.firstIndex = mesh.firstIndex;
		drawIndirectCommand.vertexOffset = mesh.vertexOffset;
		drawIndirectCommand.firstInstance = 0;

		std::unique_lock<std::mutex> lock(mutex);
		drawIndirectCommands.push_back(drawIndirectCommand);
		perDraw.push_back(object.index);
		lock.unlock();
		});

	m_jobSystem->wait();
#elif FRUSTUM_CULLING_TYPE == FRUSTUM_CULLING_CPU_SINGLETHREADED
	for (auto& it : objects) {
		const InternalObject& object = it.second;
		const InternalMesh& mesh = meshes[object.meshID];

		NtshEngn::Math::vec3 aabbMin = mesh.aabbMin;
		NtshEngn::Math::vec3 aabbMax = mesh.aabbMax;

		const NtshEngn::Transform& entityTransform = m_ecs->getComponent<NtshEngn::Transform>(it.first);

		NtshEngn::Math::vec3 newAABBMin = entityTransform.position;
		NtshEngn::Math::vec3 newAABBMax = entityTransform.position;

		float a;
		float b;

		const NtshEngn::Math::mat4 rotationMatrix = NtshEngn::Math::rotate(entityTransform.rotation.x, NtshEngn::Math::vec3(1.0f, 0.0f, 0.0f)) *
			NtshEngn::Math::rotate(entityTransform.rotation.y, NtshEngn::Math::vec3(0.0f, 1.0f, 0.0f)) *
			NtshEngn::Math::rotate(entityTransform.rotation.z, NtshEngn::Math::vec3(0.0f, 0.0f, 1.0f));

		for (uint8_t i = 0; i < 3; i++) {
			for (uint8_t j = 0; j < 3; j++) {
				a = rotationMatrix[j][i] * aabbMin[j] * std::abs(entityTransform.scale[i]);
				b = rotationMatrix[j][i] * aabbMax[j] * std::abs(entityTransform.scale[i]);

				newAABBMin[i] += (a < b) ? a : b;
				newAABBMax[i] += (a < b) ? b : a;
			}
		}

		aabbMin = newAABBMin;
		aabbMax = newAABBMax;

		bool inFrustum = true;
		for (uint8_t i = 0; i < 6; i++) {
			if (((frustum[i].x * aabbMin.x + frustum[i].y * aabbMin.y + frustum[i].z * aabbMin.z + frustum[i].w) <= 0.0f)
				&& ((frustum[i].x * aabbMax.x + frustum[i].y * aabbMin.y + frustum[i].z * aabbMin.z + frustum[i].w) <= 0.0f)
				&& ((frustum[i].x * aabbMin.x + frustum[i].y * aabbMax.y + frustum[i].z * aabbMin.z + frustum[i].w) <= 0.0f)
				&& ((frustum[i].x * aabbMax.x + frustum[i].y * aabbMax.y + frustum[i].z * aabbMin.z + frustum[i].w) <= 0.0f)
				&& ((frustum[i].x * aabbMin.x + frustum[i].y * aabbMin.y + frustum[i].z * aabbMax.z + frustum[i].w) <= 0.0f)
				&& ((frustum[i].x * aabbMax.x + frustum[i].y * aabbMin.y + frustum[i].z * aabbMax.z + frustum[i].w) <= 0.0f)
				&& ((frustum[i].x * aabbMin.x + frustum[i].y * aabbMax.y + frustum[i].z * aabbMax.z + frustum[i].w) <= 0.0f)
				&& ((frustum[i].x * aabbMax.x + frustum[i].y * aabbMax.y + frustum[i].z * aabbMax.z + frustum[i].w) <= 0.0f)) {
				inFrustum = false;
			}
		}

		if (inFrustum) {
			VkDrawIndexedIndirectCommand drawIndirectCommand;
			drawIndirectCommand.indexCount = mesh.indexCount;
			drawIndirectCommand.instanceCount = 1;
			drawIndirectCommand.firstIndex = mesh.firstIndex;
			drawIndirectCommand.vertexOffset = mesh.vertexOffset;
			drawIndirectCommand.firstInstance = 0;

			drawIndirectCommands.push_back(drawIndirectCommand);
			perDraw.push_back(object.index);
		}
	}
#else
#if FRUSTUM_CULLING_TYPE == FRUSTUM_CULLING_DISABLED
	NTSHENGN_UNUSED(cameraView);
	NTSHENGN_UNUSED(cameraProjection);
#endif

#if FRUSTUM_CULLING_TYPE == FRUSTUM_CULLING_GPU
	std::vector<FrustumCullingObject> frustumCullingObjects;
#endif
	for (auto& it : objects) {
		const InternalObject& object = it.second;
		const InternalMesh& mesh = meshes[object.meshID];

		VkDrawIndexedIndirectCommand drawIndirectCommand;
		drawIndirectCommand.indexCount = mesh.indexCount;
		drawIndirectCommand.instanceCount = 1;
		drawIndirectCommand.firstIndex = mesh.firstIndex;
		drawIndirectCommand.vertexOffset = mesh.vertexOffset;
		drawIndirectCommand.firstInstance = 0;
		drawIndirectCommands.push_back(drawIndirectCommand);
		perDraw.push_back(object.index);

#if FRUSTUM_CULLING_TYPE == FRUSTUM_CULLING_GPU
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
#endif
	}
#endif

	uint32_t drawIndirectCount = static_cast<uint32_t>(drawIndirectCommands.size());
	memcpy(m_drawIndirectBuffers[currentFrameInFlight].address, &drawIndirectCount, sizeof(uint32_t));
	memcpy(reinterpret_cast<char*>(m_drawIndirectBuffers[currentFrameInFlight].address) + sizeof(uint32_t), drawIndirectCommands.data(), sizeof(VkDrawIndexedIndirectCommand) * drawIndirectCommands.size());

	memcpy(m_perDrawBuffers[currentFrameInFlight].address, perDraw.data(), sizeof(uint32_t)* perDraw.size());

#if FRUSTUM_CULLING_TYPE == FRUSTUM_CULLING_GPU
	memcpy(m_gpuFrustumCullingBuffers[currentFrameInFlight].address, frustum.data(), sizeof(NtshEngn::Math::vec4) * 6);
	memcpy(reinterpret_cast<char*>(m_gpuFrustumCullingBuffers[currentFrameInFlight].address) + sizeof(NtshEngn::Math::vec4) * 6, frustumCullingObjects.data(), sizeof(FrustumCullingObject) * frustumCullingObjects.size());

	VkBufferMemoryBarrier2 beforeFillBufferDrawCountMemoryBarrier = {};
	beforeFillBufferDrawCountMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	beforeFillBufferDrawCountMemoryBarrier.pNext = nullptr;
	beforeFillBufferDrawCountMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
	beforeFillBufferDrawCountMemoryBarrier.srcAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
	beforeFillBufferDrawCountMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	beforeFillBufferDrawCountMemoryBarrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	beforeFillBufferDrawCountMemoryBarrier.srcQueueFamilyIndex = m_computeQueueFamilyIndex;
	beforeFillBufferDrawCountMemoryBarrier.dstQueueFamilyIndex = m_computeQueueFamilyIndex;
	beforeFillBufferDrawCountMemoryBarrier.buffer = m_gpuDrawIndirectBuffer.handle;
	beforeFillBufferDrawCountMemoryBarrier.offset = 0;
	beforeFillBufferDrawCountMemoryBarrier.size = sizeof(uint32_t);

	VkBufferMemoryBarrier2 beforeFillBufferDrawIndirectMemoryBarrier = {};
	beforeFillBufferDrawIndirectMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	beforeFillBufferDrawIndirectMemoryBarrier.pNext = nullptr;
	beforeFillBufferDrawIndirectMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
	beforeFillBufferDrawIndirectMemoryBarrier.srcAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
	beforeFillBufferDrawIndirectMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	beforeFillBufferDrawIndirectMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
	beforeFillBufferDrawIndirectMemoryBarrier.srcQueueFamilyIndex = m_computeQueueFamilyIndex;
	beforeFillBufferDrawIndirectMemoryBarrier.dstQueueFamilyIndex = m_computeQueueFamilyIndex;
	beforeFillBufferDrawIndirectMemoryBarrier.buffer = m_gpuDrawIndirectBuffer.handle;
	beforeFillBufferDrawIndirectMemoryBarrier.offset = sizeof(uint32_t);
	beforeFillBufferDrawIndirectMemoryBarrier.size = 65536 - sizeof(uint32_t);

	std::array<VkBufferMemoryBarrier2, 2> beforeFillBufferMemoryBarriers = { beforeFillBufferDrawCountMemoryBarrier, beforeFillBufferDrawIndirectMemoryBarrier };
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

	vkCmdFillBuffer(commandBuffer, m_gpuDrawIndirectBuffer.handle, 0, sizeof(uint32_t), 0);

	VkBufferMemoryBarrier2 beforeDispatchDrawIndirectBufferMemoryBarrier = {};
	beforeDispatchDrawIndirectBufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	beforeDispatchDrawIndirectBufferMemoryBarrier.pNext = nullptr;
	beforeDispatchDrawIndirectBufferMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
	beforeDispatchDrawIndirectBufferMemoryBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
	beforeDispatchDrawIndirectBufferMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	beforeDispatchDrawIndirectBufferMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT | VK_ACCESS_2_SHADER_WRITE_BIT;
	beforeDispatchDrawIndirectBufferMemoryBarrier.srcQueueFamilyIndex = m_computeQueueFamilyIndex;
	beforeDispatchDrawIndirectBufferMemoryBarrier.dstQueueFamilyIndex = m_computeQueueFamilyIndex;
	beforeDispatchDrawIndirectBufferMemoryBarrier.buffer = m_gpuDrawIndirectBuffer.handle;
	beforeDispatchDrawIndirectBufferMemoryBarrier.offset = 0;
	beforeDispatchDrawIndirectBufferMemoryBarrier.size = sizeof(uint32_t);

	VkBufferMemoryBarrier2 beforeDispatchPerDrawBufferMemoryBarrier = {};
	beforeDispatchPerDrawBufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	beforeDispatchPerDrawBufferMemoryBarrier.pNext = nullptr;
	beforeDispatchPerDrawBufferMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
	beforeDispatchPerDrawBufferMemoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
	beforeDispatchPerDrawBufferMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	beforeDispatchPerDrawBufferMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	beforeDispatchPerDrawBufferMemoryBarrier.srcQueueFamilyIndex = m_computeQueueFamilyIndex;
	beforeDispatchPerDrawBufferMemoryBarrier.dstQueueFamilyIndex = m_computeQueueFamilyIndex;
	beforeDispatchPerDrawBufferMemoryBarrier.buffer = m_gpuPerDrawBuffer.handle;
	beforeDispatchPerDrawBufferMemoryBarrier.offset = 0;
	beforeDispatchPerDrawBufferMemoryBarrier.size = 32768;

	std::array<VkBufferMemoryBarrier2, 2> beforeDispatchBufferMemoryBarriers = { beforeDispatchDrawIndirectBufferMemoryBarrier, beforeDispatchPerDrawBufferMemoryBarrier };
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

	vkCmdDispatch(commandBuffer, ((drawIndirectCount + 64 - 1) / 64), 1, 1);

	VkBufferMemoryBarrier2 drawIndirectBufferMemoryBarrier = {};
	drawIndirectBufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	drawIndirectBufferMemoryBarrier.pNext = nullptr;
	drawIndirectBufferMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	drawIndirectBufferMemoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT | VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	drawIndirectBufferMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
	drawIndirectBufferMemoryBarrier.dstAccessMask = VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
	drawIndirectBufferMemoryBarrier.srcQueueFamilyIndex = m_computeQueueFamilyIndex;
	drawIndirectBufferMemoryBarrier.dstQueueFamilyIndex = m_computeQueueFamilyIndex;
	drawIndirectBufferMemoryBarrier.buffer = m_gpuDrawIndirectBuffer.handle;
	drawIndirectBufferMemoryBarrier.offset = 0;
	drawIndirectBufferMemoryBarrier.size = 65536;

	VkBufferMemoryBarrier2 perDrawBufferMemoryBarrier = {};
	perDrawBufferMemoryBarrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
	perDrawBufferMemoryBarrier.pNext = nullptr;
	perDrawBufferMemoryBarrier.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
	perDrawBufferMemoryBarrier.srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT;
	perDrawBufferMemoryBarrier.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
	perDrawBufferMemoryBarrier.dstAccessMask = VK_ACCESS_2_SHADER_STORAGE_READ_BIT;
	perDrawBufferMemoryBarrier.srcQueueFamilyIndex = m_computeQueueFamilyIndex;
	perDrawBufferMemoryBarrier.dstQueueFamilyIndex = m_computeQueueFamilyIndex;
	perDrawBufferMemoryBarrier.buffer = m_gpuPerDrawBuffer.handle;
	perDrawBufferMemoryBarrier.offset = 0;
	perDrawBufferMemoryBarrier.size = 32768;

	std::array<VkBufferMemoryBarrier2, 2> indirectDrawBufferMemoryBarriers = { drawIndirectBufferMemoryBarrier, perDrawBufferMemoryBarrier };
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
#endif

	return drawIndirectCount;
}

VulkanBuffer& FrustumCulling::getDrawIndirectBuffer(uint32_t frameInFlight) {
#if FRUSTUM_CULLING_TYPE == FRUSTUM_CULLING_GPU
	NTSHENGN_UNUSED(frameInFlight);

	return m_gpuDrawIndirectBuffer;
#else
	return m_drawIndirectBuffers[frameInFlight];
#endif
}

std::vector<VkBuffer> FrustumCulling::getPerDrawBuffers() {
#if FRUSTUM_CULLING_TYPE == FRUSTUM_CULLING_GPU
	const std::vector<VkBuffer> gpuPerDrawBuffers(m_framesInFlight, m_gpuPerDrawBuffer.handle);

	return gpuPerDrawBuffers;
#else
	std::vector<VkBuffer> perDrawBuffer;
	for (size_t i = 0; i < m_perDrawBuffers.size(); i++) {
		perDrawBuffer.push_back(m_perDrawBuffers[i].handle);
	}

	return perDrawBuffer;
#endif
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

	// Create draw indirect buffers
	m_drawIndirectBuffers.resize(m_framesInFlight);
	bufferCreateInfo.size = 65536;
	bufferCreateInfo.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &bufferCreateInfo, &bufferAllocationCreateInfo, &m_drawIndirectBuffers[i].handle, &m_drawIndirectBuffers[i].allocation, &allocationInfo));
		m_drawIndirectBuffers[i].address = allocationInfo.pMappedData;
	}

	// Create per draw buffers
	m_perDrawBuffers.resize(m_framesInFlight);
	bufferCreateInfo.size = 32768;
	bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &bufferCreateInfo, &bufferAllocationCreateInfo, &m_perDrawBuffers[i].handle, &m_perDrawBuffers[i].allocation, &allocationInfo));
		m_perDrawBuffers[i].address = allocationInfo.pMappedData;
	}

#if FRUSTUM_CULLING_TYPE == FRUSTUM_CULLING_GPU
	// Create frustum culling buffers
	m_gpuFrustumCullingBuffers.resize(m_framesInFlight);
	bufferCreateInfo.size = 262144;
	bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &bufferCreateInfo, &bufferAllocationCreateInfo, &m_gpuFrustumCullingBuffers[i].handle, &m_gpuFrustumCullingBuffers[i].allocation, &allocationInfo));
		m_gpuFrustumCullingBuffers[i].address = allocationInfo.pMappedData;
	}

	// Create GPU draw indirect buffers
	bufferCreateInfo.size = 65536;
	bufferCreateInfo.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;

	bufferAllocationCreateInfo.flags = 0;
	bufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &bufferCreateInfo, &bufferAllocationCreateInfo, &m_gpuDrawIndirectBuffer.handle, &m_gpuDrawIndirectBuffer.allocation, nullptr));

	// Create GPU per draw buffers
	bufferCreateInfo.size = 32768;
	bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

	bufferAllocationCreateInfo.flags = 0;
	bufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

	NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &bufferCreateInfo, &bufferAllocationCreateInfo, &m_gpuPerDrawBuffer.handle, &m_gpuPerDrawBuffer.allocation, nullptr));
#endif
}

#if FRUSTUM_CULLING_TYPE == FRUSTUM_CULLING_GPU
void FrustumCulling::createDescriptorSetLayout() {
	VkDescriptorSetLayoutBinding frustumCullingDescriptorSetLayoutBinding = {};
	frustumCullingDescriptorSetLayoutBinding.binding = 0;
	frustumCullingDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	frustumCullingDescriptorSetLayoutBinding.descriptorCount = 1;
	frustumCullingDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	frustumCullingDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding inDrawIndirectDescriptorSetLayoutBinding = {};
	inDrawIndirectDescriptorSetLayoutBinding.binding = 1;
	inDrawIndirectDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	inDrawIndirectDescriptorSetLayoutBinding.descriptorCount = 1;
	inDrawIndirectDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	inDrawIndirectDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding outDrawIndirectDescriptorSetLayoutBinding = {};
	outDrawIndirectDescriptorSetLayoutBinding.binding = 2;
	outDrawIndirectDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	outDrawIndirectDescriptorSetLayoutBinding.descriptorCount = 1;
	outDrawIndirectDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	outDrawIndirectDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding inPerDrawDescriptorSetLayoutBinding = {};
	inPerDrawDescriptorSetLayoutBinding.binding = 3;
	inPerDrawDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	inPerDrawDescriptorSetLayoutBinding.descriptorCount = 1;
	inPerDrawDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	inPerDrawDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	VkDescriptorSetLayoutBinding outPerDrawDescriptorSetLayoutBinding = {};
	outPerDrawDescriptorSetLayoutBinding.binding = 4;
	outPerDrawDescriptorSetLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	outPerDrawDescriptorSetLayoutBinding.descriptorCount = 1;
	outPerDrawDescriptorSetLayoutBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
	outPerDrawDescriptorSetLayoutBinding.pImmutableSamplers = nullptr;

	std::array<VkDescriptorSetLayoutBinding, 5> descriptorSetLayoutBindings = { frustumCullingDescriptorSetLayoutBinding, inDrawIndirectDescriptorSetLayoutBinding, outDrawIndirectDescriptorSetLayoutBinding, inPerDrawDescriptorSetLayoutBinding, outPerDrawDescriptorSetLayoutBinding };
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCreateInfo = {};
	descriptorSetLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorSetLayoutCreateInfo.pNext = nullptr;
	descriptorSetLayoutCreateInfo.flags = 0;
	descriptorSetLayoutCreateInfo.bindingCount = static_cast<uint32_t>(descriptorSetLayoutBindings.size());
	descriptorSetLayoutCreateInfo.pBindings = descriptorSetLayoutBindings.data();
	NTSHENGN_VK_CHECK(vkCreateDescriptorSetLayout(m_device, &descriptorSetLayoutCreateInfo, nullptr, &m_descriptorSetLayout));
}

void FrustumCulling::createComputePipeline() {
	const std::string computeShaderCode = R"GLSL(
        #version 460

		layout(local_size_x = 64) in;

		struct ObjectInfo {
			vec3 position;
			mat4 rotation;
			vec3 scale;
			vec3 aabbMin;
			vec3 aabbMax;
		};

		layout(set = 0, binding = 0) restrict readonly buffer FrustumCulling {
			vec4 frustum[6];
			ObjectInfo objectInfo[];
		} frustumCulling;

		struct DrawIndirect {
			uint indexCount;
			uint instanceCount;
			uint firstIndex;
			int vertexOffset;
			uint firstInstance;
		};

		layout(set = 0, binding = 1) restrict readonly buffer InDrawIndirect {
			uint drawCount;
			DrawIndirect commands[];
		} inDrawIndirect;

		layout(set = 0, binding = 2) buffer OutDrawIndirect {
			uint drawCount;
			DrawIndirect commands[];
		} outDrawIndirect;

		struct PerDrawInfo {
			uint objectID;
		};

		layout(set = 0, binding = 3) restrict readonly buffer InPerDraw {
			PerDrawInfo info[];
		} inPerDraw;

		layout(set = 0, binding = 4) restrict writeonly buffer OutPerDraw {
			PerDrawInfo info[];
		} outPerDraw;

		bool intersect(vec3 aabbMin, vec3 aabbMax) {
			const vec3 mmm = vec3(aabbMin.x, aabbMin.y, aabbMin.z);
			const vec3 Mmm = vec3(aabbMax.x, aabbMin.y, aabbMin.z);
			const vec3 mMm = vec3(aabbMin.x, aabbMax.y, aabbMin.z);
			const vec3 MMm = vec3(aabbMax.x, aabbMax.y, aabbMin.z);
			const vec3 mmM = vec3(aabbMin.x, aabbMin.y, aabbMax.z);
			const vec3 MmM = vec3(aabbMax.x, aabbMin.y, aabbMax.z);
			const vec3 mMM = vec3(aabbMin.x, aabbMax.y, aabbMax.z);
			const vec3 MMM = vec3(aabbMax.x, aabbMax.y, aabbMax.z);
			for (uint i = 0; i < 6; i++) {
				if (((dot(frustumCulling.frustum[i].xyz, mmm) + frustumCulling.frustum[i].w) <= 0.0f)
					&& ((dot(frustumCulling.frustum[i].xyz, Mmm) + frustumCulling.frustum[i].w) <= 0.0f)
					&& ((dot(frustumCulling.frustum[i].xyz, mMm) + frustumCulling.frustum[i].w) <= 0.0f)
					&& ((dot(frustumCulling.frustum[i].xyz, MMm) + frustumCulling.frustum[i].w) <= 0.0f)
					&& ((dot(frustumCulling.frustum[i].xyz, mmM) + frustumCulling.frustum[i].w) <= 0.0f)
					&& ((dot(frustumCulling.frustum[i].xyz, MmM) + frustumCulling.frustum[i].w) <= 0.0f)
					&& ((dot(frustumCulling.frustum[i].xyz, mMM) + frustumCulling.frustum[i].w) <= 0.0f)
					&& ((dot(frustumCulling.frustum[i].xyz, MMM) + frustumCulling.frustum[i].w) <= 0.0f)) {
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

			vec3 aabbMin = frustumCulling.objectInfo[objectIndex].aabbMin;
			vec3 aabbMax = frustumCulling.objectInfo[objectIndex].aabbMax;

			vec3 newAABBMin = frustumCulling.objectInfo[objectIndex].position;
			vec3 newAABBMax = frustumCulling.objectInfo[objectIndex].position;

			float a;
			float b;

			for (uint i = 0; i < 3; i++) {
				for (uint j = 0; j < 3; j++) {
					a = frustumCulling.objectInfo[objectIndex].rotation[j][i] * aabbMin[j] * abs(frustumCulling.objectInfo[objectIndex].scale[j]);
					b = frustumCulling.objectInfo[objectIndex].rotation[j][i] * aabbMax[j] * abs(frustumCulling.objectInfo[objectIndex].scale[j]);

					newAABBMin[i] += (a < b) ? a : b;
					newAABBMax[i] += (a < b) ? b : a;
				}
			}

			aabbMin = newAABBMin;
			aabbMax = newAABBMax;

			if (intersect(aabbMin, aabbMax)) {
				uint drawIndex = atomicAdd(outDrawIndirect.drawCount, 1);

				outDrawIndirect.commands[drawIndex] = inDrawIndirect.commands[objectIndex];
				outPerDraw.info[drawIndex] = inPerDraw.info[objectIndex];
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
	VkDescriptorPoolSize frustumCullingDescriptorPoolSize = {};
	frustumCullingDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	frustumCullingDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize inDrawIndirectDescriptorPoolSize = {};
	inDrawIndirectDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	inDrawIndirectDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize outDrawIndirectDescriptorPoolSize = {};
	outDrawIndirectDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	outDrawIndirectDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize inPerDrawDescriptorPoolSize = {};
	inPerDrawDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	inPerDrawDescriptorPoolSize.descriptorCount = m_framesInFlight;

	VkDescriptorPoolSize outPerDrawDescriptorPoolSize = {};
	outPerDrawDescriptorPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
	outPerDrawDescriptorPoolSize.descriptorCount = m_framesInFlight;

	std::array<VkDescriptorPoolSize, 5> descriptorPoolSizes = { frustumCullingDescriptorPoolSize, inDrawIndirectDescriptorPoolSize, outDrawIndirectDescriptorPoolSize, inPerDrawDescriptorPoolSize, outPerDrawDescriptorPoolSize };
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
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = {};
		descriptorSetAllocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptorSetAllocateInfo.pNext = nullptr;
		descriptorSetAllocateInfo.descriptorPool = m_descriptorPool;
		descriptorSetAllocateInfo.descriptorSetCount = 1;
		descriptorSetAllocateInfo.pSetLayouts = &m_descriptorSetLayout;
		NTSHENGN_VK_CHECK(vkAllocateDescriptorSets(m_device, &descriptorSetAllocateInfo, &m_descriptorSets[i]));
	}

	// Update descriptor sets
	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		std::vector<VkWriteDescriptorSet> writeDescriptorSets;

		VkDescriptorBufferInfo frustumCullingDescriptorBufferInfo;
		frustumCullingDescriptorBufferInfo.buffer = m_gpuFrustumCullingBuffers[i].handle;
		frustumCullingDescriptorBufferInfo.offset = 0;
		frustumCullingDescriptorBufferInfo.range = 262144;

		VkWriteDescriptorSet frustumCullingDescriptorWriteDescriptorSet = {};
		frustumCullingDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		frustumCullingDescriptorWriteDescriptorSet.pNext = nullptr;
		frustumCullingDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		frustumCullingDescriptorWriteDescriptorSet.dstBinding = 0;
		frustumCullingDescriptorWriteDescriptorSet.dstArrayElement = 0;
		frustumCullingDescriptorWriteDescriptorSet.descriptorCount = 1;
		frustumCullingDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		frustumCullingDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		frustumCullingDescriptorWriteDescriptorSet.pBufferInfo = &frustumCullingDescriptorBufferInfo;
		frustumCullingDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;
		writeDescriptorSets.push_back(frustumCullingDescriptorWriteDescriptorSet);

		VkDescriptorBufferInfo inDrawIndirectDescriptorBufferInfo;
		inDrawIndirectDescriptorBufferInfo.buffer = m_drawIndirectBuffers[i].handle;
		inDrawIndirectDescriptorBufferInfo.offset = 0;
		inDrawIndirectDescriptorBufferInfo.range = 65536;

		VkWriteDescriptorSet inDrawIndirectDescriptorWriteDescriptorSet = {};
		inDrawIndirectDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		inDrawIndirectDescriptorWriteDescriptorSet.pNext = nullptr;
		inDrawIndirectDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		inDrawIndirectDescriptorWriteDescriptorSet.dstBinding = 1;
		inDrawIndirectDescriptorWriteDescriptorSet.dstArrayElement = 0;
		inDrawIndirectDescriptorWriteDescriptorSet.descriptorCount = 1;
		inDrawIndirectDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		inDrawIndirectDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		inDrawIndirectDescriptorWriteDescriptorSet.pBufferInfo = &inDrawIndirectDescriptorBufferInfo;
		inDrawIndirectDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;
		writeDescriptorSets.push_back(inDrawIndirectDescriptorWriteDescriptorSet);

		VkDescriptorBufferInfo outDrawIndirectDescriptorBufferInfo;
		outDrawIndirectDescriptorBufferInfo.buffer = m_gpuDrawIndirectBuffer.handle;
		outDrawIndirectDescriptorBufferInfo.offset = 0;
		outDrawIndirectDescriptorBufferInfo.range = 65536;

		VkWriteDescriptorSet outDrawIndirectDescriptorWriteDescriptorSet = {};
		outDrawIndirectDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		outDrawIndirectDescriptorWriteDescriptorSet.pNext = nullptr;
		outDrawIndirectDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		outDrawIndirectDescriptorWriteDescriptorSet.dstBinding = 2;
		outDrawIndirectDescriptorWriteDescriptorSet.dstArrayElement = 0;
		outDrawIndirectDescriptorWriteDescriptorSet.descriptorCount = 1;
		outDrawIndirectDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		outDrawIndirectDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		outDrawIndirectDescriptorWriteDescriptorSet.pBufferInfo = &outDrawIndirectDescriptorBufferInfo;
		outDrawIndirectDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;
		writeDescriptorSets.push_back(outDrawIndirectDescriptorWriteDescriptorSet);

		VkDescriptorBufferInfo inPerDrawDescriptorBufferInfo;
		inPerDrawDescriptorBufferInfo.buffer = m_perDrawBuffers[i].handle;
		inPerDrawDescriptorBufferInfo.offset = 0;
		inPerDrawDescriptorBufferInfo.range = 32768;

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

		VkDescriptorBufferInfo outPerDrawDescriptorBufferInfo;
		outPerDrawDescriptorBufferInfo.buffer = m_gpuPerDrawBuffer.handle;
		outPerDrawDescriptorBufferInfo.offset = 0;
		outPerDrawDescriptorBufferInfo.range = 32768;

		VkWriteDescriptorSet outPerDrawDescriptorWriteDescriptorSet = {};
		outPerDrawDescriptorWriteDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		outPerDrawDescriptorWriteDescriptorSet.pNext = nullptr;
		outPerDrawDescriptorWriteDescriptorSet.dstSet = m_descriptorSets[i];
		outPerDrawDescriptorWriteDescriptorSet.dstBinding = 4;
		outPerDrawDescriptorWriteDescriptorSet.dstArrayElement = 0;
		outPerDrawDescriptorWriteDescriptorSet.descriptorCount = 1;
		outPerDrawDescriptorWriteDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		outPerDrawDescriptorWriteDescriptorSet.pImageInfo = nullptr;
		outPerDrawDescriptorWriteDescriptorSet.pBufferInfo = &outPerDrawDescriptorBufferInfo;
		outPerDrawDescriptorWriteDescriptorSet.pTexelBufferView = nullptr;
		writeDescriptorSets.push_back(outPerDrawDescriptorWriteDescriptorSet);

		vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, nullptr);
	}
}
#endif
