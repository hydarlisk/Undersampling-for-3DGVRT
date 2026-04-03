#include "KdTreeModel.h"
#include "VulkanUtils.h"

#include "Logger.h"

#if defined(__ANDROID__)
#include "VulkanAndroid.h"
#endif

#include <stdio.h>

using namespace std;

vks::utils::Aabb KdTreeModel::getAabb() {
	vks::utils::Aabb ret;
	ret.minX = sceneBox.min.x;
	ret.minY = sceneBox.min.y;
	ret.minZ = sceneBox.min.z;
	ret.maxX = sceneBox.max.x;
	ret.maxY = sceneBox.max.y;
	ret.maxZ = sceneBox.max.z;
	return ret;
}
bool KdTreeModel::loadGLBin(string glbinPath) {
#if defined(__ANDROID__)
	AAsset* asset = AAssetManager_open(androidApp->activity->assetManager, glbinPath.c_str(), AASSET_MODE_STREAMING);
	if (!asset) {
		LOGE("Error: failed to open asset: %s\n", glbinPath.c_str());
		return false;
	}

	size_t assetLength = AAsset_getLength(asset);
	std::vector<uint8_t> buffer(assetLength);
	AAsset_read(asset, buffer.data(), assetLength);
	AAsset_close(asset);
	uint8_t* cursor = buffer.data();
	uint8_t* endPtr = buffer.data() + assetLength;

	if (cursor + 4 > endPtr) return false;
	uint32_t vntArrLength;
	memcpy(&vntArrLength, cursor, 4);
	cursor += 4;

	if (cursor + 4 > endPtr) return false;
	uint32_t faceCnt;
	memcpy(&faceCnt, cursor, 4);
	cursor += 4;

	int triCnt = faceCnt / 3;
	int vntCnt = vntArrLength / VERTEX_SIZE;
    LOGI("<GL> vntSize: %d, faceSize: %d\n", vntArrLength, faceCnt);
    LOGI("<GL> numOfTri: %d, numOfVert: %d\n", triCnt, vntCnt);

	vntArray.resize(vntArrLength);
	size_t vntByteSize = sizeof(float) * vntArrLength;
	if (cursor + vntByteSize > endPtr) return false;

	memcpy(vntArray.data(), cursor, vntByteSize);
	cursor += vntByteSize;

	faceArray.resize(faceCnt);
	for (int i = 0; i < faceCnt; i++) {
		faceArray[i] = i;
	} 
#else
	FILE* fp = fopen(glbinPath.c_str(), "rb");
	if (fp == NULL) {
		printf("SceneLoaderForGL : Scene data open error\n");
		printf("total Path : %s\n", glbinPath.c_str());
		return false;
	}

	fread(&vntArrLength, 4, 1, fp);
	faceCnt = vntArrLength / VERTEX_SIZE;
	triCnt = faceCnt / 3;
	vntCnt = vntArrLength / VERTEX_SIZE;
	printf("<GL> vntSize: %d, faceSize: %d\n", vntArrLength, faceCnt);
	printf("<GL> numOfTri: %d, numOfVert: %d\n", triCnt, vntCnt);
	vntArray.resize(vntArrLength);
	if (vntArray.size() < vntArrLength) {
		printf("Mem Alloc Error : vntArray\n");
		return false;
	}
	fread(vntArray.data(), sizeof(float), vntArrLength, fp);

	faceArray.resize(faceCnt);
	if (faceArray.size() < faceCnt) {
		printf("Mem Alloc Error : faceArray\n");
		return false;
	}
	//fread(faceArray.data(), sizeof(uint32_t), faceArrLength, fp);
	for (int i = 0; i < faceCnt; i++) {
		faceArray[i] = i;
	}

	fflush(fp);
	fclose(fp);
#endif

	return true;
}

void boxMinMax(Aabb& sb, glm::vec3& v) {
	sb.min.x = min(sb.min.x, v.x);
	sb.min.y = min(sb.min.y, v.y);
	sb.min.z = min(sb.min.z, v.z);

	sb.max.x = max(sb.max.x, v.x);
	sb.max.y = max(sb.max.y, v.y);
	sb.max.z = max(sb.max.z, v.z);
}

