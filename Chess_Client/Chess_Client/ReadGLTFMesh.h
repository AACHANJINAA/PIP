#pragma once
#include "Mesh.h"
struct GltfVertex // GLTF에서 사용하는 정점 구조체 -> DW계획 : 추후 필요없는 구조체는 삭제할 예정
{
	XMFLOAT3 _position;
	XMFLOAT3 _normal;
	XMFLOAT4 _tangent;
	XMFLOAT2 _texCoord;
};


struct GltfPrimitiveData
{
	ComPtr<ID3D12Resource> _vertexBuffer;
	ComPtr<ID3D12Resource> _indexBuffer;

	D3D12_VERTEX_BUFFER_VIEW _vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW _indexBufferView;

	UINT _indexCount = 0;
	int _materialIndex = -1; // 이 프리미티브가 사용할 m_textures 벡터의 인덱스
};

struct GltfMeshData
{
	std::vector<IlluminatedVertex> _vertices;
	std::vector<uint32_t> _indices; // 인덱스 타입은 accessor에 따라 달라질 수 있음 -> 이거 경고 막고싶은디...
};
class ReadGLTFMesh : public Mesh
{
public:
	ReadGLTFMesh() {};
	ReadGLTFMesh(ID3D12Device* d3d_device, ID3D12GraphicsCommandList* d3d_commandList, const std::string str, class Scene* pScene);

	~ReadGLTFMesh() override;

	void render(ID3D12GraphicsCommandList* pd3dCommandList) override;

private: // DW생각 : gltf이기 때문에 함수 단위로 분리해서 불러오기
	// .gltf파일 불러오기 및 .bin파일 읽고, 성공여부를 반환ㅎㅏ기
	bool can_load_gltf_file(const std::string& filename, json& out_json, std::vector<char>& out_bin_buffer);

	// 거대한 bin 데이서에서 gltf의 accessor정보에 따라 잘라내는 것
	void copy_data_from_buffer(std::vector<char>& dest, const std::vector<char>& source_bin_buffer, const json& gltf_json, int accessor_index);

	// 데이터 추출
	bool can_Extract_mesh_data(const json& gltf_json, const std::vector<char>& bin_buffer, GltfMeshData& out_mesh_data);

	// 정점버퍼 및 인덱스버퍼 생성
	void create_vertex_and_index_buffers(ID3D12Device* d3d_device, ID3D12GraphicsCommandList* d3d_commandList, const GltfMeshData& gltf_mesh_data);

	// 텍스쳐 로드
	void load_textures(ID3D12Device* d3d_device, ID3D12GraphicsCommandList* d3d_commandList, const json& gltf_json, const std::string& base_path);

private:
	// json _gltfJson; // .gltf파일의 JSON 데이터를 저장할 json객체
	// std::vector<char> _binaryData; // .bin파일의 바이너리 데이터를 저장할 벡터
	// 렌더링 시 사용할 함수


private:
	// 버퍼 리소스
	ComPtr<ID3D12Resource> _d3dVertexBuffer = nullptr;
	ComPtr<ID3D12Resource> _d3dIndexBuffer = nullptr;

	// 버퍼 생성을 위한 임시 업로드 버퍼
	ComPtr<ID3D12Resource> _d3dVertexUploadBuffer = nullptr;
	ComPtr<ID3D12Resource> _d3dIndexUploadBuffer = nullptr;

	// 텍스처 리소스 (여러 개일 수 있음)
	std::vector<ComPtr<ID3D12Resource>> _TextureResources;
	std::vector<ComPtr<ID3D12Resource>> _TextureUploadBuffers; // 텍스처 업로드용

	// 버퍼 뷰
	D3D12_VERTEX_BUFFER_VIEW _d3dVertexBufferView;
	D3D12_INDEX_BUFFER_VIEW _d3dIndexBufferView;

	// 그릴 인덱스 개수
	UINT _indexCount = 0;
};

