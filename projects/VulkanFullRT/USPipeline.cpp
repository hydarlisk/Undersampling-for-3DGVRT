/*
 * Sogang Univ, Graphics Lab, 2024
 * 
 * Abura Soba, 2025
 */

#include "USPipeline.hpp"
#include "Define.h"

#include <vector>
#include <algorithm>

#include <numeric>

using namespace std;

// value 1024 should change if local group size change
USPipeline::USPipeline(vks::VulkanDevice& device, VkQueue& queue, int swapchainImageCnt, string projectPath) : vulkanDevice(device), device(device.logicalDevice), queue(queue){
	this->swapchainImageCnt = swapchainImageCnt;
	this->projectPath = projectPath;
	interpolationDescriptorSets.resize(swapchainImageCnt);
	rtMaskBuffers.resize(swapchainImageCnt);

	exclusiveScan = new ExclusiveScan(device, queue, swapchainImageCnt, projectPath);

	uint32_t sharedDataSize = min((uint32_t)1024, (uint32_t)(vulkanDevice.properties.limits.maxComputeSharedMemorySize / sizeof(glm::vec4)));
	assert(sharedDataSize >= 1024);
}

USPipeline::~USPipeline() {
	vkDestroyPipeline(device, horizontalPipeline, nullptr);
	vkDestroyPipeline(device, verticalPipeline, nullptr);
	vkDestroyPipelineLayout(device, interpolationPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, interpolationDescriptorSetLayout, nullptr);
	if (interpolationDescriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(device, interpolationDescriptorPool, nullptr);
	}

	for (auto& shaderModule : shaderModules) {
		vkDestroyShaderModule(device, shaderModule, nullptr);
	}
}

string USPipeline::getShaderPath(string shaderName) {
	return "./../shaders/glsl/" + projectPath + shaderName;
}

void USPipeline::createMaskBuffers(uint32_t width, uint32_t height) {
	for (int i = 0; i < swapchainImageCnt; i++) {
		VK_CHECK_RESULT(vulkanDevice.createBuffer(
			VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&rtMaskBuffers[i],
			width * height * sizeof(uint32_t)));
	}
}

void USPipeline::createInterpolationDescriptorSets(VulkanSwapChain& swapChain) {
	// create descriptor pool
	vector<VkDescriptorPoolSize> poolSizes = {
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 * swapchainImageCnt),	// input, output
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 * swapchainImageCnt)
	};
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, swapchainImageCnt);

	VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &interpolationDescriptorPool));

	// create descriptor set layout Binding
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
		// Binding 0: RT result image
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 0),
		// Binding 1: RT mask buffer
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1),
	};

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &interpolationDescriptorSetLayout));

	for (int i = 0; i < swapChain.imageCount; i++) {
		VkDescriptorSet& descriptorSet = interpolationDescriptorSets[i];
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = vks::initializers::descriptorSetAllocateInfo(interpolationDescriptorPool, &interpolationDescriptorSetLayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet));

		VkDescriptorImageInfo storageImageDescriptor = { VK_NULL_HANDLE, swapChain.buffers[i].view, VK_IMAGE_LAYOUT_GENERAL };

		vector<VkWriteDescriptorSet> writeDescriptorSets = {
			// Binding 0: output image
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 0, &storageImageDescriptor),
			//// Binding 1: input image
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &rtMaskBuffers[i].descriptor)
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, VK_NULL_HANDLE);
	}
}

void USPipeline::createPipelineLayouts() {
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&interpolationDescriptorSetLayout, 1);
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
	VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &interpolationPipelineLayout));
}

void USPipeline::createHorizontalPipeline() {
	VkComputePipelineCreateInfo computePipelineCreateInfo = vks::initializers::computePipelineCreateInfo(interpolationPipelineLayout);
	
	//load shader	
	string shaderPath = getShaderPath("horizontalLinearInterpolation.comp.spv");
	VkPipelineShaderStageCreateInfo shaderStage = vks::utils::createShaderStageCI(device, shaderPath, VK_SHADER_STAGE_COMPUTE_BIT);
	shaderModules.push_back(shaderStage.module);
	computePipelineCreateInfo.stage = shaderStage;
	VK_CHECK_RESULT(vkCreateComputePipelines(device, nullptr, 1, &computePipelineCreateInfo, nullptr, &horizontalPipeline));
}

void USPipeline::createVerticalPipeline() {
	VkComputePipelineCreateInfo computePipelineCreateInfo = vks::initializers::computePipelineCreateInfo(interpolationPipelineLayout);

	//load shader
	string shaderPath = getShaderPath("verticalLinearInterpolation.comp.spv");
	VkPipelineShaderStageCreateInfo shaderStage = vks::utils::createShaderStageCI(device, shaderPath, VK_SHADER_STAGE_COMPUTE_BIT);
	shaderModules.push_back(shaderStage.module);
	computePipelineCreateInfo.stage = shaderStage;
	VK_CHECK_RESULT(vkCreateComputePipelines(device, nullptr, 1, &computePipelineCreateInfo, nullptr, &verticalPipeline));
}

