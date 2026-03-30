#pragma once
#include <vector>
#include <string>
#include <DirectXMath.h>
#include "Vector3.h"
#include "json.hpp"

namespace PIP
{
    // Jolt Physics 등 물리 엔진에서 충돌체를 생성하기 위한 최소 데이터 구조
    struct MeshData
    {
        std::vector<common::Vec3> vertices;
        std::vector<uint32_t> indices;
        std::string name;
    };

    class glTFMeshLoader
    {
    public:
        /**
         * @brief .gltf 파일을 로드하여 모든 메쉬의 전역 좌표 기준 정점/인덱스 데이터를 추출합니다.
         * @param filePath .gltf 파일 경로
         * @return 추출된 메쉬 데이터 리스트
         */
        static std::vector<MeshData> LoadStaticMesh(const std::string& filePath);

    private:
        // glTF JSON 및 연결된 .bin 파일을 로드합니다.
        static bool LoadGltfFile(const std::string& filename, nlohmann::json& outJson, std::vector<char>& outBinBuffer);
        
        // glTF Accessor를 통해 바이너리 버퍼에서 속성 데이터를 추출하는 헬퍼 템플릿
        template<typename T>
        static std::vector<T> GetAttributeData(const nlohmann::json& gltfJson, const std::vector<char>& binaryBuffer, int accessorIndex);

        // 노드 계층 구조를 순회하며 Transform을 적용하고 메쉬를 처리합니다.
        static void ProcessNode(const nlohmann::json& gltfJson, const std::vector<char>& binaryBuffer, 
                               int nodeIndex, const DirectX::XMFLOAT4X4& parentTransform, std::vector<MeshData>& outMeshes);
        
        // 실제 메쉬 프리미티브에서 정점(Position)과 인덱스 데이터를 추출합니다.
        static void ProcessMesh(const nlohmann::json& gltfJson, const std::vector<char>& binaryBuffer, 
                               const nlohmann::json& meshJson, const DirectX::XMFLOAT4X4& transform, std::vector<MeshData>& outMeshes);
    };
}
