/*
 * Sogang Univ, Graphics Lab, 2024
 *
 * Abura Soba, 2025
 */

#include "ExclusiveScan.hpp"
#include "Define.h"

#include <vector>
#include <algorithm>

using namespace std;

// value 1024 should change if local group size change
ExclusiveScan::ExclusiveScan(vks::VulkanDevice& device, VkQueue& queue, int swapchainImageCnt, string projectPath) : vulkanDevice(device), device(device.logicalDevice), queue(queue) {
	this->swapchainImageCnt = swapchainImageCnt;
	this->projectPath = projectPath;
	this->bufferSize = 0;
	descriptorSets.resize(swapchainImageCnt);
	descriptorSets2.resize(swapchainImageCnt);
	partialSums.resize(swapchainImageCnt);

	uint32_t sharedDataSize = min((uint32_t)1024, (uint32_t)(vulkanDevice.properties.limits.maxComputeSharedMemorySize / sizeof(glm::vec4)));
	assert(sharedDataSize >= 1024);

	//check subgroup properties
	VkPhysicalDeviceSubgroupProperties subgroupProperties{};
	subgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
	subgroupProperties.pNext = NULL;
	VkPhysicalDeviceProperties2 physicalDeviceProperties;
	physicalDeviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	physicalDeviceProperties.pNext = &subgroupProperties;
	vkGetPhysicalDeviceProperties2(vulkanDevice.physicalDevice, &physicalDeviceProperties);

	subgroupSize = subgroupProperties.subgroupSize;
	assert(subgroupProperties.supportedOperations && VK_SUBGROUP_FEATURE_ARITHMETIC_BIT);
}

ExclusiveScan::~ExclusiveScan() {
	vkDestroyPipeline(device, pipeline1, nullptr);
	vkDestroyPipeline(device, pipeline2, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout2, nullptr);
	vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, descriptorSetLayout2, nullptr);
	if (descriptorPool1 != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(device, descriptorPool1, nullptr);
	}
	if (descriptorPool1 != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(device, descriptorPool2, nullptr);
	}

	for (auto& shaderModule : shaderModules) {
		vkDestroyShaderModule(device, shaderModule, nullptr);
	}
}

void ExclusiveScan::createDescriptorSets1(vector<vks::Buffer>& inputBuffers, vector<vks::Buffer>& outputBuffers) {
	// create descriptor pool
	vector<VkDescriptorPoolSize> poolSizes = {
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 * swapchainImageCnt)	// input, output
	};
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, swapchainImageCnt);

	VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &descriptorPool1));

	// create descriptor set layout Binding
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
		// Binding 0: input buffer
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
		// Binding 1: output scan buffer
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1),
		// Binding 2: partial sum buffer
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 2),
	};
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayout));

	// create descriptor sets
	for (int i = 0; i < swapchainImageCnt; i++) {
		VkDescriptorSet& descriptorSet = descriptorSets[i];
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool1, &descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet));

		vector<VkWriteDescriptorSet> writeDescriptorSets = {
			// Binding 0: input buffer
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, &inputBuffers[i].descriptor),
			// Binding 1: output scan buffer
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &outputBuffers[i].descriptor),
			// Binding 2: partial sum buffer
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2, &partialSums[i].descriptor),
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, VK_NULL_HANDLE);
	}
}

void ExclusiveScan::createDescriptorSets2(vector<vks::Buffer>& outputBuffers) {
	// create descriptor pool
	vector<VkDescriptorPoolSize> poolSizes = {
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 * swapchainImageCnt)	// input, output
	};
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, swapchainImageCnt);

	VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &descriptorPool2));

	// create descriptor set layout Binding
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
		// Binding 0: partial sum buffer
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
		// Binding 1: output scan buffer
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1),
	};
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayout2));

	// create descriptor sets
	for (int i = 0; i < swapchainImageCnt; i++) {
		VkDescriptorSet& descriptorSet = descriptorSets2[i];
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool2, &descriptorSetLayout2, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet));

		vector<VkWriteDescriptorSet> writeDescriptorSets = {
			// Binding 0: partial sum buffer
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, &partialSums[i].descriptor),
			// Binding 1: output scan buffer
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &outputBuffers[i].descriptor),
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, VK_NULL_HANDLE);
	}
}

