/*
 * Sogang Univ, Graphics Lab, 2024
 * 
 * Abura Soba, 2025
 */

#include "USPipeline.hpp"
#include "Define.h"

#include <vector>
#include <algorithm>

using namespace std;

USPipeline::USPipeline(vks::VulkanDevice& device, VkQueue queue, int swapchainImageCnt, string projectPath) : vulkanDevice(device), device(device.logicalDevice), queue(queue){
	this->swapchainImageCnt = swapchainImageCnt;
	this->projectPath = projectPath;
	descriptorSets.resize(swapchainImageCnt);

	uint32_t sharedDataSize = min((uint32_t)1024, (uint32_t)(vulkanDevice.properties.limits.maxComputeSharedMemorySize / sizeof(glm::vec4)));
	assert(sharedDataSize >= 1024);
}

USPipeline::~USPipeline() {
	if (descriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(device, descriptorPool, nullptr);
	}

	for (auto& shaderModule : shaderModules) {
		vkDestroyShaderModule(device, shaderModule, nullptr);
	}
}

void USPipeline::init(vks::VulkanDevice& device, VkQueue queue, int swapchainImageCnt, string projectPath) {
	this->vulkanDevice = device;
	this->device = device.logicalDevice;
	this->queue = queue;
	this->swapchainImageCnt = swapchainImageCnt;
	this->projectPath = projectPath;
	descriptorSets.resize(swapchainImageCnt);
}

void USPipeline::createDescriptorSets(VulkanSwapChain& swapChain) {
	// create descriptor pool
	vector<VkDescriptorPoolSize> poolSizes = {
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 2 * swapchainImageCnt)	// input, output
	};
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, swapchainImageCnt);

	VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &descriptorPool));

	// create descriptor set layout Binding
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
		// Binding 0: RT result image
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT, 0),
	};

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayout));

	for (int i = 0; i < swapChain.imageCount; i++) {
		VkDescriptorSet& descriptorSet = descriptorSets[i];
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet));

		//// input - tmp image(copy of RT result)
		//VkDescriptorImageInfo inputStorageImageDescriptor = { VK_NULL_HANDLE, storageImages[i].view, VK_IMAGE_LAYOUT_GENERAL};
		// output - swapchain image
		VkDescriptorImageInfo storageImageDescriptor = { VK_NULL_HANDLE, swapChain.buffers[i].view, VK_IMAGE_LAYOUT_GENERAL };

		vector<VkWriteDescriptorSet> writeDescriptorSets = {
			// Binding 0: output image
			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 0, &storageImageDescriptor),
			//// Binding 1: input image
			//vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, &inputStorageImageDescriptor),
		};
		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, VK_NULL_HANDLE);
	}
}

//TODO
void USPipeline::handleResize() {
	for (int i = 0; i < swapchainImageCnt; i++) {
//		createStorageImage
	}
}

void USPipeline::createHorizontalPipeline() {
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
	VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

	VkComputePipelineCreateInfo computePipelineCreateInfo = vks::initializers::computePipelineCreateInfo(pipelineLayout);
	
	//load shader
	VkPipelineShaderStageCreateInfo shaderStage = vks::initializers::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT);
	string shaderPath = "./../shaders/glsl/" + projectPath + "horizontalLinearInterpolation.comp.spv";
	shaderStage.module = vks::tools::loadShader(shaderPath.c_str(), device);
	assert(shaderStage.module != VK_NULL_HANDLE);
	computePipelineCreateInfo.stage = shaderStage;

	VK_CHECK_RESULT(vkCreateComputePipelines(device, nullptr, 1, &computePipelineCreateInfo, nullptr, &horizontalPipeline));
}

void USPipeline::createVerticalPipeline() {
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
	VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

	VkComputePipelineCreateInfo computePipelineCreateInfo = vks::initializers::computePipelineCreateInfo(pipelineLayout);

	//load shader
	VkPipelineShaderStageCreateInfo shaderStage = vks::initializers::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT);
	string shaderPath = "./../shaders/glsl/" + projectPath + "verticalLinearInterpolation.comp.spv";
	shaderStage.module = vks::tools::loadShader(shaderPath.c_str(), device);
	assert(shaderStage.module != VK_NULL_HANDLE);
	computePipelineCreateInfo.stage = shaderStage;

	VK_CHECK_RESULT(vkCreateComputePipelines(device, nullptr, 1, &computePipelineCreateInfo, nullptr, &verticalPipeline));
}

void USPipeline::createPipelines() {
	createHorizontalPipeline();
	createVerticalPipeline();
}

void USPipeline::buildCommandBuffer(VkCommandBuffer commandBuffer, VulkanSwapChain& swapChain, uint32_t imageIndex, uint32_t width, uint32_t height) {
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
	VkWriteDescriptorSet resultImageWrite = vks::initializers::writeDescriptorSet(descriptorSets[imageIndex], VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 0, &storageImageDescriptor);
	vkUpdateDescriptorSets(device, 1, &resultImageWrite, 0, VK_NULL_HANDLE);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSets[imageIndex], 0, 0);
	
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