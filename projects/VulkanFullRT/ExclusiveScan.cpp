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

ExclusiveScan::ExclusiveScan(vks::VulkanDevice& device, VkQueue& queue, int swapchainImageCnt, string projectPath) : vulkanDevice(device), device(device.logicalDevice), queue(queue) {
	this->swapchainImageCnt = swapchainImageCnt;
	this->projectPath = projectPath;
	localScanDescriptorSets.resize(swapchainImageCnt);
	addPartialSumDescriptorSets.resize(swapchainImageCnt);
	partialSums.resize(swapchainImageCnt);

	//check shared memory properties
	uint32_t sharedDataSize = min((uint32_t)SHARED_MEMORY_SIZE, (uint32_t)(vulkanDevice.properties.limits.maxComputeSharedMemorySize / sizeof(glm::vec4)));
	assert(sharedDataSize >= SHARED_MEMORY_SIZE);

	//check subgroup properties
	VkPhysicalDeviceSubgroupProperties subgroupProperties{};
	subgroupProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SUBGROUP_PROPERTIES;
	subgroupProperties.pNext = NULL;
	VkPhysicalDeviceProperties2 physicalDeviceProperties;
	physicalDeviceProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2;
	physicalDeviceProperties.pNext = &subgroupProperties;
	vkGetPhysicalDeviceProperties2(vulkanDevice.physicalDevice, &physicalDeviceProperties);

	subgroupSize = subgroupProperties.subgroupSize;
	assert(subgroupSize == 32);	// must change shader if subgroupSize is not 32
	assert(subgroupProperties.supportedOperations && VK_SUBGROUP_FEATURE_ARITHMETIC_BIT);
}

ExclusiveScan::~ExclusiveScan() {
	vkDestroyPipeline(device, localScanPipeline, nullptr);
	vkDestroyPipeline(device, addPartialSumPipeline, nullptr);
	vkDestroyPipelineLayout(device, localScanPipelineLayout, nullptr);
	vkDestroyPipelineLayout(device, addPartialSumPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, localScanDescriptorSetLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, addPartialSumDescriptorSetLayout, nullptr);
	if (localScanDescriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(device, localScanDescriptorPool, nullptr);
	}
	if (localScanDescriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(device, addPartialSumDescriptorPool, nullptr);
	}

	for (auto& shaderModule : shaderModules) {
		vkDestroyShaderModule(device, shaderModule, nullptr);
	}

	for (int i = 0; i < partialSums.size(); i++) {
		for (int j = 0; j < partialSums[i].size(); j++) {
			partialSums[i][j].destroy();
		}
	}
}

void ExclusiveScan::calcLevel(uint32_t n) {
	uint32_t currentSize = n;
	elementCnts.push_back(n);
	while (currentSize > THREADS) {
		uint32_t nextSize = (currentSize + THREADS - 1) / THREADS;
		currentSize = nextSize;
		workGroupSizes.push_back(currentSize);
		elementCnts.push_back(currentSize);
	}
	workGroupSizes.push_back(1);
	level = workGroupSizes.size();
}

void ExclusiveScan::createBuffers() {
	partialSums.resize(swapchainImageCnt);
	for (int i = 0; i < swapchainImageCnt; i++) {
		partialSums[i].resize(workGroupSizes.size());
		for (int j = 0; j < workGroupSizes.size(); j++) {
			string bufferName = "partialSum" + to_string(i) + "_" + to_string(j);
			VK_CHECK_RESULT(vulkanDevice.createBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
				VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &partialSums[i][j], workGroupSizes[j] * sizeof(uint32_t), nullptr, bufferName));
		}
	}
}

void ExclusiveScan::createLocalScanDescriptorSets(vector<vks::Buffer>& inputBuffers) {
	// create descriptor pool
	vector<VkDescriptorPoolSize> poolSizes = {
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 * swapchainImageCnt * level)	// input, output, partialsum
	};
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, swapchainImageCnt * level);

	VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &localScanDescriptorPool));

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
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &localScanDescriptorSetLayout));

	for (int i = 0; i < swapchainImageCnt; i++) {
		for (int j = 0; j < level; j++) {
			VkDescriptorSet& descriptorSet = localScanDescriptorSets[i][j];
			VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = vks::initializers::descriptorSetAllocateInfo(localScanDescriptorPool, &localScanDescriptorSetLayout, 1);
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet));

			vector<VkWriteDescriptorSet> writeDescriptorSets;
			if (j == 0) {
				writeDescriptorSets = {
					// Binding 0: input buffer
					vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, &inputBuffers[i].descriptor),
					// Binding 1: output scan buffer
					vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &inputBuffers[i].descriptor),
					// Binding 2: partial sum buffer
					vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2, &partialSums[i][j].descriptor),
				};
			}
			else {
				writeDescriptorSets = {
					// Binding 0: input buffer
					vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, &partialSums[i][j - 1].descriptor),
					// Binding 1: output scan buffer
					vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &partialSums[i][j - 1].descriptor),
					// Binding 2: partial sum buffer
					vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2, &partialSums[i][j].descriptor),
				};
			}
			vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, VK_NULL_HANDLE);
		}
	}
}

