/*
 * Sogang Univ, Graphics Lab, 2024
 *
 * Abura Soba, 2025
 */

#include "RTPipeline.hpp"
#include "Define.h"

#include <algorithm>

using namespace std;

// value 1024 should change if local group size change
RTPipeline::RTPipeline(vks::VulkanDevice& device, VkQueue& queue, int swapchainImageCnt, string projectPath) : vulkanDevice(device), device(device.logicalDevice), queue(queue) {
	this->swapchainImageCnt = swapchainImageCnt;
	this->projectPath = projectPath;
	descriptorSets.resize(swapchainImageCnt);
	rtMaskBuffers.resize(swapchainImageCnt);
	particleIdBuffers.resize(swapchainImageCnt);
	similarityVarBuffers.resize(swapchainImageCnt);

	vkCmdTraceRaysKHR = reinterpret_cast<PFN_vkCmdTraceRaysKHR>(vkGetDeviceProcAddr(device, "vkCmdTraceRaysKHR"));
	vkCreateRayTracingPipelinesKHR = reinterpret_cast<PFN_vkCreateRayTracingPipelinesKHR>(vkGetDeviceProcAddr(device, "vkCreateRayTracingPipelinesKHR"));
}

RTPipeline::~RTPipeline() {
	vkDestroyPipeline(device, pipeline, nullptr);
	vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
	vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
	if (descriptorPool != VK_NULL_HANDLE) {
		vkDestroyDescriptorPool(device, descriptorPool, nullptr);
	}

	for (auto& shaderModule : shaderModules) {
		vkDestroyShaderModule(device, shaderModule, nullptr);
	}

	for (int i = 0; i < swapchainImageCnt; i++) {
		rtMaskBuffers[i].destroy();
		particleIdBuffers[i].destroy();
		similarityVarBuffers[i].destroy();
	}
}

/* Private Functions */
inline string RTPipeline::getShaderPath(string shaderName) {
	return "./../shaders/glsl/" + projectPath + shaderName;
}

//void

void RTPipeline::createSimilarityVarBuffers() {
	for (int i = 0; i < swapchainImageCnt; i++) {
		// particle id
		VK_CHECK_RESULT(vulkanDevice.createBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &particleIdBuffers[i], pushConstants.width * pushConstants.height * sizeof(uint32_t) * MAX_SIMILARITY_VAR));
		string bufferName = "particleIdBuffer" + to_string(i);
		debugManager.setDebugName(particleIdBuffers[i].buffer, bufferName.c_str());
		// t, galpha
		VK_CHECK_RESULT(vulkanDevice.createBuffer(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &similarityVarBuffers[i], pushConstants.width * pushConstants.height * sizeof(float) * MAX_SIMILARITY_VAR));
		bufferName = "tBuffer" + to_string(i);
		debugManager.setDebugName(similarityVarBuffers[i].buffer, bufferName.c_str());
	}
}

void RTPipeline::createDescriptorSets() {
	std::vector<VkDescriptorPoolSize> poolSizes = {
		// ray tracing pipeline
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1 * swapchainImageCnt),
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1 * swapchainImageCnt),
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2 * swapchainImageCnt),
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 * swapchainImageCnt),
#if SPLIT_BLAS && !RAY_QUERY
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 * swapchainImageCnt),
#endif 
#if ENABLE_HIT_COUNTS && !RAY_QUERY
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 * swapchainImageCnt),
#endif
#if UNDERSAMPLING
	#if STATISTICS
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1 * swapchainImageCnt),
	#endif
		vks::initializers::descriptorPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2 * swapchainImageCnt),
