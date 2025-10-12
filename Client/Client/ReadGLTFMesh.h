#pragma once
#include "stdafx.h"
#include "Mesh.h"
struct GltfVertex : public Vertex
{
public:
	XMFLOAT3 _normal;      // 법선 벡터
	XMFLOAT2 _texCoord;    // 텍스처 좌표 (UV)
	XMFLOAT4 _tangent;     // 탄젠트 벡터 (w 요소는 handedness를 나타냄)

	// 향후 애니메이션 확장 영역
	// 주석 처리된 이 부분에 스키닝 데이터를 추가할 수 있습니다.
	// XMFLOAT4 _boneIndices; // 영향을 주는 뼈(Bone)의 인덱스 (최대 4개)
	// XMFLOAT4 _boneWeights; // 각 뼈로부터 받는 가중치

public:
	GltfVertex() {
		_position = XMFLOAT3(0.0f, 0.0f, 0.0f);
		_normal = XMFLOAT3(0.0f, 0.0f, 0.0f);
		_texCoord = XMFLOAT2(0.0f, 0.0f);
		_tangent = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.0f);
	}

	GltfVertex(XMFLOAT3 p, XMFLOAT3 n, XMFLOAT2 t, XMFLOAT4 tan) {
		_position = p;
		_normal = n;
		_texCoord = t;
		_tangent = tan;
	}
};

struct GltfPrimitive
{
	// CPU
	std::vector<GltfVertex> _vertices;
	std::vector<UINT> _indices;

	// GPU
	ComPtr<ID3D12Resource> _vertexBuffer;
	ComPtr<ID3D12Resource> _indexBuffer;

	D3D12_VERTEX_BUFFER_VIEW _vertexBufferView;
	D3D12_INDEX_BUFFER_VIEW _indexBufferView;

	UINT _vertexCount = 0;
	UINT _indexCount = 0;
	int _materialIndex = -1;

	BoundingOrientedBox _orientedBoundingBox;

	// 임시 업로드 버퍼 로딩이 끝나면 해제
	ComPtr<ID3D12Resource> _vertexUploadBuffer;
	ComPtr<ID3D12Resource> _indexUploadBuffer;
};

class ReadGLTFMesh : public Mesh
{
public:
	ReadGLTFMesh(const std::string& filePath);
	~ReadGLTFMesh() override;

	void upload_to_gpu_internal(ID3D12Device* device, ID3D12GraphicsCommandList* commandList) override;

	virtual void render(ID3D12GraphicsCommandList* commandList) override;
	void release_upload_buffers() override;

private:
	bool load_gltf_file(const std::string& filename, json& outJson, std::vector<char>& outBinBuffer);
	void process_node(const json& gltfJson, const std::vector<char>& binaryBuffer, int nodeIndex, const DirectX::XMFLOAT4X4& parentTransform);
	void process_mesh(const json& gltfJson, const std::vector<char>& binaryBuffer, const json& mesh, const DirectX::XMFLOAT4X4& transform);

	template<typename T>
	std::vector<T> get_attribute_data(const json& gltfJson, const std::vector<char>& binaryBuffer, int accessorIndex)
	{
		if (accessorIndex < 0) return {};

		const json& accessor = gltfJson["accessors"][accessorIndex];
		const json& bufferView = gltfJson["bufferViews"][accessor["bufferView"].get<size_t>()];

		size_t count = accessor["count"];
		size_t byteOffset = bufferView.value("byteOffset", 0) + accessor.value("byteOffset", 0);
		size_t elementSize = sizeof(T);
		size_t byteStride = bufferView.value("byteStride", elementSize);

		std::vector<T> data(count);
		const char* bufferStart = binaryBuffer.data() + byteOffset;

		for (size_t i = 0; i < count; ++i) {
			memcpy(&data[i], bufferStart + i * byteStride, elementSize);
		}

		return data;
	}

private:
	std::vector<std::unique_ptr<GltfPrimitive>> _primitives;
};