void ExclusiveScan::createDescriptorSets(vector<vks::Buffer>& inputBuffers, vector<vks::Buffer>& outputBuffers) {
	createPartialSumBuffers(inputBuffers[0].size);
	createDescriptorSets1(inputBuffers, outputBuffers);
	createDescriptorSets2(outputBuffers);
}

void ExclusiveScan::createPipeline1() {
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
	VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

	VkComputePipelineCreateInfo computePipelineCreateInfo = vks::initializers::computePipelineCreateInfo(pipelineLayout);

	//load shader
	VkPipelineShaderStageCreateInfo shaderStage = vks::initializers::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT);
	string shaderPath = "./../shaders/glsl/" + projectPath + "exclusiveScan.comp.spv";
	shaderStage.module = vks::tools::loadShader(shaderPath.c_str(), device);
	assert(shaderStage.module != VK_NULL_HANDLE);
	computePipelineCreateInfo.stage = shaderStage;
	shaderModules.push_back(shaderStage.module);
	VK_CHECK_RESULT(vkCreateComputePipelines(device, nullptr, 1, &computePipelineCreateInfo, nullptr, &pipeline1));
}

void ExclusiveScan::createPipeline2() {
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout2, 1);
	VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout2));

	VkComputePipelineCreateInfo computePipelineCreateInfo = vks::initializers::computePipelineCreateInfo(pipelineLayout2);

	//load shader
	VkPipelineShaderStageCreateInfo shaderStage = vks::initializers::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT);
	string shaderPath = "./../shaders/glsl/" + projectPath + "exclusiveScan2.comp.spv";
	shaderStage.module = vks::tools::loadShader(shaderPath.c_str(), device);
	assert(shaderStage.module != VK_NULL_HANDLE);
	computePipelineCreateInfo.stage = shaderStage;
	shaderModules.push_back(shaderStage.module);
	VK_CHECK_RESULT(vkCreateComputePipelines(device, nullptr, 1, &computePipelineCreateInfo, nullptr, &pipeline2));
}

void ExclusiveScan::createPipelines() {
	createPipeline1();
	createPipeline2();
}

void ExclusiveScan::createPartialSumBuffers(uint32_t bufferSize) {
	for (int i = 0; i < partialSums.size(); i++) {
		if (partialSums[i].size != 0) {
			partialSums[i].destroy();
		}
		VK_CHECK_RESULT(vulkanDevice.createBuffer(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &partialSums[i], bufferSize * sizeof(uint32_t)));
		VK_CHECK_RESULT(vulkanDevice.createBuffer(VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &partialSums[i], bufferSize * sizeof(uint32_t)));
	}
	this->bufferSize = bufferSize;
}

void ExclusiveScan::prepare(vector<vks::Buffer>& inputBuffers, vector<vks::Buffer>& outputBuffers) {
	createDescriptorSets(inputBuffers, outputBuffers);
	createPipelines();
}

//value groupSize should change if local group size change
void ExclusiveScan::buildCommandBuffer(VkCommandBuffer commandBuffer, VulkanSwapChain& swapChain, uint32_t imageIndex, uint32_t bufferSize) {
	/*if (bufferSize != this->bufferSize) {
		createPartialSumBuffers(bufferSize);
	}*/

	const uint32_t groupSizeX = 1024;
	uint32_t groupCntX;

	VkMemoryBarrier barrier = vks::initializers::memoryBarrier();
	barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_FLAGS_NONE, 1, &barrier, 0, nullptr, 0, nullptr);

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSets[imageIndex], 0, 0);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline1);
	groupCntX = (bufferSize + groupSizeX - 1) / groupSizeX;
	vkCmdDispatch(commandBuffer, groupCntX, 1, 1);

	barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_FLAGS_NONE, 1, &barrier, 0, nullptr, 0, nullptr);

	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout2, 0, 1, &descriptorSets2[imageIndex], 0, 0);
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline2);
	vkCmdDispatch(commandBuffer, groupCntX, 1, 1);
}