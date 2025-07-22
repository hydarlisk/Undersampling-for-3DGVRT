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

// value 1024 should change if local group size change
USPipeline::USPipeline(vks::VulkanDevice& device, VkQueue& queue, int swapchainImageCnt, string projectPath) : vulkanDevice(device), device(device.logicalDevice), queue(queue){
	this->swapchainImageCnt = swapchainImageCnt;
	this->projectPath = projectPath;
	interpolationDescriptorSets.resize(swapchainImageCnt);
	additionalRTDescriptorSets.resize(swapchainImageCnt);
	rtMaskBuffers.resize(swapchainImageCnt);

	uint32_t sharedDataSize = min((uint32_t)1024, (uint32_t)(vulkanDevice.properties.limits.maxComputeSharedMemorySize / sizeof(glm::vec4)));
	assert(sharedDataSize >= 1024);
}

USPipeline::~USPipeline() {
	vkDestroyPipeline(device, horizontalPipeline, nullptr);
	vkDestroyPipeline(device, verticalPipeline, nullptr);
	vkDestroyPipeline(device, additionalRTPipeline, nullptr);
	vkDestroyPipelineLayout(device, interpolationPipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, interpolationDescriptorSetLayout, nullptr);
	if (interpolationDescriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(device, interpolationDescriptorPool, nullptr);
	}

	for (auto& shaderModule : shaderModules) {
		vkDestroyShaderModule(device, shaderModule, nullptr);
	}
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
	VkPipelineShaderStageCreateInfo shaderStage = vks::initializers::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT);
	string shaderPath = "./../shaders/glsl/" + projectPath + "horizontalLinearInterpolation.comp.spv";
	shaderStage.module = vks::tools::loadShader(shaderPath.c_str(), device);
	assert(shaderStage.module != VK_NULL_HANDLE);
	computePipelineCreateInfo.stage = shaderStage;

	VK_CHECK_RESULT(vkCreateComputePipelines(device, nullptr, 1, &computePipelineCreateInfo, nullptr, &horizontalPipeline));
}

void USPipeline::createVerticalPipeline() {
	VkComputePipelineCreateInfo computePipelineCreateInfo = vks::initializers::computePipelineCreateInfo(interpolationPipelineLayout);

	//load shader
	VkPipelineShaderStageCreateInfo shaderStage = vks::initializers::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT);
	string shaderPath = "./../shaders/glsl/" + projectPath + "verticalLinearInterpolation.comp.spv";
	shaderStage.module = vks::tools::loadShader(shaderPath.c_str(), device);
	assert(shaderStage.module != VK_NULL_HANDLE);
	computePipelineCreateInfo.stage = shaderStage;

	VK_CHECK_RESULT(vkCreateComputePipelines(device, nullptr, 1, &computePipelineCreateInfo, nullptr, &verticalPipeline));
}

void USPipeline::createRTDescriptorSets(VulkanSwapChain& swapChain, AccelerationStructure topLevelAS3DGRT) {
//	std::vector<VkDescriptorPoolSize> poolSizes = {
//		// ray tracing pipeline
//		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 * swapChain.imageCount),
//		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 * swapChain.imageCount),
//		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 * swapChain.imageCount),
//		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 * swapChain.imageCount),
//#if SPLIT_BLAS && !RAY_QUERY
//			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 * swapChain.imageCount),
//#endif 
//#if ENABLE_HIT_COUNTS && !RAY_QUERY
//			vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 * swapChain.imageCount),
//#endif
//	};
//	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, swapChain.imageCount); // ray tracing pipeline
//
//	VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &additionalRTDescriptorPool));	// descriptor pool
//
//	// for ray tracing pipeline begin
//	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
//		// Binding 0: Top level acceleration structure
//		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0),
//		// Binding 1: Ray tracing result image
//		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 1),
//		// Binding 2: Uniform buffer Dynamic
//		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 2),
//		// Binding 3: Uniform buffer Static
//		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 3),
//		// Binding 4: Storage buffer - Particle Densities
//		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 4),
//		// Binding 5: Storage buffer - Particle Sph Coefficients
//		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 5),
//	#if SPLIT_BLAS && !RAY_QUERY
//		// Binding 6: Storage buffer - primitive Id
//		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ANY_HIT_BIT_KHR, 6),
//	#endif
//	#if ENABLE_HIT_COUNTS
//		// Binding 7: Storage buffer - Ray Hit Count for debugging
//		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 7),
//	#endif
//	};
//
//	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
//	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &additionalRTDescriptorSetLayout));
//
//	for (int i = 0; i < swapchainImageCnt; i++)
//	{
//		VkDescriptorSet& descriptorSet = additionalRTDescriptorSets[i];
//		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = vks::initializers::descriptorSetAllocateInfo(additionalRTDescriptorPool, &additionalRTDescriptorSetLayout, 1);
//		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet));	// descriptor set
//
//		// WriteDescriptorSet for TLAS (binding0)
//		VkWriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo = vks::initializers::writeDescriptorSetAccelerationStructureKHR();
//		descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
//#if SPLIT_BLAS && !RAY_QUERY
//		descriptorAccelerationStructureInfo.pAccelerationStructures = &splitBLAS.splittedTLAS.handle;
//#else
//		descriptorAccelerationStructureInfo.pAccelerationStructures = &topLevelAS3DGRT.handle;
//#endif
//
//		VkWriteDescriptorSet accelerationStructureWrite{};
//		accelerationStructureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
//		// The specialized acceleration structure descriptor has to be chained
//		accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo;
//		accelerationStructureWrite.dstSet = descriptorSet;
//		accelerationStructureWrite.dstBinding = 0;
//		accelerationStructureWrite.descriptorCount = 1;
//		accelerationStructureWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
//
//		VkDescriptorImageInfo storageImageDescriptor = { VK_NULL_HANDLE, swapChain.buffers[i].view, VK_IMAGE_LAYOUT_GENERAL };
//
//		std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
//			// Binding 0: Top level acceleration structure
//			accelerationStructureWrite,
//			// Binding 1: Ray tracing result image
//			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, &storageImageDescriptor),
//			// Binding 2: Uniform data Dynamic
//			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2, &frame.uniformBuffer.descriptor),
//			// Binding 3: Uniform data Static
//			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3, &frame.uniformBufferStatic.descriptor),
//			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4, &particleDensities.descriptor),
//			vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 5, &particleSphCoefficients.descriptor),
//#if SPLIT_BLAS && !RAY_QUERY
//				vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 6, &splitBLAS.d_splittedPrimitiveIdsDeviceAddress.descriptor),
//#endif
//#if ENABLE_HIT_COUNTS && !RAY_QUERY
//				vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 7, &frame.hitCountsbuffer.descriptor),
//#endif
//		};
//
//		vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, VK_NULL_HANDLE);
//	}
//	// for ray tracing pipeline end
}

void USPipeline::createRTPipeline() {

}

void USPipeline::createPipelines() {
	createHorizontalPipeline();
	createVerticalPipeline();
	//createRTPipeline();
}

void USPipeline::prepare(VulkanSwapChain& swapChain, uint32_t width, uint32_t height) {
	createMaskBuffers(width, height);
	createInterpolationDescriptorSets(swapChain);
	//createRTDescriptorSets(swapChain);
	createPipelineLayouts();
	pushConstantRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, 8, 0);
	createPipelines();
}

//value groupSize should change if local group size change
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