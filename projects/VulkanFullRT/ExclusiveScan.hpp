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

#define THREADS 1024
#define SUBGROUP_SIZE 32
#define SHARED_MEMORY_SIZE 128
// thread per block : 1024
// subgroup size : 32
// max shared memory per workgroup : 32 * 4byte

//TODO: handle resize
class ExclusiveScan {
private:
	struct PushConstants {
		uint32_t n;
		uint32_t additional;
	}pushConstants;

	uint32_t subgroupSize;
	vector<uint32_t> workGroupSizes;
	vector<uint32_t> elementCnts;
	vector<vector<vks::Buffer>> partialSums;
	uint32_t level = 0;

	VkDescriptorPool localScanDescriptorPool{ VK_NULL_HANDLE };
	VkDescriptorPool addPartialSumDescriptorPool{ VK_NULL_HANDLE };
	VkDescriptorSetLayout localScanDescriptorSetLayout{ VK_NULL_HANDLE };
	VkDescriptorSetLayout addPartialSumDescriptorSetLayout{ VK_NULL_HANDLE };
	vector<vector<VkDescriptorSet>> localScanDescriptorSets;
	vector<vector<VkDescriptorSet>> addPartialSumDescriptorSets;

	VkPipelineLayout localScanPipelineLayout{ VK_NULL_HANDLE };
	VkPipelineLayout addPartialSumPipelineLayout{ VK_NULL_HANDLE };
	vector<VkShaderModule> shaderModules;
	VkPipeline localScanPipeline{ VK_NULL_HANDLE };
	VkPipeline addPartialSumPipeline{ VK_NULL_HANDLE };

	string projectPath;
	VkDevice& device;
	vks::VulkanDevice& vulkanDevice;
	VkQueue& queue;
	int swapchainImageCnt;

	void calcLevel(uint32_t n);
	void createBuffers();
	void createLocalScanDescriptorSets(vector<vks::Buffer>& inputBuffers);
	void createAddPartialSumDescriptorSets(vector<vks::Buffer>& outputBuffers);
	void createDescriptorSets(vector<vks::Buffer>& inputBuffers);
	void createLocalScanPipeline();
	void createAddPartialSumPipeline();
	void createPipelines();

public:
	ExclusiveScan(vks::VulkanDevice& device, VkQueue& queue, int swapchainImageCnt, string projectPath);
	~ExclusiveScan();

	void prepare(vector<vks::Buffer>& inputBuffers, uint32_t n);
	void record(VkCommandBuffer& commandBuffer, uint32_t imageIndex);
};