void USPipeline::createPipelines() {
	createHorizontalPipeline();
	createVerticalPipeline();
}

void USPipeline::prepare(VulkanSwapChain& swapChain, uint32_t width, uint32_t height) {
	createMaskBuffers(width, height);
	createInterpolationDescriptorSets(swapChain);
	pushConstantRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 0, 4);
	createPipelineLayouts();
	createPipelines();
}


//value groupSize should change if local group size change
void USPipeline::recordHorizontalPipeline(VkCommandBuffer& commandBuffer, VulkanSwapChain& swapChain, uint32_t imageIndex, uint32_t width, uint32_t height) {
	const uint32_t groupSizeX = 32;
	const uint32_t groupSizeY = 32;
	uint32_t groupCntX;
	uint32_t groupCntY;

	VkMemoryBarrier barrier = vks::initializers::memoryBarrier();
	barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_FLAGS_NONE, 1, &barrier, 0, nullptr, 0, nullptr);

	VkDescriptorImageInfo storageImageDescriptor{ VK_NULL_HANDLE, swapChain.buffers[imageIndex].view, VK_IMAGE_LAYOUT_GENERAL };
	VkWriteDescriptorSet resultImageWrite = vks::initializers::writeDescriptorSet(interpolationDescriptorSets[imageIndex], VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 0, &storageImageDescriptor);
	vkUpdateDescriptorSets(device, 1, &resultImageWrite, 0, VK_NULL_HANDLE);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, interpolationPipelineLayout, 0, 1, &interpolationDescriptorSets[imageIndex], 0, 0);

	uint32_t pushConstant[2] = { width, height };
	vkCmdPushConstants(commandBuffer, interpolationPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 16, pushConstant);

	//horizontal pass
	groupCntX = (width / 2 + groupSizeX - 1) / groupSizeX;
	groupCntY = (height / 2 + groupSizeY - 1) / groupSizeY;
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, horizontalPipeline);
	vkCmdDispatch(commandBuffer, groupCntX, groupCntY, 1);

	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_FLAGS_NONE, 1, &barrier, 0, nullptr, 0, nullptr);
}

void USPipeline::recordVerticalPipeline(VkCommandBuffer& commandBuffer, VulkanSwapChain& swapChain, uint32_t imageIndex, uint32_t width, uint32_t height) {
	const uint32_t groupSizeX = 32;
	const uint32_t groupSizeY = 32;
	uint32_t groupCntX;
	uint32_t groupCntY;

	VkMemoryBarrier barrier = vks::initializers::memoryBarrier();
	barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_FLAGS_NONE, 1, &barrier, 0, nullptr, 0, nullptr);

	//vertical pass
	groupCntX = (width + groupSizeX - 1) / groupSizeX;
	groupCntY = (height / 2 + groupSizeY - 1) / groupSizeY;
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, verticalPipeline);
	vkCmdDispatch(commandBuffer, groupCntX, groupCntY, 1);
}

//value groupSize should change if local group size change
void USPipeline::buildCommandBuffer(VkCommandBuffer& commandBuffer, VulkanSwapChain& swapChain, uint32_t imageIndex, uint32_t width, uint32_t height) {
	const uint32_t groupSizeX = 32;
	const uint32_t groupSizeY = 32;
	uint32_t groupCntX;
	uint32_t groupCntY;

	VkMemoryBarrier barrier = vks::initializers::memoryBarrier();
	barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
	barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_FLAGS_NONE, 1, &barrier, 0, nullptr, 0, nullptr);

	VkDescriptorImageInfo storageImageDescriptor{ VK_NULL_HANDLE, swapChain.buffers[imageIndex].view, VK_IMAGE_LAYOUT_GENERAL };
	VkWriteDescriptorSet resultImageWrite = vks::initializers::writeDescriptorSet(interpolationDescriptorSets[imageIndex], VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 0, &storageImageDescriptor);
	vkUpdateDescriptorSets(device, 1, &resultImageWrite, 0, VK_NULL_HANDLE);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, interpolationPipelineLayout, 0, 1, &interpolationDescriptorSets[imageIndex], 0, 0);

	uint32_t pushConstant[2] = { width, height };
	vkCmdPushConstants(commandBuffer, interpolationPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 16, pushConstant);
	
	//horizontal pass
	groupCntX = (width / 2 + groupSizeX - 1) / groupSizeX;
	groupCntY = (height / 2 + groupSizeY - 1) / groupSizeY;
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, horizontalPipeline);
	vkCmdDispatch(commandBuffer, groupCntX, groupCntY, 1);

	vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
		VK_FLAGS_NONE, 1, &barrier, 0, nullptr, 0, nullptr);
	//vertical pass
	groupCntX = (width + groupSizeX - 1) / groupSizeX;
	groupCntY = (height / 2 + groupSizeY - 1) / groupSizeY;
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, verticalPipeline);
	vkCmdDispatch(commandBuffer, groupCntX, groupCntY, 1);
}