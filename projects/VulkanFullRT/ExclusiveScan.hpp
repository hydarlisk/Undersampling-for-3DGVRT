/*
 * Sogang Univ, Graphics Lab, 2024
 *
 * Abura Soba, 2025
 */

#pragma once

#include "vulkan/vulkan.h"
#include "VulkanUtils.h"

#include <vector>

using namespace std;

class ExclusiveScan {
	uint32_t subgroupSize;
	vector<vks::Buffer> partialSums;
	uint32_t bufferSize;

	VkDescriptorPool descriptorPool1{ VK_NULL_HANDLE };
	VkDescriptorPool descriptorPool2{ VK_NULL_HANDLE };
	VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };
	VkDescriptorSetLayout descriptorSetLayout2{ VK_NULL_HANDLE };
	vector<VkDescriptorSet> descriptorSets;
	vector<VkDescriptorSet> descriptorSets2;

	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
	VkPipelineLayout pipelineLayout2{ VK_NULL_HANDLE };
	vector<VkShaderModule> shaderModules;
	VkPipeline pipeline1{ VK_NULL_HANDLE };
	VkPipeline pipeline2{ VK_NULL_HANDLE };

	string projectPath;
	VkDevice& device;
	vks::VulkanDevice& vulkanDevice;
	VkQueue& queue;
	int swapchainImageCnt;

	void createDescriptorSets1(vector<vks::Buffer>& inputBuffers, vector<vks::Buffer>& outputBuffers);
	void createDescriptorSets2(vector<vks::Buffer>& outputBuffers);
	void createDescriptorSets(vector<vks::Buffer>& inputBuffers, vector<vks::Buffer>& outputBuffers);
	void createPipeline1();
	void createPipeline2();
	void createPipelines();
	void createPartialSumBuffers(uint32_t bufferSize);
public:
	ExclusiveScan(vks::VulkanDevice& device, VkQueue& queue, int swapchainImageCnt, string projectPath);
	~ExclusiveScan();

	void prepare(vector<vks::Buffer>& inputBuffers, vector<vks::Buffer>& outputBubffers);
	void buildCommandBuffer(VkCommandBuffer commandBuffer, VulkanSwapChain& swapChain, uint32_t imageIndex, uint32_t bufferSize);
};