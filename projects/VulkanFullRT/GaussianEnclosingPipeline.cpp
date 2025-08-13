/*
 * Sogang Univ, Graphics Lab, 2024
 * 
 * Abura Soba, 2025
 */

#include "GaussianEnclosingPipeline.hpp"

#include "Define.h"
#include "DebugManager.hpp"

#include <vector>
#include <algorithm>

using namespace std;

GaussianEnclosingPipeline::GaussianEnclosingPipeline(vks::VulkanDevice& device, VkQueue& queue, VkCommandPool& cmdPool, vk3DGRT::Model& gModel, string projectPath) : vulkanDevice(device), device(device.logicalDevice), queue(queue), cmdPool(cmdPool), gModel(gModel){
	this->projectPath = projectPath;
	createCommandBuffer();
}

GaussianEnclosingPipeline::~GaussianEnclosingPipeline() {
	uniformBuffer.destroy();
	vkDestroyPipeline(device, pipeline, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
	if (descriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(device, descriptorPool, nullptr);
	}
	for (auto& shaderModule : shaderModules) {
		vkDestroyShaderModule(device, shaderModule, nullptr);
	}
	vkFreeCommandBuffers(device, cmdPool, 1, &commandBuffer);
}

void GaussianEnclosingPipeline::createUniformBuffer() {
	VK_CHECK_RESULT(vulkanDevice.createBuffer(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &uniformBuffer, sizeof(vks::utils::GaussianEnclosingUniformData), nullptr));
	VK_CHECK_RESULT(vulkanDevice.createBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &totalCounts, sizeof(unsigned int), 0));

	uniformData.numOfGaussians = gModel.splatSet.size();
	uniformData.kernelMinResponse = KERNEL_MIN_RESPONSE;	// these values should be managed as config val
	uniformData.opts = vks::utils::MOGRenderNone;
	uniformData.degree = KERNEL_DEGREE;

	// mapping
	vks::Buffer stagingBuffer;
	stagingBuffer.size = sizeof(vks::utils::GaussianEnclosingUniformData);
	VK_CHECK_RESULT(vulkanDevice.createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer.size, &stagingBuffer.buffer, &stagingBuffer.memory, nullptr));

	void* data;
	vkMapMemory(vulkanDevice.logicalDevice, stagingBuffer.memory, 0, sizeof(vks::utils::GaussianEnclosingUniformData), 0, &data);
	memcpy(data, (void*)&uniformData, sizeof(vks::utils::GaussianEnclosingUniformData));
	vkUnmapMemory(vulkanDevice.logicalDevice, stagingBuffer.memory);

	VkBufferCopy copyRegion;
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = stagingBuffer.size;
	vulkanDevice.copyBuffer(&stagingBuffer, &uniformBuffer, queue, &copyRegion);

	vkDestroyBuffer(vulkanDevice.logicalDevice, stagingBuffer.buffer, nullptr);
	vkFreeMemory(vulkanDevice.logicalDevice, stagingBuffer.memory, nullptr);
}

