/*
 * Sogang Univ, Graphics Lab, 2024
 * 
 * Abura Soba, 2025
 */

#pragma once

#include "vulkan/vulkan.h"
#include "VulkanUtils.h"

#include "cameraQuaternion.hpp"

#include <vector>

using namespace std;

template<typename>
inline constexpr bool dependent_false_v = false;

class Singleton {
public:
	static Singleton& GetInstance() {
		// Allocate with `new` in case Singleton is not trivially destructible.
		static Singleton instance;
		return instance;
	}

private:
	Singleton() = default;

	// Delete copy/move so extra instances can't be created/moved.
	Singleton(const Singleton&) = delete;
	Singleton& operator=(const Singleton&) = delete;
	Singleton(Singleton&&) = delete;
	Singleton& operator=(Singleton&&) = delete;
};

//TODO: change to singleton, use cpp file
class DebugManager{
public:
	static DebugManager& getInstance() {
		static DebugManager instance;
		return instance;
	}
private:
	DebugManager() = default;
	~DebugManager();
	DebugManager(const DebugManager&) = delete;
	DebugManager& operator=(const DebugManager&) = delete;
	DebugManager(DebugManager&&) = delete;
	DebugManager& operator=(DebugManager&&) = delete;
	PFN_vkSetDebugUtilsObjectNameEXT vkSetDebugUtilsObjectNameEXT{ nullptr };
	/*void setDebugNameImpl(VkDevice& device, VkObjectType objectType, uint64_t objectHandle, const char* name) {
		if (name) {
			VkDebugUtilsObjectNameInfoEXT nameInfo{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
			nameInfo.objectType = objectType;
			nameInfo.objectHandle = (uint64_t)objectHandle;
			nameInfo.pObjectName = name;
			vkSetDebugUtilsObjectNameEXT(device, &nameInfo);
		}
	}*/
	vks::VulkanDevice* vulkanDevice;
	VkDevice* device;

	/* capture image */
	VkQueue* queue;
	uint32_t width, height;
	vks::Buffer currentImgBuffer;
	void* currentImg;

	void printRayHitCounts(const vector<uint32_t>& hitCnts, uint32_t cnt);
	void saveGrayScaleImage(const std::vector<uint32_t>& data, string filename);
	void saveColorMapImage(const vector<uint32_t>& data, uint32_t maxHit, string filename);
	template <typename T>
	void writeCSVFile(vector<T>& vec, uint32_t stride, string& fileName);
	void captureSimilVarValidCnt(vks::Buffer& similVarValidCntBuffers);

public:
	void prepare(VkInstance instance, vks::VulkanDevice* device, VkQueue* queue, uint32_t width, uint32_t height);

