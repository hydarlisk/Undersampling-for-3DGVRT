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

struct StorageImage {
	VkDeviceMemory memory = VK_NULL_HANDLE;
	VkImage image = VK_NULL_HANDLE;
	VkImageView view = VK_NULL_HANDLE;
	VkFormat format;
};

class USPipeline {
	vector<VkDescriptorSet> descriptorSets;
	vector<StorageImage> storageImages;

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

	//void createStorageImage(StorageImage& storageImage, VkFormat format, VkExtent3D extent);
	void createHorizontalPipeline();
	void createVerticalPipeline();
public:
	USPipeline(vks::VulkanDevice& device, VkQueue queue, int swapchainImageCnt, string projectPath);
	~USPipeline();
	void init(vks::VulkanDevice& device, VkQueue queue, int swapchainImageCnt, string projectPath);
	void createDescriptorSets(VulkanSwapChain& swapChain);
	void createPipelines();
	void buildCommandBuffer(VkCommandBuffer commandBuffer, VulkanSwapChain& swapChain, uint32_t imageIndex, uint32_t width, uint32_t height);

	void handleResize();
};