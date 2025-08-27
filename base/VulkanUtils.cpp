#include "VulkanUtils.h"

namespace vks {
	namespace utils {
		void updateUniformBufferStatic(UniformDataStatic& params, BaseFrameObject& currentFrame, vks::VulkanDevice* vulkanDevice, VkQueue queue) {
			// mapping
			vks::Buffer stagingBuffer;
			stagingBuffer.size = sizeof(UniformDataStatic);
			VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer.size, &stagingBuffer.buffer, &stagingBuffer.memory, nullptr));

			void* data;
			vkMapMemory(vulkanDevice->logicalDevice, stagingBuffer.memory, 0, sizeof(UniformDataStatic), 0, &data);
			memcpy(data, (void*)&params, sizeof(params));
			vkUnmapMemory(vulkanDevice->logicalDevice, stagingBuffer.memory);

			VkBufferCopy copyRegion;
			copyRegion.srcOffset = 0;
			copyRegion.dstOffset = 0;
			copyRegion.size = stagingBuffer.size;
			vulkanDevice->copyBuffer(&stagingBuffer, &currentFrame.uniformBufferStatic, queue, &copyRegion);

			vkDestroyBuffer(vulkanDevice->logicalDevice, stagingBuffer.buffer, nullptr);
			vkFreeMemory(vulkanDevice->logicalDevice, stagingBuffer.memory, nullptr);
		}

		VkPipelineShaderStageCreateInfo createShaderStageCI(VkDevice& device, std::string fileName, VkShaderStageFlagBits stage)
		{
			VkPipelineShaderStageCreateInfo shaderStage = {};
			shaderStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			shaderStage.stage = stage;
#if defined(VK_USE_PLATFORM_ANDROID_KHR)
			shaderStage.module = vks::tools::loadShader(androidApp->activity->assetManager, fileName.c_str(), device);
#else
			shaderStage.module = vks::tools::loadShader(fileName.c_str(), device);
#endif
			shaderStage.pName = "main";
			assert(shaderStage.module != VK_NULL_HANDLE);
			return shaderStage;
		}
	}
}