void GaussianEnclosingPipeline::createDescriptorSet(vks::Buffer& particleDensities, vks::Buffer& particleSphCoefficients) {
	// create descriptor pool
	std::vector<VkDescriptorPoolSize> poolSizes = {
		// gaussianEnclosing pipeline
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 6),	// vertices, triangles, position, rotation, scale, density
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1),
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1),	// particle density
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3),	// particle sph coefficient
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1)		// particle totalCount
	};
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, 1); // gaussianEnclosing pipeline

	VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &descriptorPool));	// descriptor pool

	// create descriptor set layout Binding
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
		// Binding 0: Vertices
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 0),
		// Binding 1: Triangles
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 1),
		// Binding 2: Position
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 2),
		// Binding 3: Rotation
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 3),
		// Binding 4: Scale
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 4),
		// Binding 5: Density
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 5),
		// Binding 6: Uniform Buffer
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 6),
		// Binding 7: Particle density
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 7),
		// Binding 8: Features Albedo
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 8),
		// Binding 9: Features Specular
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 9),
		// Binding 10: Particle Sph Coefficient
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 10),
		// Binding 11: Particle Total Counts
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT, 11),
	};

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayout));

	VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
	VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet));

	std::vector<VkWriteDescriptorSet> computeWriteDescriptorSets = {
		// Binding 0: Vertices
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 0, &gModel.vertices.storageBuffer.descriptor),
		// Binding 1: Triangles
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, &gModel.indices.storageBuffer.descriptor),
		// Binding 2: Position
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2, &gModel.positions.storageBuffer.descriptor),
		// Binding 3: Rotation
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3, &gModel.rotations.storageBuffer.descriptor),
		// Binding 4: Scale
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4, &gModel.scales.storageBuffer.descriptor),
		// Binding 5: Density
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 5, &gModel.densities.storageBuffer.descriptor),
		// Binding 6: Uniform Buffer 
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 6, &uniformBuffer.descriptor),
		// Binding 7: Particle density
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 7, &particleDensities.descriptor),
		// Binding 8: Features albedo
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 8, &gModel.featuresAlbedo.storageBuffer.descriptor),
		// Binding 9: Features specular
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 9, &gModel.featuresSpecular.storageBuffer.descriptor),
		// Binding 10: Particle sph coefficient
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10, &particleSphCoefficients.descriptor),
		// Binding 11: Particle Total Counts
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 11, &totalCounts.descriptor),
	};

	vkUpdateDescriptorSets(device, static_cast<uint32_t>(computeWriteDescriptorSets.size()), computeWriteDescriptorSets.data(), 0, nullptr);
}

void GaussianEnclosingPipeline::createPipeline() {
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
	VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));

	VkComputePipelineCreateInfo computePipelineCreateInfo = vks::initializers::computePipelineCreateInfo(pipelineLayout);

	//load shader
	VkPipelineShaderStageCreateInfo shaderStage = vks::initializers::pipelineShaderStageCreateInfo(VK_SHADER_STAGE_COMPUTE_BIT);
	string shaderPath = "./../shaders/glsl/" + projectPath + "particlePrimitives.comp.spv";
	shaderStage.module = vks::tools::loadShader(shaderPath.c_str(), device);
	assert(shaderStage.module != VK_NULL_HANDLE);
	computePipelineCreateInfo.stage = shaderStage;

	VK_CHECK_RESULT(vkCreateComputePipelines(device, nullptr, 1, &computePipelineCreateInfo, nullptr, &pipeline));
}

void GaussianEnclosingPipeline::createCommandBuffer() {
	VkCommandBufferAllocateInfo cmdBufAllocateInfo = vks::initializers::commandBufferAllocateInfo(cmdPool, VK_COMMAND_BUFFER_LEVEL_PRIMARY, 1);
	VK_CHECK_RESULT(vkAllocateCommandBuffers(device, &cmdBufAllocateInfo, &commandBuffer));
}

void GaussianEnclosingPipeline::buildCommandBuffer() {
	VkCommandBufferBeginInfo cmdBufInfo = vks::initializers::commandBufferBeginInfo();
	VK_CHECK_RESULT(vkBeginCommandBuffer(commandBuffer, &cmdBufInfo));

	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, 0);

	uint32_t groupCountX = NUM_OF_GAUSSIANS;
	vkCmdDispatch(commandBuffer, (gModel.splatSet.size() + groupCountX - 1) / groupCountX, 1, 1);

	VK_CHECK_RESULT(vkEndCommandBuffer(commandBuffer));
}

void GaussianEnclosingPipeline::prepare(vks::Buffer& particleDensities, vks::Buffer& particleSphCoefficients) {
	createCommandBuffer();
	createUniformBuffer();
	createDescriptorSet(particleDensities, particleSphCoefficients);
	createPipeline();
}

void GaussianEnclosingPipeline::run() {
	buildCommandBuffer();
	VkSubmitInfo submitInfo = vks::initializers::submitInfo();
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;
	VK_CHECK_RESULT(vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE));
	vkDeviceWaitIdle(device);
	DebugManager::getInstance().dumpIcosahedron(gModel.vertices.storageBuffer, gModel.indices.storageBuffer, gModel.vertices.count, gModel.indices.count);
}