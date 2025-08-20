/*
 * Sogang Univ, Graphics Lab, 2024
 *
 * Abura Soba, 2025
 */

#include "DebugManager.hpp"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "camera.hpp"

#include <iostream>
#include <algorithm>
#include <filesystem>

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

void DebugManager::setModel(vk3DGRT::Model& gModel) {
	this->gModel = &gModel;
}

//// Print the ray hit count of each pixel of last frame to the txt file.
//void DebugManager::printRayHitCounts(const vector<uint32_t>& hitCnts, uint32_t cnt) {
//	string filename = DEBUG_FILE_PATH + string("hitCounts/text") + to_string(cnt) + ".txt";
//	FILE* fp = fopen(filename.c_str(), "w");
//	if (fp) {
//		for (size_t i = 0; i < height; ++i) {
//			for (size_t j = 0; j < width; ++j) {
//				fprintf(fp, "%u\t", hitCnts[i * width + j]);
//			}
//			fprintf(fp, "\n");
//		}
//		fclose(fp);
//	}
//	cout << "Writing " + filename + " done\n";
//}

void DebugManager::printRayHitCounts(const std::vector<uint32_t>& hitCnts, uint32_t cnt) {
	std::string filename = DEBUG_FILE_PATH + std::string("hitCounts/text") + std::to_string(cnt) + ".txt";
	std::ofstream ofs(filename);
	if (!ofs) {
		std::cerr << "Failed to open " << filename << "\n";
		return;
	}

	for (size_t i = 0; i < height; ++i) {
		for (size_t j = 0; j < width; ++j) {
			ofs << hitCnts[i * width + j];
			if (j + 1 < width) ofs << "\t";
		}
		ofs << "\n";
	}

	ofs.close();
	std::cout << "Writing " << filename << " done\n";
}

void DebugManager::saveGrayScaleImage(const std::vector<uint32_t>& data, string filename) {
	std::vector<uint8_t> grayscaleData(width * height);

	for (int i = 0; i < width * height; ++i) {
		grayscaleData[i] = static_cast<uint8_t>(data[i] & 0xFF);
	}
	
	stbi_write_png(filename.c_str(), width, height, 1, grayscaleData.data(), width);
	cout << "Writing " + filename + " done\n";
}

void DebugManager::saveColorMapImage(const vector<uint32_t>& data, uint32_t maxHit, string filename) {
	vector<uint32_t> colormapData(width * height);

	float norm;
	for (int i = 0; i < width * height; i++) {
		norm = (float)data[i] / maxHit;
		if (norm == 0) {
			colormapData[i] = 0xFF000000;
		}
		else if (norm > 0 && norm <= 0.2) {
			colormapData[i] = 0xFFFF0000;
		}
		else if (norm > 0.2 && norm <= 0.4) {
			colormapData[i] = 0xFFFFFF00;
		}
		else if (norm > 0.4 && norm <= 0.6) {
			colormapData[i] = 0xFF00FFFF;
		}
		else if (norm > 0.6 && norm <= 0.8) {
			colormapData[i] = 0xFF00A5FF;
		}
		else if (norm > 0.8 && norm <= 1.0) {
			colormapData[i] = 0xFF0000FF;
		}
	}
	int stride = width * 4;
	stbi_write_png(filename.c_str(), width, height, 4, colormapData.data(), stride);
	cout << "Writing " + filename + " done\n";
}

void DebugManager::captureHitCnt(vks::Buffer& hitCountsBuffer) {
	static uint32_t cnt = 0;
	vkQueueWaitIdle(*queue);

	vector<uint32_t> hitCnts(width * height);
	vulkanDevice->copyDeviceBufferToHost(hitCnts.data(), hitCountsBuffer, *queue);
	auto maxHit = std::max_element(hitCnts.begin(), hitCnts.end());
	uint32_t totalHit = 0;
	uint32_t zeroCnt = 0;
	for (int val : hitCnts) {
		if (val != 0) {
			totalHit += val;
			zeroCnt++;
		}
	}
	float avgHit = (float)totalHit / zeroCnt;
	std::cout << "max hit : " << *maxHit << "\n";
	std::cout << "totalHit : " << totalHit << "\n";
	std::cout << "Average hit (ignore zero) : " << avgHit << "\n";
	saveGrayScaleImage(hitCnts, DEBUG_FILE_PATH + string("hitCounts/grayscale") + to_string(cnt) + ".png");
	saveColorMapImage(hitCnts, *maxHit, DEBUG_FILE_PATH + string("hitCounts/colormap") + std::to_string(cnt) + ".png");
	printRayHitCounts(hitCnts, cnt);
	std::cout << "*** Ray hit counts END ***\n\n";
	cnt++;
}

