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

	struct PushConstants {
		uint32_t width;
		uint32_t height;
		uint32_t additionalRT;
		float colorThreshold;
		float hitThreshold;
		float weightThreshold;
	}pushConstants;

#if SIMILARITY_VAR
	struct SimilarityVar {
		float t;
		//float galpha;	// maybe weight?
	};
	vector<vks::Buffer> particleIdBuffers;
	vector<vks::Buffer> alphaBuffers;
	vector<vks::Buffer> weightBuffers;
	vector<vks::Buffer> depthBuffers;
	vector<vks::Buffer> similVarValidCntBuffers;
	vector<vks::Buffer> similarityVarBuffers;
#endif
	vector<vks::Buffer> finalTransmittanceBuffers;

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

	/* RT API */
	PFN_vkCmdTraceRaysKHR vkCmdTraceRaysKHR;
	PFN_vkCreateRayTracingPipelinesKHR vkCreateRayTracingPipelinesKHR;
	/* SBT */
	VkStridedDeviceAddressRegionKHR* raygen;
	VkStridedDeviceAddressRegionKHR* miss;
	VkStridedDeviceAddressRegionKHR* hit;

	inline string getShaderPath(string shaderName);
	void createSimilarityVarBuffers();
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
	void initDescriptorSet(int frameIdx, VulkanSwapChain& swapChain, VkAccelerationStructureKHR& tlasHandle, vks::Buffer& uniformBuffer, vks::Buffer& uniformBufferStatic, vks::Buffer& particleDensities, vks::Buffer& particleSphCoefficients
#if ENABLE_HIT_COUNTS || SIMILARITY_VAR
		, vks::Buffer& hitCountsbuffer
#endif
//TODO: use member rtMaskBuffers
#if UNDERSAMPLING && STATISTICS
		, vks::Buffer& rtMaskBuffer
#endif
	);
	void initShaderBindingTable(VkStridedDeviceAddressRegionKHR* raygen, VkStridedDeviceAddressRegionKHR* miss, VkStridedDeviceAddressRegionKHR* hit);
	void record(VkCommandBuffer& commandBuffer, uint32_t imageIndex, uint32_t additionalRTFlag);
	void updateColorThreshold(float threshold);
	void updateHitThreshold(float threshold);
	void updateWeightThreshold(float threshold);

	void captureSimilVarBuffers(uint32_t idx);
	void captureValidCntBuffer(uint32_t idx);
};