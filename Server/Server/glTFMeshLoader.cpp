#include "pch.h"
#include "glTFMeshLoader.h"
#include <filesystem>
#include <fstream>

using namespace DirectX;
using json = nlohmann::json;

namespace PIP
{
	std::vector<MeshData> glTFMeshLoader::LoadStaticMesh(const std::string& filePath)
	{
		json gltfJson;
		std::vector<char> binaryBuffer;
		std::vector<MeshData> outMeshes;

		if (!LoadGltfFile(filePath, gltfJson, binaryBuffer))
		{
			return outMeshes;
		}
		int scene_idx = gltfJson.value("scene", 0);
		const json& scene = gltfJson["scenes"][scene_idx];

		XMFLOAT4X4 identity;
		XMStoreFloat4x4(&identity, XMMatrixIdentity());

		
		for (const auto& node_idx : scene["nodes"]) {
			ProcessNode(gltfJson, binaryBuffer, node_idx.get<int>(), identity, outMeshes);
		}

		return outMeshes;
	}

	std::vector<MeshData> glTFMeshLoader::LoadStaticMeshWithTransform(const std::string& filePath,
		const DirectX::XMFLOAT4X4& externalTransform)
	{

		json gltfJson;
		std::vector<char> binaryBuffer;
		std::vector<MeshData> outMeshes;

		if (!LoadGltfFile(filePath, gltfJson, binaryBuffer)) return outMeshes;

		// 외부에서 전달받은 transform을 시작점으로 노드 순회 시작
		if (gltfJson.contains("scenes") && gltfJson.contains("scene")) {
			int sceneIdx = gltfJson["scene"];
			for (int nodeIdx : gltfJson["scenes"][sceneIdx]["nodes"]) {
				ProcessNode(gltfJson, binaryBuffer, nodeIdx, externalTransform, outMeshes);
			}
		}
		return outMeshes;
	}

	bool glTFMeshLoader::LoadGltfFile(const std::string& filename, json& outJson, std::vector<char>& outBinBuffer)
	{
		std::ifstream file(filename);
		if (!file.is_open()) return false;
		
		try {
			file >> outJson;
		} catch (...) {
			return false;
		}

		std::filesystem::path path(filename);
		if (!outJson.contains("buffers") || outJson["buffers"].empty()) return false;

		std::string binName = outJson["buffers"][0]["uri"].get<std::string>();
		std::string binPath = (path.parent_path() / binName).string();

		std::ifstream binFile(binPath, std::ios::binary);
		if (!binFile.is_open()) return false;

		outBinBuffer = std::vector<char>((std::istreambuf_iterator<char>(binFile)), std::istreambuf_iterator<char>());
		return true;
	}

	void glTFMeshLoader::ProcessNode(const json& gltfJson, const std::vector<char>& binaryBuffer, 
								   int nodeIndex, const XMFLOAT4X4& parentTransform, std::vector<MeshData>& outMeshes)
	{
		const json& node = gltfJson["nodes"][nodeIndex];
		XMMATRIX localMat = XMMatrixIdentity();

		if (node.contains("matrix"))
		{
			std::vector<float> m = node["matrix"].get<std::vector<float>>();
			// glTF 행렬은 Column-major입니다. XMFLOAT4X4(float*)는 Row-major 순으로 로드하므로 
			// glTF 데이터를 올바르게 해석하려면 주의가 필요합니다. 
			// 클라이언트(ReadGLTFMesh.cpp:1734)는 직접 대입하므로 동일하게 구성합니다.
			XMFLOAT4X4 mat4x4 = XMFLOAT4X4(
				m[0], m[1], m[2], m[3],
				m[4], m[5], m[6], m[7],
				m[8], m[9], m[10], m[11],
				m[12], m[13], m[14], m[15]
			);
			// [수정] 클라이언트와 동일하게 Z-Flip 행렬 적용
			DirectX::XMMATRIX raw_mat = XMLoadFloat4x4(&mat4x4);
			DirectX::XMMATRIX z_flip = DirectX::XMMatrixScaling(1.0f, 1.0f, -1.0f);
			localMat = z_flip * raw_mat * z_flip;
		}
		else
		{
			XMMATRIX scale_mat = XMMatrixIdentity();
			if (node.contains("scale"))
				scale_mat = XMMatrixScaling(node["scale"][0].get<float>(), node["scale"][1].get<float>(), node["scale"][2].get<float>());

			XMMATRIX rot_mat = XMMatrixIdentity();
			if (node.contains("rotation"))
				rot_mat = XMMatrixRotationQuaternion(XMVectorSet(
					-node["rotation"][0].get<float>(), 
					-node["rotation"][1].get<float>(), 
					node["rotation"][2].get<float>(), 
					node["rotation"][3].get<float>()));

			XMMATRIX trans_mat = XMMatrixIdentity();
			if (node.contains("translation"))
				trans_mat = XMMatrixTranslation(
					node["translation"][0].get<float>(), 
					node["translation"][1].get<float>(), 
					-node["translation"][2].get<float>());

			localMat = scale_mat * rot_mat * trans_mat;
		}

		XMMATRIX worldMat = localMat * XMLoadFloat4x4(&parentTransform);
		XMFLOAT4X4 world_transform;
		XMStoreFloat4x4(&world_transform, worldMat);

		XMVECTOR det = XMMatrixDeterminant(worldMat);
		bool isMirrored = XMVectorGetX(det) < 0.0f;

		if (node.contains("mesh"))
		{
			int meshIdx = node["mesh"].get<int>();
			ProcessMesh(gltfJson, binaryBuffer, gltfJson["meshes"][meshIdx], world_transform, outMeshes, isMirrored);
		}

		if (node.contains("children"))
		{
			for (const auto& childIdx : node["children"])
			{
				ProcessNode(gltfJson, binaryBuffer, childIdx.get<int>(), world_transform, outMeshes);
			}
		}
	}

