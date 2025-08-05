/*
 * Sogang Univ, Graphics Lab, 2024
 *
 * Abura Soba, 2025
 */

#include "DebugManager.hpp"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <iostream>

using namespace std;

DebugManager::~DebugManager() {
	currentImgBuffer.destroy();
	free(currentImg);
}

void DebugManager::prepare(VkInstance instance, vks::VulkanDevice* device, VkQueue* queue, uint32_t width, uint32_t height) {
	vkSetDebugUtilsObjectNameEXT = reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT"));
	this->vulkanDevice = device;
	this->device = &device->logicalDevice;
	this->queue = queue;
	this->width = width;
	this->height = height;
	currentImg = (void*)malloc(width * height * 4);
	VK_CHECK_RESULT(vulkanDevice->createBuffer(VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &currentImgBuffer, width * height * 4, nullptr));
}

void DebugManager::captureImage(VkImage image) {
	static uint32_t cnt = 0;
	vkQueueWaitIdle(*queue);

	vulkanDevice->copyImageToBuffer(image, currentImgBuffer, *queue, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, width, height);

	currentImgBuffer.map();
	memcpy(currentImg, currentImgBuffer.mapped, width * height * 4);
	currentImgBuffer.unmap();
	int stride = width * 4;
	std::string fileName = DEBUG_FILE_PATH + string("images/rederingImage") + std::to_string(cnt) + ".png";
	stbi_write_png(fileName.c_str(), width, height, 4, currentImg, stride);
	cout << "Rendering Image Capture Done\n";
	cnt++;
}

void DebugManager::captureRTMask(vks::Buffer rtMaskBuffer) {
	static uint32_t cnt = 0;
	vector<uint32_t> rtMask(width * height);
	vkQueueWaitIdle(*queue);
	// current RTMask is host visible
	//vulkanDevice->copyDeviceBufferToHost(rtMask.data(), rtMaskBuffer, *queue);
	rtMaskBuffer.map();
	memcpy(rtMask.data(), rtMaskBuffer.mapped, rtMaskBuffer.size);
	rtMaskBuffer.unmap();
	vector<uint8_t> grayscaleData(width * height);
	for (int i = 0; i < width * height; i++) {
		grayscaleData[i] = (rtMask[i] == 1) ? 255 : 0;
		if (i % 1920 % 2 == 0 || i / 1920 % 2 == 0) {
			grayscaleData[i] = 0;
		}
	}
	string filename = DEBUG_FILE_PATH + string("rtMasks/rtMask") + to_string(cnt) + ".png";
	stbi_write_png(filename.c_str(), width, height, 1, grayscaleData.data(), width);
	cout << "RTMask Capture Done\n";
	cnt++;
}