#endif
	};
	VkDescriptorPoolCreateInfo descriptorPoolCreateInfo = vks::initializers::descriptorPoolCreateInfo(poolSizes, swapchainImageCnt); // ray tracing pipeline

	VK_CHECK_RESULT(vkCreateDescriptorPool(device, &descriptorPoolCreateInfo, nullptr, &descriptorPool));	// descriptor pool

	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings = {
		// Binding 0: Top level acceleration structure
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0),
		// Binding 1: Ray tracing result image
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 1),
		// Binding 2: Uniform buffer Dynamic
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 2),
		// Binding 3: Uniform buffer Static
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 3),
		// Binding 4: Storage buffer - Particle Densities
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 4),
		// Binding 5: Storage buffer - Particle Sph Coefficients
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 5),
#if SPLIT_BLAS && !RAY_QUERY
		// Binding 6: Storage buffer - primitive Id
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_ANY_HIT_BIT_KHR, 6),
#endif
#if ENABLE_HIT_COUNTS && !RAY_QUERY
		// Binding 7: Storage buffer - Ray Hit Count for debugging
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 7),
#endif
#if UNDERSAMPLING 
	#if STATISTICS
		// Binding 8: Storage buffer - RT mask
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 8),
	#endif
		// Binding 9: Storage buffer - Particle ID
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 9),
		// Binding 10: Storage buffer - Similarity Var
		vks::initializers::descriptorSetLayoutBinding(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 10),
#endif
	};

	VkDescriptorSetLayoutCreateInfo descriptorSetLayoutCI = vks::initializers::descriptorSetLayoutCreateInfo(setLayoutBindings);
	VK_CHECK_RESULT(vkCreateDescriptorSetLayout(device, &descriptorSetLayoutCI, nullptr, &descriptorSetLayout));

	for (int i = 0; i < swapchainImageCnt; i++)
	{
		VkDescriptorSet& descriptorSet = descriptorSets[i];
		VkDescriptorSetAllocateInfo descriptorSetAllocateInfo = vks::initializers::descriptorSetAllocateInfo(descriptorPool, &descriptorSetLayout, 1);
		VK_CHECK_RESULT(vkAllocateDescriptorSets(device, &descriptorSetAllocateInfo, &descriptorSet));	// descriptor set
	}
}

void RTPipeline::createPipelineLayout() {
	VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo = vks::initializers::pipelineLayoutCreateInfo(&descriptorSetLayout, 1);
	// original push constant
	//VkPushConstantRange pushConstantRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_RAYGEN_BIT_KHR, sizeof(pushConstants), 0);
	//pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	//pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;
	// undersampling flag
	VkPushConstantRange pushConstantRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_RAYGEN_BIT_KHR, sizeof(PushConstants), 0);
	pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
	pipelineLayoutCreateInfo.pPushConstantRanges = &pushConstantRange;

	VK_CHECK_RESULT(vkCreatePipelineLayout(device, &pipelineLayoutCreateInfo, nullptr, &pipelineLayout));
}

