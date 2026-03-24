#pragma once

#include "glm/glm.hpp"

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

class KDTreeModel {
	int vntCnt;
	int faceCnt;
	vector<float> vntArray;
	//float* vntArray;
	vector<uint32_t> faceArray;
	//uint32_t* faceArray;
	
	int triCnt;

	vector<WaldTriangle> triAccList;

	vector<uint32_t> kdTreeNode;
	int nodeCnt;
	vector<uint32_t> triOffsetList;
	int triOffsetCnt;
	
	Aabb sceneBox;

	

private:
	bool loadGLBin(string glBinPath);
	bool makeTriAccData();
	bool loadKDTree(string kdtbinPath);
public:
	void load(string glbinPath, string kdtbinPath);
};