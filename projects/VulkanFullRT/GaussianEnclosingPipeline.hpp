/*
 * Sogang Univ, Graphics Lab, 2024
 * 
 * Abura Soba, 2025
 */

#pragma once

#include "vulkan/vulkan.h"
#include "VulkanUtils.h"

#include "Vulkan3DGRTModel.h"

#include <vector>

using namespace std;

class GaussianEnclosingPipeline {
	vk3DGRT::Model& gModel;
	vks::utils::GaussianEnclosingUniformData uniformData;
	vks::Buffer uniformBuffer;
	vks::Buffer totalCounts;
	VkCommandBuffer commandBuffer{ VK_NULL_HANDLE };

	VkDescriptorSet descriptorSet{ VK_NULL_HANDLE };
	VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };
	VkDescriptorPool descriptorPool{ VK_NULL_HANDLE };

	VkPipeline pipeline{ VK_NULL_HANDLE };
	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
	vector<VkShaderModule> shaderModules;

	string projectPath;
	VkDevice& device;
	vks::VulkanDevice& vulkanDevice;
	VkQueue& queue;
	VkCommandPool& cmdPool;
	
	void createCommandBuffer();
	void createUniformBuffer();
	void createDescriptorSet(vks::Buffer& particleDensities, vks::Buffer& particleSphCoefficients);
	void createPipeline();
	void buildCommandBuffer();
public:
	GaussianEnclosingPipeline(vks::VulkanDevice& device, VkQueue& queue, VkCommandPool& cmdPool, vk3DGRT::Model& gModel, string projectPath);
	~GaussianEnclosingPipeline();

	void prepare(vks::Buffer& particleDensities, vks::Buffer& particleSphCoefficients);
	void run();

	void dumpIcosahedron();
};