	void glTFMeshLoader::ProcessMesh(const json& gltfJson, const std::vector<char>& binaryBuffer, 
								   const json& meshJson, const XMFLOAT4X4& transform, std::vector<MeshData>& outMeshes, bool isMirrored)
	{
		XMMATRIX worldMat = XMLoadFloat4x4(&transform);

		for (const auto& primitive : meshJson["primitives"])
		{
			if (!primitive["attributes"].contains("POSITION")) continue;

			MeshData meshData;
			meshData.name = meshJson.value("name", "Unnamed_Mesh");

			// 1. POSITION 데이터 추출 및 전역 좌표 변환
			std::vector<XMFLOAT3> positions = GetAttributeData<XMFLOAT3>(gltfJson, binaryBuffer, primitive["attributes"]["POSITION"]);
			meshData.vertices.reserve(positions.size());

			for (const auto& p : positions)
			{
				XMFLOAT3 lhPos(p.x, p.y, -p.z); // TODO: 문제있으면 -z해볼것
				XMVECTOR posVec = XMLoadFloat3(&lhPos);
				posVec = XMVector3Transform(posVec, worldMat);
				
				XMFLOAT3 finalPos;
				XMStoreFloat3(&finalPos, posVec);
				meshData.vertices.push_back({ finalPos.x, finalPos.y, finalPos.z });
			}

			// 2. INDICES 데이터 추출 (타입별 처리)
			if (primitive.contains("indices"))
			{
				int accessorIdx = primitive["indices"].get<int>();
				const json& accessor = gltfJson["accessors"][accessorIdx];
				const json& bufferView = gltfJson["bufferViews"][accessor["bufferView"].get<int>()];
				
				size_t byteOffset = bufferView.value("byteOffset", 0) + accessor.value("byteOffset", 0);
				const char* dataPtr = binaryBuffer.data() + byteOffset;
				
				uint32_t count = accessor["count"];
				meshData.indices.resize(count);

				uint32_t componentType = accessor["componentType"];
				if (componentType == 5121) // UNSIGNED_BYTE
				{
					const uint8_t* buf = reinterpret_cast<const uint8_t*>(dataPtr);
					for (uint32_t i = 0; i < count; ++i) meshData.indices[i] = buf[i];
				}
				else if (componentType == 5123) // UNSIGNED_SHORT
				{
					const uint16_t* buf = reinterpret_cast<const uint16_t*>(dataPtr);
					for (uint32_t i = 0; i < count; ++i) meshData.indices[i] = buf[i];
				}
				else if (componentType == 5125) // UNSIGNED_INT
				{
					memcpy(meshData.indices.data(), dataPtr, count * sizeof(uint32_t));
				}
			}
			
			for (uint32_t i = 0; i < meshData.indices.size(); i += 3)
			{
				std::swap(meshData.indices[i + 1], meshData.indices[i + 2]);
			}

			outMeshes.push_back(std::move(meshData));
		}
	}

	template<typename T>
	std::vector<T> glTFMeshLoader::GetAttributeData(const json& gltfJson, const std::vector<char>& binaryBuffer, int accessorIndex)
	{
		const json& accessor = gltfJson["accessors"][accessorIndex];
		const json& bufferView = gltfJson["bufferViews"][accessor["bufferView"].get<int>()];
		
		size_t byteOffset = bufferView.value("byteOffset", 0) + accessor.value("byteOffset", 0);
		const char* dataPtr = binaryBuffer.data() + byteOffset;
		
		std::vector<T> data(accessor["count"]);
		memcpy(data.data(), dataPtr, data.size() * sizeof(T));
		return data;
	}
}
