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

class RTPipeline {
	struct AccelerationStructure {
		VkAccelerationStructureKHR handle;
		uint64_t deviceAddress = 0;
		VkDeviceMemory memory;
		VkBuffer buffer;
	};

	vector<VkDescriptorSet> descriptorSets;

	VkPushConstantRange pushConstantRange;
	vector<VkShaderModule> shaderModules;

	VkPipelineLayout pipelineLayout{ VK_NULL_HANDLE };

	string projectPath;
	VkDevice& device;
	vks::VulkanDevice& vulkanDevice;
	VkQueue& queue;
	int swapchainImageCnt;

	VkDescriptorPool descriptorPool{ VK_NULL_HANDLE };
	VkDescriptorSetLayout descriptorSetLayout{ VK_NULL_HANDLE };

	PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;
	PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;

	string getShaderPath(string shaderName);
	void createDescriptorSets();
	void createPipelineLayout();
	void createPipeline();
public:
	vector<vks::Buffer> rtMaskBuffers;
	VkPipeline pipeline{ VK_NULL_HANDLE };
	std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups{};

	RTPipeline(vks::VulkanDevice& device, VkQueue& queue, int swapchainImageCnt, string projectPath);
	~RTPipeline();
	
	void prepare(uint32_t width, uint32_t height);
	void initDescriptorSet(int frameIdx, VulkanSwapChain& swapChain, VkAccelerationStructureKHR& tlasHandle, vks::Buffer& uniformBuffer, vks::Buffer& uniformBufferStatic, vks::Buffer& particleDensities, vks::Buffer& particleSphCoefficients);
	//void updateDescriptorSet(vks::Buffer uniformBuffer);
	void record(VkCommandBuffer& commandBuffer, VkStridedDeviceAddressRegionKHR& raygen, VkStridedDeviceAddressRegionKHR& miss, VkStridedDeviceAddressRegionKHR& hit, uint32_t imageIndex, uint32_t width, uint32_t height, uint32_t additionalRTFlag);
	//void buildCommandBuffer(VkCommandBuffer commandBuffer, VulkanSwapChain& swapChain, uint32_t imageIndex, uint32_t width, uint32_t height);
};