void RTPipeline::createPipeline() {
	// For transfer of num of lights, use specialization constant.
	struct SpecializationData {
		uint32_t numOfLights = NUM_OF_LIGHTS_SUPPORTED;
		uint32_t numOfDynamicLights = NUM_OF_DYNAMIC_LIGHTS;
		uint32_t numOfStaticLights = NUM_OF_STATIC_LIGHTS;
		uint32_t staticLightOffset = STATIC_LIGHT_OFFSET;
	} specializationData;

	std::vector<VkSpecializationMapEntry> specializationMapEntries = {
		vks::initializers::specializationMapEntry(0, 0, sizeof(uint32_t)),
		vks::initializers::specializationMapEntry(1, sizeof(uint32_t), sizeof(uint32_t)),
		vks::initializers::specializationMapEntry(2, sizeof(uint32_t) * 2, sizeof(uint32_t)),
		vks::initializers::specializationMapEntry(3, sizeof(uint32_t) * 3, sizeof(uint32_t)),
	};
	VkSpecializationInfo specializationInfo = vks::initializers::specializationInfo(static_cast<uint32_t>(specializationMapEntries.size()), specializationMapEntries.data(), sizeof(SpecializationData), &specializationData);

	/*
		Setup ray tracing shader groups
	*/
	vector<VkPipelineShaderStageCreateInfo> shaderStages;

	// Ray generation group
	{
		string shaderPath = getShaderPath("raygen.rgen.spv");
		shaderStages.push_back(vks::utils::createShaderStageCI(device, shaderPath, VK_SHADER_STAGE_RAYGEN_BIT_KHR));
		shaderStages[shaderStages.size() - 1].pSpecializationInfo = &specializationInfo;
		VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
		shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
		shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
		shaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
		shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
		shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
		shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
		shaderGroups.push_back(shaderGroup);
	}

	// Miss group
	{
		string shaderPath = getShaderPath("miss.rmiss.spv");
		shaderStages.push_back(vks::utils::createShaderStageCI(device, shaderPath, VK_SHADER_STAGE_MISS_BIT_KHR));
		VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
		shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
		shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
		shaderGroup.generalShader = static_cast<uint32_t>(shaderStages.size()) - 1;
		shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
		shaderGroup.anyHitShader = VK_SHADER_UNUSED_KHR;
		shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
		shaderGroups.push_back(shaderGroup);
	}

	// Hit group
	{
		string shaderPath = getShaderPath("anyhit.rahit.spv");
		shaderStages.push_back(vks::utils::createShaderStageCI(device, shaderPath, VK_SHADER_STAGE_ANY_HIT_BIT_KHR));
		VkRayTracingShaderGroupCreateInfoKHR shaderGroup{};
		shaderGroup.sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR;
		shaderGroup.type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
		shaderGroup.generalShader = VK_SHADER_UNUSED_KHR;
		shaderGroup.closestHitShader = VK_SHADER_UNUSED_KHR;
		shaderGroup.intersectionShader = VK_SHADER_UNUSED_KHR;
		shaderGroup.anyHitShader = static_cast<uint32_t>(shaderStages.size()) - 1;
		shaderGroups.push_back(shaderGroup);
	}
	/*
		Create the ray tracing pipeline
	*/
	VkRayTracingPipelineCreateInfoKHR rayTracingPipelineCI{};
	rayTracingPipelineCI.sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR;
	rayTracingPipelineCI.stageCount = static_cast<uint32_t>(shaderStages.size());
	rayTracingPipelineCI.pStages = shaderStages.data();
	rayTracingPipelineCI.groupCount = static_cast<uint32_t>(shaderGroups.size());
	rayTracingPipelineCI.pGroups = shaderGroups.data();
	rayTracingPipelineCI.maxPipelineRayRecursionDepth = 1;
	rayTracingPipelineCI.layout = pipelineLayout;
	VK_CHECK_RESULT(vkCreateRayTracingPipelinesKHR(device, VK_NULL_HANDLE, VK_NULL_HANDLE, 1, &rayTracingPipelineCI, nullptr, &pipeline));
}

/* Public Functions */
void RTPipeline::prepare(uint32_t width, uint32_t height) {
	pushConstants.width = width;
	pushConstants.height = height;
#if UNDERSAMPLING
	createSimilarityVarBuffers();
#endif
	createDescriptorSets();
	createPipelineLayout();
	pushConstantRange = vks::initializers::pushConstantRange(VK_SHADER_STAGE_COMPUTE_BIT, sizeof(PushConstants), 0);
	createPipeline();
}

// init descriptor set of frameIdx
// need to init each frame seperately
void RTPipeline::initDescriptorSet(int frameIdx, VulkanSwapChain& swapChain, VkAccelerationStructureKHR& tlasHandle, vks::Buffer& uniformBuffer, vks::Buffer& uniformBufferStatic, vks::Buffer& particleDensities, vks::Buffer& particleSphCoefficients
#if ENABLE_HIT_COUNTS && !RAY_QUERY
	, vks::Buffer& hitCountsbuffer
#endif
#if UNDERSAMPLING && STATISTICS
	,vks::Buffer& rtMaskBuffer
#endif
) {
	VkDescriptorSet& descriptorSet = descriptorSets[frameIdx];
	// WriteDescriptorSet for TLAS (binding0)
	VkWriteDescriptorSetAccelerationStructureKHR descriptorAccelerationStructureInfo = vks::initializers::writeDescriptorSetAccelerationStructureKHR();
	descriptorAccelerationStructureInfo.accelerationStructureCount = 1;
#if SPLIT_BLAS && !RAY_QUERY
	descriptorAccelerationStructureInfo.pAccelerationStructures = &splitBLAS.splittedTLAS.handle;
#else
	descriptorAccelerationStructureInfo.pAccelerationStructures = &tlasHandle;
#endif

	VkWriteDescriptorSet accelerationStructureWrite{};
	accelerationStructureWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	// The specialized acceleration structure descriptor has to be chained
	accelerationStructureWrite.pNext = &descriptorAccelerationStructureInfo;
	accelerationStructureWrite.dstSet = descriptorSet;
	accelerationStructureWrite.dstBinding = 0;
	accelerationStructureWrite.descriptorCount = 1;
	accelerationStructureWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;

	VkDescriptorImageInfo storageImageDescriptor = { VK_NULL_HANDLE, swapChain.buffers[frameIdx].view, VK_IMAGE_LAYOUT_GENERAL };

	std::vector<VkWriteDescriptorSet> writeDescriptorSets = {
		// Binding 0: Top level acceleration structure
		accelerationStructureWrite,
		// Binding 1: Ray tracing result image
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, &storageImageDescriptor),
		// Binding 2: Uniform data Dynamic
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2, &uniformBuffer.descriptor),
		// Binding 3: Uniform data Static
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3, &uniformBufferStatic.descriptor),
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4, &particleDensities.descriptor),
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 5, &particleSphCoefficients.descriptor),
#if SPLIT_BLAS && !RAY_QUERY
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 6, &splitBLAS.d_splittedPrimitiveIdsDeviceAddress.descriptor),
#endif
#if ENABLE_HIT_COUNTS && !RAY_QUERY
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 7, &hitCountsbuffer.descriptor),
#endif
#if UNDERSAMPLING
	#if STATISTICS
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 8, &rtMaskBuffer.descriptor),
	#endif
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 9, &particleIdBuffers[frameIdx].descriptor),
		vks::initializers::writeDescriptorSet(descriptorSet, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10, &similarityVarBuffers[frameIdx].descriptor),
