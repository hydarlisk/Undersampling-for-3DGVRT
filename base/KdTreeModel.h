#pragma once

#include "glm/glm.hpp"

#include "VulkanDevice.h"

#include <vector>
#include <string>

using namespace std;

struct WaldTriangle {
	float n_u;
	float n_v;
	float n_d;
	float k;

	float b_nu;
	float b_nv;
	float b_d;
	float pad;

	float c_nu;
	float c_nv;
	float c_d;
	float pd;
};

struct Aabb {
	glm::vec4 min;
	glm::vec4 max;
	void print(const char* label = "Aabb") const {
		printf("[%s]\n", label);
		printf("  min: (%.3f, %.3f, %.3f)\n", min.x, min.y, min.z);
		printf("  max: (%.3f, %.3f, %.3f)\n", max.x, max.y, max.z);
	}
};

class KdTreeModel {
	int vntCnt;
	int vntArrLength;
	int faceCnt;
	vector<float> vntArray;
	vector<uint32_t> faceArray;
	int triCnt;
	vector<uint32_t> kdTreeNode;
	int nodeCnt;
	vector<uint32_t> triOffsetList;
	int triOffsetCnt;
	vector<WaldTriangle> triAccList;
	
	Aabb sceneBox;

public:
	vks::Buffer d_vntArray;
	vks::Buffer d_faceArray;
	vks::Buffer d_triAccList;
	vks::Buffer d_kdTreeNode;
	vks::Buffer d_triOffsetList;
	
private:
	bool loadGLBin(string glBinPath);
	bool makeTriAccData();
	bool loadKDTree(string kdtbinPath);
public:
	void load(string glbinPath, string kdtbinPath);
	bool uploadToGPU(vks::VulkanDevice* vulkanDevice, VkQueue& queue);
};