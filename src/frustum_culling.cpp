#include "frustum_culling.h"

void FrustumCulling::init(VkDevice device,
	VkQueue computeQueue,
	uint32_t computeQueueFamilyIndex,
	VmaAllocator allocator,
	VkFence initializationFence,
	uint32_t framesInFlight,
	PFN_vkCmdPipelineBarrier2KHR vkCmdPipelineBarrier2KHR) {
	m_device = device;
	m_computeQueue = computeQueue;
	m_computeQueueFamilyIndex = computeQueueFamilyIndex;
	m_allocator = allocator;
	m_initializationFence = initializationFence;
	m_framesInFlight = framesInFlight;
	m_vkCmdPipelineBarrier2KHR = vkCmdPipelineBarrier2KHR;

	createComputePipeline();
}

void FrustumCulling::destroy() {
	vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);

	vkDestroyPipeline(m_device, m_computePipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_computePipelineLayout, nullptr);

	vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
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