bool KdTreeModel::makeTriAccData() {
	sceneBox.min.x = 100000;
	sceneBox.min.y = 100000;
	sceneBox.min.z = 100000;

	sceneBox.max.x = -100000;
	sceneBox.max.y = -100000;
	sceneBox.max.z = -100000;
	
	glm::vec3 A, B, C, b, c, N;
	glm::vec3 tmpb, tmpc, tmpA, tmpN;
	int k, u, v;
	float r;

#if USE_TRI_ACCEL
	triAccList.resize(triCnt);
	if (triAccList.size() < triCnt) {
		printf("Mem Alloc Error : triAccList\n");
		return false;
	}
#endif
	for (int i = 0; i < faceCnt; i+=3) {
		//calc sceneBox
		A.x = vntArray[i * VERTEX_SIZE + 0];
		A.y = vntArray[i * VERTEX_SIZE + 1];
		A.z = vntArray[i * VERTEX_SIZE + 2];
		B.x = vntArray[(i + 1) * VERTEX_SIZE + 0];
		B.y = vntArray[(i + 1) * VERTEX_SIZE + 1];
		B.z = vntArray[(i + 1) * VERTEX_SIZE + 2];
		C.x = vntArray[(i + 2) * VERTEX_SIZE + 0];
		C.y = vntArray[(i + 2) * VERTEX_SIZE + 1];
		C.z = vntArray[(i + 2) * VERTEX_SIZE + 2];
		boxMinMax(sceneBox, A);
		boxMinMax(sceneBox, B);
		boxMinMax(sceneBox, C);
#if USE_TRI_ACCEL
		b = C - A;
		c = B - A;
		N = glm::cross(b, c);
		N = normalize(N);
		
		if (abs(N.x) > abs(N.y)) {
			if (abs(N.x) > abs(N.z)) k = 0;
			else k = 2;
		}
		else {
			if (abs(N.y) > abs(N.z)) k = 1;
			else k = 2;
		}

		tmpN = N;
		N /= tmpN[k];

		u = (k + 1) % 3;
		v = (k + 2) % 3;

		int triIdx = i / 3;
		triAccList[triIdx].k = k;
		triAccList[triIdx].n_u = tmpN[u];
		triAccList[triIdx].n_v = tmpN[v];
		triAccList[triIdx].n_d = glm::dot(N, A);

		tmpb = b;
		tmpc = c;
		tmpA = A;

		//TODO: divide by zero?
		r = 1.f / (tmpb[u] * tmpc[v] - tmpb[v] * tmpc[u]);
		if ((tmpb[u] * tmpc[v] - tmpb[v] * tmpc[u] == 0.0f))
			tmpA[0] = 0;

		triAccList[triIdx].b_nu = r * -tmpb[v];
		triAccList[triIdx].b_nv = r * tmpb[u];
		triAccList[triIdx].b_d = r * (tmpb[v] * tmpA[u] - tmpb[u] * tmpA[v]);
		triAccList[triIdx].c_nu = r * tmpc[v];
		triAccList[triIdx].c_nv = r * -tmpc[u];
		triAccList[triIdx].c_d = r * (tmpc[u] * tmpA[v] - tmpc[v] * tmpA[u]);
		printf("Tri Acc List size : [%dB][%dMB]", triCnt * 48, triCnt * 48 / (1024 * 1024));
#endif
	}
	sceneBox.print("Scene box");
	
	return true;
}

