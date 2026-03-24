/*
 * Sogang Univ, Graphics Lab, 2024
 * 
 * Abura Soba, 2025
 */

#include "Define.h"

#include "KdTreePipeline.hpp"

using namespace std;

KdTreePipeline::KdTreePipeline(vks::VulkanDevice& device, VkQueue& queue, int swapchainImageCnt, string projectPath) : vulkanDevice(device), device(device.logicalDevice), queue(queue){
	this->swapchainImageCnt = swapchainImageCnt;
	this->projectPath = projectPath;
	descriptorSets.resize(swapchainImageCnt);

	//TODO
	uint32_t sharedDataSize = min((uint32_t)1024, (uint32_t)(vulkanDevice.properties.limits.maxComputeSharedMemorySize / sizeof(glm::vec4)));
	assert(sharedDataSize >= 1024);
}

KdTreePipeline::~KdTreePipeline() {
	vkDestroyPipeline(device, pipeline, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
	if (descriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(device, descriptorPool, nullptr);
	}

	for (auto& shaderModule : shaderModules) {
		vkDestroyShaderModule(device, shaderModule, nullptr);
	}
}

string KdTreePipeline::getShaderPath(string shaderName) {
	return vks::tools::getShadersPath() + projectPath + shaderName;
}

void KdTreePipeline::createBuffers(uint32_t width, uint32_t height) {
	//for (int i = 0; i < swapchainImageCnt; i++) {
	//	VK_CHECK_RESULT(vulkanDevice.createBuffer(
	//		VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
	//		VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
	//		&rtMaskBuffers[i],
	//		width * height * sizeof(uint32_t)));
	//}
}

void KdTreePipeline::createDescriptorSets(VulkanSwapChain& swapChain) {
	// create descriptor pool
	vector<VkDescriptorPoolSize> poolSizes = {
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 * swapchainImageCnt),	// output
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 * swapchainImageCnt),
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 7 * swapchainImageCnt) // 
	};
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, swapchainImageCnt);

	VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &descriptorPool));

	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
		// Binding 0: Ray tracing result image
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 0),
		// Binding 1: Uniform buffer Dynamic
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1),
		// Binding 2: Uniform buffer Static
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 2),
		// Binding 3: Storage buffer - Particle Densities
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 3),
		// Binding 4: Storage buffer - Particle Sph Coefficients
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 4),
		// Binding 5: Storage buffer - KdTree
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 5),
		// Binding 6: Storage buffer - Triangle Offset List
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 6),
		// Binding 7: Storage buffer - Vertex Buffer
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 7),
		// Binding 8: Storage buffer - Index Buffer
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 8),
		// Binding 9: Storage buffer - Triangle Acceleration List
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 9),
	};

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayout));

	for (int i = 0; i < swapChain.imageCount; i++) {
		VkDescriptorSet& descriptorSet = descriptorSets[i];
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet));
	}
}

void KdTreePipeline::createPipelineLayout() {
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
	//pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	//pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
	VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));
}

void KdTreePipeline::createPipeline() {
	VkComputePipelineCreateInfo computePipelineCreateInfo = vks::initializers::computePipelineCreateInfo(pipelineLayout);

	//load shader
	string shaderPath = getShaderPath("kdTreeRT.comp.spv");
	VkPipelineShaderStageCreateInfo shaderStage = vks::utils::createShaderStageCI(device, shaderPath, VK_SHADER_STAGE_COMPUTE_BIT);
	shaderModules.push_back(shaderStage.module);
	computePipelineCreateInfo.stage = shaderStage;
	VK_CHECK_RESULT(vkCreateComputePipelines(device, nullptr, 1, &computePipelineCreateInfo, nullptr, &pipeline));
}

void KdTreePipeline::prepare(VulkanSwapChain& swapChain, uint32_t width, uint32_t height) {
	createBuffers(width, height);
	createDescriptorSets(swapChain);
	pushConstantRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, 4);
	createPipelineLayout();
	createPipeline();
}

void KdTreePipeline::initDescriptorSet(int frameIdx, VulkanSwapChain& swapChain, vks::Buffer& uniformBuffer, vks::Buffer& uniformBufferStatic, vks::Buffer& particleDensities, vks::Buffer& particleSphCoefficients, KdTreeModel& kdTreeModel) {
	VkDescriptorSet& descriptorSet = descriptorSets[frameIdx];

	VkDescriptorImageInfo storageImageDescriptor = { VK_NULL_HANDLE, swapChain.buffers[frameIdx].view, VK_IMAGE_LAYOUT_GENERAL };

	int dIdx = 0;
	std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
		// Binding 0: Ray tracing result image
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, dIdx++, &storageImageDescriptor),
		// Binding 1: Uniform data Dynamic
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, dIdx++, &uniformBuffer.descriptor),
		// Binding 2: Uniform data Static
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, dIdx++, &uniformBufferStatic.descriptor),
		// Binding 3: Storage buffer - Particle Densities
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, dIdx++, &particleDensities.descriptor),
		// Binding 4: Storage buffer - Particle Sph Coefficients
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, dIdx++, &particleSphCoefficients.descriptor),
		// Binding 5: Storage buffer - KdTree
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, dIdx++, &kdTreeModel.d_kdTreeNode.descriptor),
		// Binding 6: Storage buffer - Triangle Offset List
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, dIdx++, &kdTreeModel.d_triOffsetList.descriptor),
		// Binding 7: Storage buffer - Vertex Buffer
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, dIdx++, &kdTreeModel.d_vntArray.descriptor),
		// Binding 8: Storage buffer - Index Buffer
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, dIdx++, &kdTreeModel.d_faceArray.descriptor),
		// Binding 9: Storage buffer - Triangle Acceleration List
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, dIdx++, &kdTreeModel.d_triAccList.descriptor),
	};

	vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, VK_NULL_HANDLE);
}


void KdTreePipeline::record(VkCommandBuffer& commandBuffer, uint32_t imageIndex, uint32_t width, uint32_t height) {
	const uint32_t groupSizeX = 32;
	const uint32_t groupSizeY = 32;
	uint32_t groupCntX;
	uint32_t groupCntY;

	//VkMemoryBarrier barrier = vks::initializers::memoryBarrier();
	//barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	//barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	//vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
	//	VK_FLAGS_NONE, 1, &barrier, 0, nullptr, 0, nullptr);

	groupCntX = (width + groupSizeX - 1) / groupSizeX;
	groupCntY = (height + groupSizeY - 1) / groupSizeY;
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSets[imageIndex], 0, 0);
	vkCmdDispatch(commandBuffer, groupCntX, groupCntY, 1);
}