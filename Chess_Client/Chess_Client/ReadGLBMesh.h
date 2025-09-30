#pragma once
#include "Mesh.h"


// 스킨드 메시에 사용될 정점(vertex)의 구조를 정의합니다.
// 위치, 법선, 텍스처 좌표 외에 뼈의 인덱스와 가중치 정보를 포함합니다.
struct SkinnedVertex
{
    XMFLOAT3 m_xmf3Position;
    XMFLOAT3 m_xmf3Normal;
    XMFLOAT2 m_xmf2TexCoord;
    XMFLOAT4 m_xmf4BoneIndices;
    XMFLOAT4 m_xmf4BoneWeights;
};

// glTF 씬의 계층 구조를 나타내는 노드(Node)의 구조를 정의합니다.
// 변환(위치, 회전, 크기) 정보와 부모-자식 관계, 메시 참조 등을 포함합니다.
struct Node
{
    std::string name;
    int parentIndex = -1;
    std::vector<int> childrenIndices;
    XMFLOAT3 translation = { 0.0f, 0.0f, 0.0f };
    XMFLOAT4 rotation = { 0.0f, 0.0f, 0.0f, 1.0f };
    XMFLOAT3 scale = { 1.0f, 1.0f, 1.0f };
    int meshIndex = -1;
    int skinIndex = -1;
};

// GLB 파일에서 읽어온 데이터를 GPU에 올리기 전,
// CPU 메모리에 임시로 저장하기 위한 구조체입니다.
// 메시를 구성하는 각 프리미티브(primitive) 단위의 정점 및 인덱스 데이터를 담습니다.
struct GlbCpuData
{
    std::vector<SkinnedVertex> vertices;
    std::vector<UINT> indices;
    int material_index = -1;
};

// Mesh 클래스를 상속받아 glTF 바이너리 포맷(.glb)을 로드하는 데 특화된 클래스입니다.
// 파일 로딩(CPU)과 GPU 리소스 생성(GPU)의 역할을 명확히 분리합니다.
class ReadGlbMesh : public Mesh
{
public:
    // 생성자: 파일 경로를 받아 모델 데이터를 CPU 메모리로 로드합니다.
    ReadGlbMesh(const std::string& file_path);
    virtual ~ReadGlbMesh() = default;

    // Mesh 베이스 클래스로부터 상속받은 가상 함수들을 오버라이드합니다.

    // CPU 데이터를 기반으로 실제 GPU 리소스를 생성합니다.
    virtual void upload_to_gpu(ID3D12Device* device, ID3D12GraphicsCommandList* command_list) override;
    // 각 프리미티브에 대한 드로우 콜(Draw Call)을 제출하여 메시를 렌더링합니다.
    virtual void render(ID3D12GraphicsCommandList* command_list) override;
	std::tuple<std::vector<unsigned char>, UINT, UINT> load_image_from_glb(const json& j,
																		   const std::vector<char>& binary_data,
																		   int texture_index);

private:
    // accessor 인덱스를 기반으로 바이너리 버퍼에서 데이터를 추출하는 헬퍼 함수입니다.
    template<typename T>
    std::pair<T*, size_t> ReadGlbMesh::get_data(const nlohmann::json& j, const std::vector<char>& binary_data,
        int accessor_index)
    {
        if (accessor_index < 0 || !j.contains("accessors") || accessor_index >= j["accessors"].size()) {
            return { nullptr, 0 };
        }
        const auto& accessor = j["accessors"][accessor_index];
        if (!accessor.contains("bufferView")) {
            return { nullptr, 0 };
        }
        int bufferViewIndex = accessor["bufferView"];
        if (bufferViewIndex < 0 || !j.contains("bufferViews") || bufferViewIndex >= j["bufferViews"].size()) {
            return { nullptr, 0 };
        }
        const auto& bufferView = j["bufferViews"][bufferViewIndex];

        size_t totalOffset = 0;
        if (bufferView.contains("byteOffset")) {
            totalOffset += bufferView["byteOffset"].get<size_t>();
        }
        if (accessor.contains("byteOffset")) {
            totalOffset += accessor["byteOffset"].get<size_t>();
        }

        const char* dataStart = binary_data.data() + totalOffset;
        size_t count = accessor["count"];

        size_t dataSizeInBytes = count * sizeof(T);
        if ((totalOffset + dataSizeInBytes) > binary_data.size()) {
            return { nullptr, 0 };
        }
        return { reinterpret_cast<T*>(const_cast<char*>(dataStart)), count };
    }

private:
    // --- 파일에서 로드한 CPU 측 데이터 ---
    nlohmann::json _json_data;                      // 파싱된 GLB의 JSON 부분
    std::vector<char> _binary_data;                 // 원본 바이너리 버퍼 (BIN 청크)
    std::vector<GlbCpuData> _cpu_data_primitives;   // 각 프리미티브의 정점/인덱스 데이터
    std::vector<Node> _nodes;                       // 씬 계층 구조

    // --- upload_to_gpu에서 생성되는 GPU 측 데이터 ---
    // 렌더링 가능한 각 부분을 나타내는 MeshPrimitive 객체의 벡터입니다.
    // 각 MeshPrimitive는 버퍼, 뷰 같은 GPU 리소스를 가집니다.
    std::vector<std::unique_ptr<MeshPrimitive>> _primitives;

    // 스키닝 애니메이션을 위한 뼈 변환 행렬용 상수 버퍼입니다.
    ComPtr<ID3D12Resource> _cbBoneTransforms;
};