/*
 * Sogang Univ, Graphics Lab, 2024
 * 
 * Abura Soba, 2025
 */

#pragma once

#include "vulkan/vulkan.h"
#include "VulkanUtils.h"
#include "ExclusiveScan.hpp"

#include <vector>

using namespace std;

class USPipeline {
	/* exclusive scan test */
	vector<vks::Buffer> testInput;
	vector<vks::Buffer> testOutput;

	ExclusiveScan* exclusiveScan;
	vector<vks::Buffer> rtMaskBuffers;
	vector<vks::Buffer> rtMaskScanBuffers;

	vector<VkDescriptorSet> descriptorSets;

	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
	vector<VkShaderModule> shaderModules;
	VkPipeline horizontalPipeline{ VK_NULL_HANDLE };
	VkPipeline verticalPipeline{ VK_NULL_HANDLE };


	string projectPath;
	VkDevice& device;
	vks::VulkanDevice& vulkanDevice;
	VkQueue& queue;
	int swapchainImageCnt;

	VkDescriptorPool descriptorPool{ VK_NULL_HANDLE };
	VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };

	void createHorizontalPipeline();
	void createVerticalPipeline();
public:
	USPipeline(vks::VulkanDevice& device, VkQueue& queue, int swapchainImageCnt, string projectPath);
	~USPipeline();

	void createDescriptorSets(VulkanSwapChain& swapChain);
	void createPipelines();
	void buildCommandBuffer(VkCommandBuffer commandBuffer, VulkanSwapChain& swapChain, uint32_t imageIndex, uint32_t width, uint32_t height);

	void debugExclusiveScan(VkCommandBuffer commandBuffer, VulkanSwapChain& swapChain, uint32_t imageIndex);
};