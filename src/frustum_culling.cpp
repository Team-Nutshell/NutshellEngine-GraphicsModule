#include "frustum_culling.h"
#include <array>
#include <mutex>

void FrustumCulling::init(VkDevice device,
	VkQueue computeQueue,
	uint32_t computeQueueFamilyIndex,
	VmaAllocator allocator,
	VkFence initializationFence,
	uint32_t framesInFlight,
	PFN_vkCmdPipelineBarrier2KHR vkCmdPipelineBarrier2KHR, 
	NtshEngn::JobSystem* jobSystem,
	NtshEngn::ECS* ecs) {
	m_device = device;
	m_computeQueue = computeQueue;
	m_computeQueueFamilyIndex = computeQueueFamilyIndex;
	m_allocator = allocator;
	m_initializationFence = initializationFence;
	m_framesInFlight = framesInFlight;
	m_vkCmdPipelineBarrier2KHR = vkCmdPipelineBarrier2KHR;
	m_jobSystem = jobSystem;
	m_ecs = ecs;
	
	createBuffers();
	/*createDescriptorSetLayout();
	createComputePipeline();*/
}

void FrustumCulling::destroy() {
	/*vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);

	vkDestroyPipeline(m_device, m_computePipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_computePipelineLayout, nullptr);

	vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);*/

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		m_perDrawBuffers[i].destroy(m_allocator);
	}

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		m_drawIndirectBuffers[i].destroy(m_allocator);
	}
}

uint32_t FrustumCulling::culling(uint32_t currentFrameInFlight,
	const NtshEngn::Math::mat4& cameraView,
	const NtshEngn::Math::mat4& cameraProjection,
	const std::set<NtshEngn::Entity>& entities,
	const std::unordered_map<NtshEngn::Entity, InternalObject>& objects,
	const std::vector<InternalMesh>& meshes) {
#if (FRUSTUM_CULLING_TYPE == FRUSTUM_CULLING_CPU_SINGLETHREADED) || (FRUSTUM_CULLING_TYPE == FRUSTUM_CULLING_CPU_MULTITHREADED)
	std::vector<VkDrawIndexedIndirectCommand> drawIndirectCommands;
	std::vector<uint32_t> perDraw;

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

	m_jobSystem->dispatch(static_cast<uint32_t>(entities.size()), (static_cast<uint32_t>(entities.size()) / m_jobSystem->getNumThreads()) + 1, [this, &drawIndirectCommands, &perDraw, &mutex, &frustum, &entities, &objects, &meshes](NtshEngn::JobDispatchArguments args) {
		std::set<NtshEngn::Entity>::iterator it = entities.begin();
		std::advance(it, args.jobIndex);

		NtshEngn::Entity entity = *it;

		if (!m_ecs->hasComponent<NtshEngn::Renderable>(entity)) {
			return;
		}

		const InternalObject& object = objects.at(entity);
		const InternalMesh& mesh = meshes[object.meshID];

		NtshEngn::Math::vec3 aabbMin = mesh.aabbMin;
		NtshEngn::Math::vec3 aabbMax = mesh.aabbMax;

		const NtshEngn::Transform& entityTransform = m_ecs->getComponent<NtshEngn::Transform>(entity);

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
	for (NtshEngn::Entity entity : entities) {
		if (!m_ecs->hasComponent<NtshEngn::Renderable>(entity)) {
			continue;
		}

		const InternalObject& object = objects.at(entity);
		const InternalMesh& mesh = meshes[object.meshID];

		NtshEngn::Math::vec3 aabbMin = mesh.aabbMin;
		NtshEngn::Math::vec3 aabbMax = mesh.aabbMax;

		const NtshEngn::Transform& entityTransform = m_ecs->getComponent<NtshEngn::Transform>(entity);

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
	NTSHENGN_UNUSED(cameraView);
	NTSHENGN_UNUSED(cameraProjection);
	NTSHENGN_UNUSED(entities);

	std::vector<VkDrawIndexedIndirectCommand> drawIndirectCommands;
	std::vector<uint32_t> perDraw;
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
	}
#endif

	void* data;
	NTSHENGN_VK_CHECK(vmaMapMemory(m_allocator, m_drawIndirectBuffers[currentFrameInFlight].allocation, &data));
	uint32_t drawIndirectCount = static_cast<uint32_t>(drawIndirectCommands.size());
	memcpy(data, &drawIndirectCount, sizeof(uint32_t));
	memcpy(reinterpret_cast<char*>(data) + sizeof(uint32_t), drawIndirectCommands.data(), sizeof(VkDrawIndexedIndirectCommand)* drawIndirectCommands.size());
	vmaUnmapMemory(m_allocator, m_drawIndirectBuffers[currentFrameInFlight].allocation);

	NTSHENGN_VK_CHECK(vmaMapMemory(m_allocator, m_perDrawBuffers[currentFrameInFlight].allocation, &data));
	memcpy(data, perDraw.data(), sizeof(uint32_t)* perDraw.size());
	vmaUnmapMemory(m_allocator, m_perDrawBuffers[currentFrameInFlight].allocation);

	return drawIndirectCount;
}

VulkanBuffer& FrustumCulling::getDrawIndirectBuffer(uint32_t frameInFlight) {
	return m_drawIndirectBuffers[frameInFlight];
}

std::vector<VulkanBuffer> FrustumCulling::getPerDrawBuffers() {
	return m_perDrawBuffers;
}

void FrustumCulling::createBuffers() {
	// Create draw indirect buffers
	m_drawIndirectBuffers.resize(m_framesInFlight);
	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.pNext = nullptr;
	bufferCreateInfo.flags = 0;
	bufferCreateInfo.size = 65536;
	bufferCreateInfo.usage = VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	bufferCreateInfo.queueFamilyIndexCount = 1;
	bufferCreateInfo.pQueueFamilyIndices = &m_computeQueueFamilyIndex;

	VmaAllocationCreateInfo bufferAllocationCreateInfo = {};
	bufferAllocationCreateInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
	bufferAllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_HOST;

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &bufferCreateInfo, &bufferAllocationCreateInfo, &m_drawIndirectBuffers[i].handle, &m_drawIndirectBuffers[i].allocation, nullptr));
	}

	// Create per draw buffers
	m_perDrawBuffers.resize(m_framesInFlight);
	bufferCreateInfo.size = 32768;
	bufferCreateInfo.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

	for (uint32_t i = 0; i < m_framesInFlight; i++) {
		NTSHENGN_VK_CHECK(vmaCreateBuffer(m_allocator, &bufferCreateInfo, &bufferAllocationCreateInfo, &m_perDrawBuffers[i].handle, &m_perDrawBuffers[i].allocation, nullptr));
	}
}

void FrustumCulling::createDescriptorSetLayout() {

}

void FrustumCulling::createComputePipeline() {
	const std::string computeShaderCode = R"GLSL(
        #version 460
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