void DebugManager::captureImage(VkImage& image) {
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

void DebugManager::captureRTMask(vks::Buffer& rtMaskBuffer) {
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

void DebugManager::captureRenderingImages(VkImage& image, QuaternionCamera& quaternionCamera, uint32_t camIdx, uint32_t evalQualityDirNum) {
	vkQueueWaitIdle(*queue);

	vulkanDevice->copyImageToBuffer(image, currentImgBuffer, *queue, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, width, height);
	vkQueueWaitIdle(*queue);

	currentImgBuffer.map();
	memcpy(currentImg, currentImgBuffer.mapped, width * height * 4);
	currentImgBuffer.unmap();

	int stride = width * 4;
	string outputDir = "../results/evaluations/outputs/output" + to_string(evalQualityDirNum) + "/";
	if (!filesystem::exists(outputDir)) {
		try {
			filesystem::create_directories(outputDir);
			cout << "create folder : " << outputDir << "\n";
		}
		catch (const std::exception& e) {
			cerr << "cannot create folder : " << e.what() << "\n";
			return;
		}
	}
	
	std::string fileName = outputDir + "r_" + std::to_string(camIdx) + ".png";
	stbi_write_png(fileName.c_str(), width, height, 4, currentImg, stride);

	std::cout << "\t- Camera index " << camIdx << " is completed.\n";
#if QUATERNION_CAMERA
	quaternionCamera.setNextCamera();
#else
	camera.setDatasetCamera(camera.dataType, evalCameraIdx, (float)width / height);
#endif
}

template<typename T>
void DebugManager::writeCSVFile(vector<T>& vec, uint32_t stride, string& fileName) {
	string output;
	output.reserve(width * height * stride * 5);
	for (int i = 0; i < vec.size();) {
		for (int j = 0; j < stride; j++) {
			output.append(to_string(vec[i]));
			output.push_back(',');
			i++;
		}
		output.pop_back();
		output.push_back('\n');
	}

	ofstream of(fileName);
	if (of.is_open()) {
		of << output;
		of.close();
		cout << "Write " << fileName << " done\n";
	}
	else {
		cout << "Failed file open : " << fileName << "\n";
	}
}

void DebugManager::captureValidHitParticles(vector<uint32_t>& particleIds) {
	static uint32_t cnt = 0;
	cout << "dumping obj file\n";
	vkQueueWaitIdle(*queue);
	
	vector<float> vertices(gModel->vertices.count);
	vector<uint32_t> indices(gModel->indices.count);
	string filename = DEBUG_FILE_PATH + string("/similVars/validParticle") + to_string(cnt) + ".obj";
	uint32_t offset;
	
	vulkanDevice->copyDeviceBufferToHost(vertices.data(), gModel->vertices.storageBuffer, *queue);
	vulkanDevice->copyDeviceBufferToHost(indices.data(), gModel->indices.storageBuffer, *queue);
	vkQueueWaitIdle(*queue);

	ofstream of(filename);
	if (of.is_open()) {
		for (int i = 0; i < vertices.size(); i += 3) {
			of << "v " << vertices[i] << " " << vertices[i + 1] << " " << vertices[i + 2] << "\n";
		}
		for (int t = 0; t < particleIds.size(); t++) {
			for (int i = 0; i < 60; i += 3) {
				offset = (particleIds[t] - 1) * 3;
				of << "f " << indices[offset + i] + 1 << " " << indices[offset + i + 1] + 1 << " " << indices[offset + i + 2] + 1 << "\n";
			}
		}
		of.close();
		cout << "Write " << filename << " done\n";
		cnt++;
	}
	else {
		cout << "Failed file open : " << filename << "\n";
	}
}

void DebugManager::captureSimilVarValidCnt(vks::Buffer& similVarValidCntBuffers) {
	static uint32_t cnt = 0;
	vector<uint32_t> validCnt(width * height);
	vulkanDevice->copyDeviceBufferToHost(validCnt.data(), similVarValidCntBuffers, *queue);

	auto maxHit = std::max_element(validCnt.begin(), validCnt.end());
	vector<uint32_t> maxIndices;
	vector<uint32_t> validHitIndices;
	uint32_t totalHit = 0;
	uint32_t nonZeroCnt = 0;
	for (int i = 0; i < validCnt.size(); i++) {
		if (validCnt[i] != 0) {
			totalHit += validCnt[i];
			nonZeroCnt++;
			validHitIndices.push_back(i);
		}
		if (validCnt[i] == *maxHit) {
			maxIndices.push_back(i + 1);
		}
	}
	captureValidHitParticles(validHitIndices);
	float avgHit = (float)totalHit / nonZeroCnt;
	std::cout << "max hit : " << *maxHit << "\n";
	cout << "max hit count : " << maxIndices.size() << "\n";
	cout << "max hit idx : ";
	for (int i = 0; i < maxIndices.size(); i++) {
		cout << maxIndices[i] << " ";
	}
	cout << "\n";
	std::cout << "totalHit : " << totalHit << "\n";
	std::cout << "Average hit (ignore zero) : " << avgHit << "\n";
	string filepath = DEBUG_FILE_PATH + string("similVars/");
	saveGrayScaleImage(validCnt, filepath + "validCntGrayscale" + to_string(cnt) + ".png");
	saveColorMapImage(validCnt, *maxHit,  filepath + string("validCntcolormap") + std::to_string(cnt) + ".png");
	std::cout << "*** Ray hit counts END ***\n\n";
	cnt++;
}

void DebugManager::captureSimilVarBuffers(vks::Buffer& particleIdBuffer, vks::Buffer& alphaBuffer, vks::Buffer& weightBuffer, vks::Buffer& depthBuffer, vks::Buffer& similVarValidCntBuffer, vks::Buffer& finalTransmittanceBuffer) {
	vector<uint32_t> particleIdVec(width * height * MAX_SIMILARITY_VAR);
	vector<float> floatVec(width * height * MAX_SIMILARITY_VAR);
	
	//particleId Buffer
	vulkanDevice->copyDeviceBufferToHost(particleIdVec.data(), particleIdBuffer, *queue);
	string fileName = DEBUG_FILE_PATH + string("similVars/") + "particleId.csv";
	writeCSVFile(particleIdVec, MAX_SIMILARITY_VAR, fileName);
	//alpha Buffer
	vulkanDevice->copyDeviceBufferToHost(floatVec.data(), alphaBuffer, *queue);
	fileName = DEBUG_FILE_PATH + string("similVars/") + "alpha.csv";
	writeCSVFile(floatVec, MAX_SIMILARITY_VAR, fileName);
	//weight Buffer
	vulkanDevice->copyDeviceBufferToHost(floatVec.data(), weightBuffer, *queue);
	fileName = DEBUG_FILE_PATH + string("similVars/") + "weight.csv";
	writeCSVFile(floatVec, MAX_SIMILARITY_VAR, fileName);
	//depth Buffer
	vulkanDevice->copyDeviceBufferToHost(floatVec.data(), depthBuffer, *queue);
	fileName = DEBUG_FILE_PATH + string("similVars/") + "depth.csv";
	writeCSVFile(floatVec, MAX_SIMILARITY_VAR, fileName);
	//final transmittance Buffer
	vulkanDevice->copyDeviceBufferToHost(floatVec.data(), finalTransmittanceBuffer, *queue);
	fileName = DEBUG_FILE_PATH + string("similVars/") + "finalTransmittance.csv";
	writeCSVFile(floatVec, width, fileName);

	captureSimilVarValidCnt(similVarValidCntBuffer);
}

void DebugManager::dumpParticles() {
	uint32_t densitiesCnt = gModel->densities.count;
	vector<float> densities(densitiesCnt);
	vector<float> forAvg(densitiesCnt);
	string output;
	output.reserve(densitiesCnt * 3);
	float avg;
	
	vulkanDevice->copyDeviceBufferToHost(densities.data(), gModel->densities.storageBuffer, *queue);

	transform(densities.begin(), densities.end(), densities.begin(), [](double val) { return 1.0f / (1.0f + exp(-val));});

	auto maxDensities = max_element(densities.begin(), densities.end());
	auto minDensities = min_element(densities.begin(), densities.end());
	
	transform(densities.begin(), densities.end(), forAvg.begin(), [densitiesCnt](double val) { return val / densitiesCnt; });
	avg = accumulate(forAvg.begin(), forAvg.end(), 0);

	std::cout << "max density : " << *maxDensities << "\n";
	std::cout << "min density : " << *minDensities << "\n";
	std::cout << "Average density : " << avg << "\n";
	
	for (int i = 0; i < densitiesCnt;) {
		for (int j = 0; j < 100 && i < densitiesCnt; j++) {
			output.append(to_string(densities[i]));
			output.push_back(',');
			i++;
		}
		output.pop_back();
		output.push_back('\n');
	}

	string fileName = DEBUG_FILE_PATH + string("particleDensities.csv");
	ofstream of(fileName);
	of << output;
	of.close();
}

void DebugManager::dumpIcosahedron() {
	string filename = "objDump.obj";
	cout << "dumping obj file\n";
	vkQueueWaitIdle(*queue);
	vector<float> vertices(gModel->vertices.count);
	vector<uint32_t> indices(gModel->indices.count);

	vulkanDevice->copyDeviceBufferToHost(vertices.data(), gModel->vertices.storageBuffer, *queue);
	vulkanDevice->copyDeviceBufferToHost(indices.data(), gModel->indices.storageBuffer, *queue);
	vkQueueWaitIdle(*queue);

	ofstream objFile(filename);
	for (int i = 0; i < vertices.size(); i += 3) {
		objFile << "v " << vertices[i] << " " << vertices[i + 1] << " " << vertices[i + 2] << "\n";
	}
	for (int i = 0; i < indices.size(); i += 3) {
		objFile << "f " << indices[i] + 1 << " " << indices[i + 1] + 1 << " " << indices[i + 2] + 1 << "\n";
	}
	objFile.close();
	cout << "dump done\n";
}