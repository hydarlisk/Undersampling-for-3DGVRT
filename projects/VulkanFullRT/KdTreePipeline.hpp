/*
 * Sogang Univ, Graphics Lab, 2024
 * 
 * Abura Soba, 2025
 */

#pragma once

#include "vulkan/vulkan.h"
#include "VulkanUtils.h"

#include "KdTreeModel.h"

#include <vector>

using namespace std;

class KdTreePipeline {
	vector<vks::Buffer> sortingBuffers;

	vector<VkDescriptorSet> descriptorSets;

	VkPushConstantRange pushConstantRange;
	vector<VkShaderModule> shaderModules;

	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };
	VkPipeline pipeline{ VK_NULL_HANDLE };

	string projectPath;
	VkDevice& device;
	vks::VulkanDevice& vulkanDevice;
	VkQueue& queue;
	int swapchainImageCnt;

	VkDescriptorPool descriptorPool{ VK_NULL_HANDLE };
	VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };

	string getShaderPath(string shaderName);
	void createBuffers(uint32_t width, uint32_t height);
	void createDescriptorSets(VulkanSwapChain& swapChain);
	void createPipeline();
	void createPipelineLayout();
public:
	KdTreePipeline(vks::VulkanDevice& device, VkQueue& queue, int swapchainImageCnt, string projectPath);
	~KdTreePipeline();
	
	void prepare(VulkanSwapChain& swapChain, uint32_t width, uint32_t height);
	void initDescriptorSet(int frameIdx, VulkanSwapChain& swapChain, vks::Buffer& uniformBuffer, vks::Buffer& uniformBufferStatic, vks::Buffer& particleDensities, vks::Buffer& particleSphCoefficients, KdTreeModel& kdTreeModel);
	void record(VkCommandBuffer& commandBuffer, VulkanSwapChain& swapChain, uint32_t imageIndex, uint32_t width, uint32_t height);

	void updateSwapchainImage(VkDescriptorImageInfo& info, int idx);
};