	template <typename VulkanObjectType>
	void setDebugName(VulkanObjectType object, const char* name) {
		VkObjectType objectType;

#define IF_TYPE_THEN_ENUM(vkType, vkObjectTypeEnum) \
    if constexpr (std::is_same<VulkanObjectType, vkType>::value) objectType = vkObjectTypeEnum;

		IF_TYPE_THEN_ENUM(VkInstance, VK_OBJECT_TYPE_INSTANCE)
	else IF_TYPE_THEN_ENUM(VkPhysicalDevice, VK_OBJECT_TYPE_PHYSICAL_DEVICE)
			else IF_TYPE_THEN_ENUM(VkDevice, VK_OBJECT_TYPE_DEVICE)
			else IF_TYPE_THEN_ENUM(VkQueue, VK_OBJECT_TYPE_QUEUE)
			else IF_TYPE_THEN_ENUM(VkSemaphore, VK_OBJECT_TYPE_SEMAPHORE)
			else IF_TYPE_THEN_ENUM(VkCommandBuffer, VK_OBJECT_TYPE_COMMAND_BUFFER)
			else IF_TYPE_THEN_ENUM(VkFence, VK_OBJECT_TYPE_FENCE)
			else IF_TYPE_THEN_ENUM(VkDeviceMemory, VK_OBJECT_TYPE_DEVICE_MEMORY)
			else IF_TYPE_THEN_ENUM(VkBuffer, VK_OBJECT_TYPE_BUFFER)
			else IF_TYPE_THEN_ENUM(VkImage, VK_OBJECT_TYPE_IMAGE)
			else IF_TYPE_THEN_ENUM(VkEvent, VK_OBJECT_TYPE_EVENT)
			else IF_TYPE_THEN_ENUM(VkQueryPool, VK_OBJECT_TYPE_QUERY_POOL)
			else IF_TYPE_THEN_ENUM(VkBufferView, VK_OBJECT_TYPE_BUFFER_VIEW)
			else IF_TYPE_THEN_ENUM(VkImageView, VK_OBJECT_TYPE_IMAGE_VIEW)
			else IF_TYPE_THEN_ENUM(VkShaderModule, VK_OBJECT_TYPE_SHADER_MODULE)
			else IF_TYPE_THEN_ENUM(VkPipelineCache, VK_OBJECT_TYPE_PIPELINE_CACHE)
			else IF_TYPE_THEN_ENUM(VkPipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT)
			else IF_TYPE_THEN_ENUM(VkRenderPass, VK_OBJECT_TYPE_RENDER_PASS)
			else IF_TYPE_THEN_ENUM(VkPipeline, VK_OBJECT_TYPE_PIPELINE)
			else IF_TYPE_THEN_ENUM(VkDescriptorSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT)
			else IF_TYPE_THEN_ENUM(VkSampler, VK_OBJECT_TYPE_SAMPLER)
			else IF_TYPE_THEN_ENUM(VkDescriptorPool, VK_OBJECT_TYPE_DESCRIPTOR_POOL)
			else IF_TYPE_THEN_ENUM(VkDescriptorSet, VK_OBJECT_TYPE_DESCRIPTOR_SET)
			else IF_TYPE_THEN_ENUM(VkFramebuffer, VK_OBJECT_TYPE_FRAMEBUFFER)
			else IF_TYPE_THEN_ENUM(VkCommandPool, VK_OBJECT_TYPE_COMMAND_POOL)
			else IF_TYPE_THEN_ENUM(VkDescriptorUpdateTemplate, VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE)
			else IF_TYPE_THEN_ENUM(VkSurfaceKHR, VK_OBJECT_TYPE_SURFACE_KHR)
			else IF_TYPE_THEN_ENUM(VkSwapchainKHR, VK_OBJECT_TYPE_SWAPCHAIN_KHR)
			else IF_TYPE_THEN_ENUM(VkAccelerationStructureKHR, VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR)
			else IF_TYPE_THEN_ENUM(VkDebugUtilsMessengerEXT, VK_OBJECT_TYPE_DEBUG_UTILS_MESSENGER_EXT)
			else static_assert(dependent_false_v<VulkanObjectType>, "Unknown Vulkan object type");
#undef IF_TYPE_THEN_ENUM

			/*void setDebugNameImpl(VkDevice& device, VkObjectType objectType, uint64_t objectHandle, const char* name);
			setDebugNameImpl(device, objectType, (uint64_t)object, name);*/

			if (name) {
				VkDebugUtilsObjectNameInfoEXT nameInfo{ VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT };
				nameInfo.objectType = objectType;
				nameInfo.objectHandle = (uint64_t)object;
				nameInfo.pObjectName = name;
				vkSetDebugUtilsObjectNameEXT(*device, &nameInfo);
			}
	}

	void captureImage(VkImage& image);
	void captureRTMask(vks::Buffer& rtMask);
	void captureHitCnt(vks::Buffer& hitCountsBuffer);
	void captureRenderingImages(VkImage& image, QuaternionCamera& quaternionCamera, uint32_t camIdx);
	void captureSimilVarBuffers(vks::Buffer& particleIdBuffer, vks::Buffer& alphaBuffer, vks::Buffer& weightBuffer, vks::Buffer& depthBuffer, vks::Buffer& similVarValidCntBuffers, vks::Buffer& finalTransmittanceBuffers);
	void dumpParticles(vks::Buffer& densitiesBuffer, uint32_t densitiesCnt);
	void dumpIcosahedron(vks::Buffer& verticesBuffer, vks::Buffer& indicesBuffer, uint32_t verticesCnt, uint32_t indicesCnt);
};