#endif
	};

	vkUpdateDescriptorSets(device, static_cast<uint32_t>(writeDescriptorSets.size()), writeDescriptorSets.data(), 0, VK_NULL_HANDLE);
}

void RTPipeline::initShaderBindingTable(VkStridedDeviceAddressRegionKHR* raygen, VkStridedDeviceAddressRegionKHR* miss, VkStridedDeviceAddressRegionKHR* hit) {
	this->raygen = raygen;
	this->miss = miss;
	this->hit = hit;
}

void RTPipeline::record(VkCommandBuffer& commandBuffer, uint32_t imageIndex, uint32_t additionalRTFlag) {
#if UNDERSAMPLING
	if (additionalRTFlag) {
		VkMemoryBarrier barrier = vks::initializers::memoryBarrier();
		barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
			VK_FLAGS_NONE, 1, &barrier, 0, nullptr, 0, nullptr);
	}
	else if (additionalRTFlag == 0) {
		vkCmdFillBuffer(commandBuffer, particleIdBuffers[imageIndex].buffer, 0, particleIdBuffers[imageIndex].size, 0);
		vkCmdFillBuffer(commandBuffer, similarityVarBuffers[imageIndex].buffer, 0, similarityVarBuffers[imageIndex].size, 0);
		VkMemoryBarrier barrier = vks::initializers::memoryBarrier();
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
		vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
			VK_FLAGS_NONE, 1, &barrier, 0, nullptr, 0, nullptr);
	}
#endif

	pushConstants.additionalRT = additionalRTFlag;

	uint32_t localWidth;
	uint32_t localHeight;
#if UNDERSAMPLING
	if (additionalRTFlag < 2) {
		localWidth = pushConstants.width / 2;
		localHeight = pushConstants.height / 2;
	}
	else {
		localWidth = pushConstants.width;
		localHeight = pushConstants.height / 2;
	}
#else
	localWidth = pushConstants.width;
	localHeight = pushConstants.height;
#endif
	
	vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipeline);
	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, pipelineLayout, 0, 1, &descriptorSets[imageIndex], 0, 0);
#if UNDERSAMPLING
	vkCmdPushConstants(commandBuffer, pipelineLayout, VK_SHADER_STAGE_RAYGEN_BIT_KHR, 0, sizeof(PushConstants), &pushConstants);
#endif
	VkStridedDeviceAddressRegionKHR emptySbtEntry{};
	vkCmdTraceRaysKHR(
		commandBuffer,
		raygen,
		miss,
		hit,
		&emptySbtEntry,
		localWidth,
		localHeight,
		1);
}

void RTPipeline::updateColorThreshold(float threshold) {
	pushConstants.colorThreshold = threshold;
}

void RTPipeline::initDebugManager(VkInstance instance) {
	debugManager.prepare(instance, &device);
}