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
	struct AccelerationStructure {
		VkAccelerationStructureKHR handle;
		uint64_t deviceAddress = 0;
		VkDeviceMemory memory;
		VkBuffer buffer;
	};
	/* exclusive scan test */
	vector<vks::Buffer> testInput;
	vector<vks::Buffer> testOutput;

	ExclusiveScan* exclusiveScan;
	vector<vks::Buffer> rtMaskBuffers;
	vector<vks::Buffer> rtMaskScanBuffers;

	vector<VkDescriptorSet> descriptorSets;

	vector<VkDescriptorSet> interpolationDescriptorSets;
	vector<VkDescriptorSet> additionalRTDescriptorSets;

	VkPushConstantRange pushConstantRange;
	vector<VkShaderModule> shaderModules;

	VkPipelineLayout interpolationPipelineLayout{ VK_NULL_HANDLE };
	VkPipeline horizontalPipeline{ VK_NULL_HANDLE };
	VkPipeline verticalPipeline{ VK_NULL_HANDLE };

	VkPipelineLayout additionalRTPipelineLayout{};
	VkPipeline additionalRTPipeline{ VK_NULL_HANDLE };


	string projectPath;
	VkDevice& device;
	vks::VulkanDevice& vulkanDevice;
	VkQueue& queue;
	int swapchainImageCnt;

	VkDescriptorPool interpolationDescriptorPool{ VK_NULL_HANDLE };
	VkDescriptorSetLayout interpolationDescriptorSetLayout{ VK_NULL_HANDLE };
	VkDescriptorPool additionalRTDescriptorPool{ VK_NULL_HANDLE };
	VkDescriptorSetLayout additionalRTDescriptorSetLayout{ VK_NULL_HANDLE };

	string getShaderPath(string shaderName);
	void createMaskBuffers(uint32_t width, uint32_t height);
	void createInterpolationDescriptorSets(VulkanSwapChain& swapChain);
	void createHorizontalPipeline();
	void createVerticalPipeline();
	//void createRTDescriptorSets(VulkanSwapChain& swapChain, AccelerationStructure topLevelAS3DGRT);
	//void createRTPipeline();
	void createPipelineLayouts();
	void createPipelines();
public:
	USPipeline(vks::VulkanDevice& device, VkQueue& queue, int swapchainImageCnt, string projectPath);
	~USPipeline();
	
	void prepare(VulkanSwapChain& swapChain, uint32_t width, uint32_t height);
	void recordHorizontalPipeline(VkCommandBuffer& commandBuffer, VulkanSwapChain& swapChain, uint32_t imageIndex, uint32_t width, uint32_t height);
	void recordVerticalPipeline(VkCommandBuffer& commandBuffer, VulkanSwapChain& swapChain, uint32_t imageIndex, uint32_t width, uint32_t height);
	void buildCommandBuffer(VkCommandBuffer& commandBuffer, VulkanSwapChain& swapChain, uint32_t imageIndex, uint32_t width, uint32_t height);

	void debugExclusiveScan(VkCommandBuffer commandBuffer, VulkanSwapChain& swapChain, uint32_t imageIndex);
};