bool KdTreeModel::loadKDTree(std::string kdtbinPath)
{
#if defined(__ANDROID__)
	AAsset* asset = AAssetManager_open(androidApp->activity->assetManager, kdtbinPath.c_str(), AASSET_MODE_STREAMING);
	if (!asset) {
		LOGE("Error: failed to open asset: %s\n", kdtbinPath.c_str());
		return false;
	}

	size_t assetLength = AAsset_getLength(asset);
	std::vector<uint8_t> buffer(assetLength);
	AAsset_read(asset, buffer.data(), assetLength);
	AAsset_close(asset);
	uint8_t* cursor = buffer.data();
	uint8_t* endPtr = buffer.data() + assetLength;

	if (cursor + sizeof(int) > endPtr) return false;
	int nodeCnt;
	memcpy(&nodeCnt, cursor, sizeof(int));
	cursor += sizeof(int);

	kdTreeNode.resize(nodeCnt * 2);
	size_t nodeByteSize = sizeof(uint32_t) * nodeCnt * 2;

	if (cursor + nodeByteSize > endPtr) {
		LOGE("Error: Unexpected EOF while reading kdTreeNodes\n");
		return false;
	}
	memcpy(kdTreeNode.data(), cursor, nodeByteSize);
	cursor += nodeByteSize;

	if (cursor + sizeof(int) > endPtr) return false;
	int triOffsetCnt;
	memcpy(&triOffsetCnt, cursor, sizeof(int));
	cursor += sizeof(int);

    LOGI("tri offset list size %d * 4 = %d B\n", triOffsetCnt, triOffsetCnt * 4);

	triOffsetList.resize(triOffsetCnt);
	size_t triOffsetByteSize = sizeof(uint32_t) * triOffsetCnt;

	if (cursor + triOffsetByteSize > endPtr) {
        LOGI("Error: Unexpected EOF while reading triOffsetList\n");
		return false;
	}
	memcpy(triOffsetList.data(), cursor, triOffsetByteSize);
	cursor += triOffsetByteSize;

    LOGV("Total kd-Tree Size = [%zu B][%.2f MB]\n",
		(nodeCnt * 2 + triOffsetCnt) * 4,
		(float)(nodeCnt * 2 + triOffsetCnt) * 4 / 1024.0f / 1024.0f);
#else
	FILE* fp = fopen(kdtbinPath.c_str(), "rb");
	if (fp == NULL) {
		printf("SceneLoaderForGL : Scene data open error\n");
		printf("total Path : %s\n", kdtbinPath.c_str());
		return false;
	}

	//kdtree nodes
	fread(&nodeCnt, sizeof(int), 1, fp);
	printf("kd tree node size %d * 8 = %d B\n", nodeCnt, nodeCnt * 8);
	kdTreeNode.resize(nodeCnt * 2);
	if (kdTreeNode.size() < nodeCnt * 2) {
		printf("Mem Alloc Error : kdTreeNode\n");
		return false;
	}
	fread(kdTreeNode.data(), sizeof(uint32_t), nodeCnt * 2, fp);

	//triangle offset
	fread(&triOffsetCnt, sizeof(int), 1, fp);
	printf("tri offset list size %d * 4 = %d B\n", triOffsetCnt, triOffsetCnt * 4);
	triOffsetList.resize(triOffsetCnt);
	if (triOffsetList.size() < triOffsetCnt) {
		printf("Mem Alloc Error : faceArray\n");
		return false;
	}
	fread(triOffsetList.data(), sizeof(uint32_t), triOffsetCnt, fp);

	fclose(fp);

	printf("Total kd-Tree Size = [%d B][%.2f MB]\n", (nodeCnt * 2 + triOffsetCnt) * 4, (float)(nodeCnt * 2 + triOffsetCnt) * 4 / 1024 / 1024);

#endif
	return true;
}

void KdTreeModel::load(string glbinPath, string kdtbinPath) {
	if (!loadGLBin(glbinPath)) {
		vks::tools::exitFatal("loading glbin file failed\n", -1);
	}
	if (!makeTriAccData()) {
		vks::tools::exitFatal("make tri acc data failed\n", -1);
	}
	if (!loadKDTree(kdtbinPath)) {
		vks::tools::exitFatal("loading kdtbin file failed\n", -1);
	}
}

bool KdTreeModel::uploadToGPU(vks::VulkanDevice* vulkanDevice, VkQueue& queue) {
	VkFlags usageFlag = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
	VkFlags memPropertyFlag = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
	LOGV("before upload vntArray\n");
	VK_CHECK_RESULT(vulkanDevice->createAndCopyToDeviceBuffer(vntArray.data(), d_vntArray, vntArrLength * sizeof(float), queue, usageFlag, memPropertyFlag));
	LOGV("upload vntArray done\n");
	VK_CHECK_RESULT(vulkanDevice->createAndCopyToDeviceBuffer(faceArray.data(), d_faceArray, faceCnt * sizeof(uint32_t), queue, usageFlag, memPropertyFlag));
	LOGV("upload faceArray done\n");
	VK_CHECK_RESULT(vulkanDevice->createAndCopyToDeviceBuffer(kdTreeNode.data(), d_kdTreeNode, nodeCnt * sizeof(uint32_t) * 2, queue, usageFlag, memPropertyFlag));
	LOGV("upload kdtreeNode done\n");
	VK_CHECK_RESULT(vulkanDevice->createAndCopyToDeviceBuffer(triOffsetList.data(), d_triOffsetList, triOffsetCnt * sizeof(uint32_t), queue, usageFlag, memPropertyFlag));
	LOGV("upload tri offset list done\n");
	VK_CHECK_RESULT(vulkanDevice->createAndCopyToDeviceBuffer(triAccList.data(), d_triAccList, triCnt * sizeof(WaldTriangle), queue, usageFlag, memPropertyFlag));
	LOGV("upload tri acc list done\n");
	LOGV("upload gaussian data to gpu done\n");
	return true;
}