void ExclusiveScan::createAddPartialSumDescriptorSets(vector<vks::Buffer>& outputBuffers) {
	// create descriptor pool
	vector<VkDescriptorPoolSize> poolSizes = {
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 * swapchainImageCnt * level)	// input, output
	};
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, swapchainImageCnt * level);

	VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &addPartialSumDescriptorPool));

	// create descriptor set layout Binding
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
		// Binding 0: partial sum buffer
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
		// Binding 1: output scan buffer
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1),
	};
	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &addPartialSumDescriptorSetLayout));

	// create descriptor sets
	for (int i = 0; i < swapchainImageCnt; i++) {
		for(int j = 0; j < workGroupSizes.size(); j++){
			VkDescriptorSet& descriptorSet = addPartialSumDescriptorSets[i][j];
			VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = vks::initializers::descriptorSetAllocateInfo(addPartialSumDescriptorPool, &addPartialSumDescriptorSetLayout, 1);
			VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet));

			vector<VkWriteDescriptorSet> writeDescriptorSets;
			if (j == 0) {
				writeDescriptorSets = {
					// Binding 0: input buffer(partial sum)
					vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, &partialSums[i][j].descriptor),
					// Binding 1: output buffer
					vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &outputBuffers[i].descriptor),
				};
			}
			else {
				writeDescriptorSets = {
					// Binding 0: input buffer(partial sum)
					vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, &partialSums[i][j].descriptor),
					// Binding 1: output buffer
					vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &partialSums[i][j-1].descriptor),
				};
			}
			
			vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, VK_NULL_HANDLE);
		}
	}
}

void ExclusiveScan::createDescriptorSets(vector<vks::Buffer>& inputBuffers) {
	createLocalScanDescriptorSets(inputBuffers);
	createAddPartialSumDescriptorSets(inputBuffers);
}

void ExclusiveScan::createLocalScanPipeline() {
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&localScanDescriptorSetLayout, 1);
	VkPushConstantRange pushConstantRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, sizeof(PushConstants), 0);
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
	VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &localScanPipelineLayout));

	VkComputePipelineCreateInfo computePipelineCreateInfo = vks::initializers::computePipelineCreateInfo(localScanPipelineLayout);

	//load shader
	VkPipelineShaderStageCreateInfo shaderStage = vks::initializers::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT);
	string shaderPath = "./../shaders/glsl/" + projectPath + "exclusiveScan.comp.spv";
	shaderStage.module = vks::tools::loadShader(shaderPath.c_str(), device);
	assert(shaderStage.module != VK_NULL_HANDLE);
	computePipelineCreateInfo.stage = shaderStage;
	shaderModules.push_back(shaderStage.module);
	VK_CHECK_RESULT(vkCreateComputePipelines(device, nullptr, 1, &computePipelineCreateInfo, nullptr, &localScanPipeline));
}

void ExclusiveScan::createAddPartialSumPipeline() {
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&addPartialSumDescriptorSetLayout, 1);
	VkPushConstantRange pushConstantRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, sizeof(uint32_t), 0);
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
	VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &addPartialSumPipelineLayout));

	VkComputePipelineCreateInfo computePipelineCreateInfo = vks::initializers::computePipelineCreateInfo(addPartialSumPipelineLayout);

	//load shader
	VkPipelineShaderStageCreateInfo shaderStage = vks::initializers::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT);
	string shaderPath = "./../shaders/glsl/" + projectPath + "exclusiveScan2.comp.spv";
	shaderStage.module = vks::tools::loadShader(shaderPath.c_str(), device);
	assert(shaderStage.module != VK_NULL_HANDLE);
	computePipelineCreateInfo.stage = shaderStage;
	shaderModules.push_back(shaderStage.module);
	VK_CHECK_RESULT(vkCreateComputePipelines(device, nullptr, 1, &computePipelineCreateInfo, nullptr, &addPartialSumPipeline));
}

void ExclusiveScan::createPipelines() {
	createLocalScanPipeline();
	createAddPartialSumPipeline();
}

void ExclusiveScan::prepare(vector<vks::Buffer>& buffers, uint32_t n) {
	calcLevel(n);
	createBuffers();
	for (int i = 0; i < swapchainImageCnt; i++) {
		localScanDescriptorSets[i].resize(level);
		addPartialSumDescriptorSets[i].resize(level);
	}
	createDescriptorSets(buffers);
	createPipelines();
}

//value groupSize should change if local group size change
void ExclusiveScan::record(VkCommandBuffer& commandBuffer, uint32_t imageIndex) {
	const uint32_t groupSizeX = THREADS;
	uint32_t groupCntX;
	int n = 0;

	VkMemoryBarrier barrier = vks::initializers::memoryBarrier();
	//local scan pipeline
	for (int i = 0; i < level; i++) {
		vkCmdPushConstants(commandBuffer, localScanPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &elementCnts[i]);
		vkCmdPushConstants(commandBuffer, localScanPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 4, sizeof(uint32_t), &i);
		
		barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_FLAGS_NONE, 1, &barrier, 0, nullptr, 0, nullptr);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, localScanPipelineLayout, 0, 1, &localScanDescriptorSets[imageIndex][i], 0, 0);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, localScanPipeline);
		groupCntX = workGroupSizes[i];
		vkCmdDispatch(commandBuffer, groupCntX, 1, 1);
	}

	//partial sum add pipeline
	for (int i = level - 1; i >= 0; i--) {
		vkCmdPushConstants(commandBuffer, addPartialSumPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t), &elementCnts[i]);

		barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
			VK_FLAGS_NONE, 1, &barrier, 0, nullptr, 0, nullptr);
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, addPartialSumPipelineLayout, 0, 1, &addPartialSumDescriptorSets[imageIndex][i], 0, 0);
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, addPartialSumPipeline);
		groupCntX = workGroupSizes[i];
		vkCmdDispatch(commandBuffer, groupCntX, 1, 1);
	}
}