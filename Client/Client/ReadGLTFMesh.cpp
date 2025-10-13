#include "stdafx.h"
#include "ReadGLTFMesh.h"


ReadGLTFMesh::ReadGLTFMesh(const std::string& filePath)
{
	// DW설명 : 파일 경로를 이름으로 설정
	set_name(filePath);

	json gltfJson;
	std::vector<char> binaryBuffer;

	if (!load_gltf_file(filePath, gltfJson, binaryBuffer)) 
	{
		CERROR("glTF 파일 로딩에 실패했습니다.");
		return;
	}

	int sceneIdx = gltfJson.value("scene", 0);
	const json& scene = gltfJson["scenes"][sceneIdx];

	XMFLOAT4X4 identityMatrix;
	XMStoreFloat4x4(&identityMatrix, XMMatrixIdentity());

	for (const auto& nodeIdx : scene["nodes"]) 
	{
		process_node(gltfJson, binaryBuffer, nodeIdx.get<int>(), identityMatrix);
	}

	// 전체 모델의 바운딩 박스를 모든 프리미티브의 바운딩 박스를 병합하여 계산
	// DW설명 : 모든 프리미티브의 OBB를 병합하여 메쉬 전체의 OBB를 계싼함
	if (!_primitives.empty())
	{
		BoundingOrientedBox mergedObb = _primitives[0]->_orientedBoundingBox;
		for (size_t i = 1; i < _primitives.size(); ++i)
		{
			std::array<XMFLOAT3, 8> cornersA, cornersB;
			mergedObb.GetCorners(cornersA.data());
			_primitives[i]->_orientedBoundingBox.GetCorners(cornersB.data());

			std::vector<XMFLOAT3> allPoints;
			allPoints.reserve(16);
			allPoints.insert(allPoints.end(), cornersA.begin(), cornersA.end());
			allPoints.insert(allPoints.end(), cornersB.begin(), cornersB.end());

			BoundingOrientedBox::CreateFromPoints(mergedObb, allPoints.size(), allPoints.data(), sizeof(XMFLOAT3));
		}
		_orientedBoundingBox = mergedObb;
	}

	_primitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

ReadGLTFMesh::~ReadGLTFMesh()
{
	_primitives.clear();
}

void ReadGLTFMesh::upload_to_gpu_internal(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
	// 각 프리미티브가 가진 CPU 데이터를 기반으로 GPU 버퍼를 생성합니다.
	for (const auto& primitive : _primitives)
	{
		if (primitive->_vertexCount > 0)
		{
			// 정점 버퍼 생성
			primitive->_vertexBuffer = ::CreateBufferResource(device, commandList, primitive->_vertices.data(), sizeof(GltfVertex) * primitive->_vertexCount,
				D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &primitive->_vertexUploadBuffer);

			// 정점 버퍼 뷰 설정
			primitive->_vertexBufferView.BufferLocation = primitive->_vertexBuffer->GetGPUVirtualAddress();
			primitive->_vertexBufferView.StrideInBytes = sizeof(GltfVertex);
			primitive->_vertexBufferView.SizeInBytes = sizeof(GltfVertex) * primitive->_vertexCount;
		}

		// 인덱스 버퍼 생성
		if (primitive->_indexCount > 0)
		{
			primitive->_indexBuffer = ::CreateBufferResource(device, commandList, primitive->_indices.data(), sizeof(UINT) * primitive->_indexCount,
				D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, &primitive->_indexUploadBuffer);

			// 인덱스 버퍼 뷰 설정
			primitive->_indexBufferView.BufferLocation = primitive->_indexBuffer->GetGPUVirtualAddress();
			primitive->_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
			primitive->_indexBufferView.SizeInBytes = sizeof(UINT) * primitive->_indexCount;
		}
	}
}

void ReadGLTFMesh::render(ID3D12GraphicsCommandList* commandList)
{
	if (!_isUploaded) return;

	commandList->IASetPrimitiveTopology(_primitiveTopology);

	for (const auto& primitive : _primitives)
	{
		commandList->IASetVertexBuffers(0, 1, &primitive->_vertexBufferView);
		if (primitive->_indexCount > 0) {
			commandList->IASetIndexBuffer(&primitive->_indexBufferView);
			commandList->DrawIndexedInstanced(primitive->_indexCount, 1, 0, 0, 0);
		}
		else if (primitive->_vertexCount > 0) {
			commandList->DrawInstanced(primitive->_vertexCount, 1, 0, 0);
		}
	}
}

void ReadGLTFMesh::release_upload_buffers()
{
	for (auto& primitive : _primitives)
	{
		if (primitive->_vertexUploadBuffer) primitive->_vertexUploadBuffer.Reset();
		if (primitive->_indexUploadBuffer) primitive->_indexUploadBuffer.Reset();
	}
}

bool ReadGLTFMesh::load_gltf_file(const std::string& filename, json& outJson, std::vector<char>& outBinBuffer)
{
	namespace fs = std::filesystem;

	std::ifstream gltfFile(filename);
	if (!gltfFile.is_open()) {
		std::cerr << "Error: Failed to open " << filename << std::endl;
		return false;
	}
	try {
		gltfFile >> outJson;
	}
	catch (json::parse_error& e) {
		std::cerr << "JSON parse error: " << e.what() << std::endl;
		return false;
	}
	gltfFile.close();

	if (outJson.contains("buffers") && !outJson["buffers"].empty() && outJson["buffers"][0].contains("uri")) {
		std::string binUri = outJson["buffers"][0]["uri"];
		fs::path gltfPath = filename;
		fs::path binPath = gltfPath.parent_path() / binUri;

		std::ifstream binFile(binPath, std::ios::binary | std::ios::ate);
		if (!binFile.is_open()) {
			std::cerr << "Error: Failed to open binary file " << binPath << std::endl;
			return false;
		}

		std::streamsize size = binFile.tellg();
		binFile.seekg(0, std::ios::beg);

		outBinBuffer.resize(size);
		if (!binFile.read(outBinBuffer.data(), size)) {
			std::cerr << "Error: Failed to read binary data from " << binPath << std::endl;
			return false;
		}
		binFile.close();
	}
	else {
		std::cerr << "Error: No buffer URI found in glTF file." << std::endl;
		return false;
	}

	return true;
}

void ReadGLTFMesh::process_node(const json& gltfJson, const std::vector<char>& binaryBuffer, int nodeIndex, const DirectX::XMFLOAT4X4& parentTransform)
{
	const json& node = gltfJson["nodes"][nodeIndex];

	XMMATRIX localMatrix = XMMatrixIdentity();
	if (node.contains("matrix")) {
		float mat[16];
		for (int i = 0; i < 16; ++i) mat[i] = node["matrix"][i].get<float>();
		localMatrix = XMLoadFloat4x4(&XMFLOAT4X4(mat));
	}
	else {
		XMMATRIX translationMatrix = XMMatrixIdentity();
		if (node.contains("translation")) {
			translationMatrix = XMMatrixTranslation(node["translation"][0].get<float>(), node["translation"][1].get<float>(), node["translation"][2].get<float>());
		}
		XMMATRIX rotationMatrix = XMMatrixIdentity();
		if (node.contains("rotation")) {
			rotationMatrix = XMMatrixRotationQuaternion(XMVectorSet(node["rotation"][0].get<float>(), node["rotation"][1].get<float>(), node["rotation"][2].get<float>(), node["rotation"][3].get<float>()));
		}
		XMMATRIX scaleMatrix = XMMatrixIdentity();
		if (node.contains("scale")) {
			scaleMatrix = XMMatrixScaling(node["scale"][0].get<float>(), node["scale"][1].get<float>(), node["scale"][2].get<float>());
		}
		localMatrix = scaleMatrix * rotationMatrix * translationMatrix;
	}

	XMMATRIX worldMatrix = localMatrix * XMLoadFloat4x4(&parentTransform);
	XMFLOAT4X4 worldTransform;
	XMStoreFloat4x4(&worldTransform, worldMatrix);

	if (node.contains("mesh")) {
		const json& mesh = gltfJson["meshes"][node["mesh"].get<int>()];
		process_mesh(gltfJson, binaryBuffer, mesh, worldTransform);
	}

	if (node.contains("children")) {
		for (const auto& childIndex : node["children"]) {
			process_node(gltfJson, binaryBuffer, childIndex.get<int>(), worldTransform);
		}
	}
}

void ReadGLTFMesh::process_mesh(const json& gltfJson, const std::vector<char>& binaryBuffer, const json& mesh, const DirectX::XMFLOAT4X4& transform)
{
	for (const auto& primitiveJson : mesh["primitives"])
	{

		_primitives.emplace_back(std::make_unique<GltfPrimitive>());

		auto& primitive = _primitives.back();


		std::vector<XMFLOAT3> positions = get_attribute_data<XMFLOAT3>(gltfJson, binaryBuffer, primitiveJson["attributes"]["POSITION"]);
		std::vector<XMFLOAT3> normals = primitiveJson["attributes"].contains("NORMAL") ? get_attribute_data<XMFLOAT3>(gltfJson, binaryBuffer, primitiveJson["attributes"]["NORMAL"]) : std::vector<XMFLOAT3>();
		std::vector<XMFLOAT2> texcoords = primitiveJson["attributes"].contains("TEXCOORD_0") ? get_attribute_data<XMFLOAT2>(gltfJson, binaryBuffer, primitiveJson["attributes"]["TEXCOORD_0"]) : std::vector<XMFLOAT2>();
		std::vector<XMFLOAT4> tangents = primitiveJson["attributes"].contains("TANGENT") ? get_attribute_data<XMFLOAT4>(gltfJson, binaryBuffer, primitiveJson["attributes"]["TANGENT"]) : std::vector<XMFLOAT4>();

		primitive->_vertexCount = (UINT)positions.size();
		primitive->_vertices.resize(primitive->_vertexCount);
		XMMATRIX worldMat = XMLoadFloat4x4(&transform);

		for (size_t i = 0; i < primitive->_vertexCount; ++i) {
			XMVECTOR pos = XMLoadFloat3(&positions[i]);
			pos = XMVector3Transform(pos, worldMat);
			XMStoreFloat3(&primitive->_vertices[i]._position, pos);

			primitive->_vertices[i]._normal = (i < normals.size()) ? normals[i] : XMFLOAT3(0.0f, 1.0f, 0.0f);
			primitive->_vertices[i]._texCoord = (i < texcoords.size()) ? texcoords[i] : XMFLOAT2(0.0f, 0.0f);
			primitive->_vertices[i]._tangent = (i < tangents.size()) ? tangents[i] : XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
		}

		// 정점 확인 디버깅 
		//CLOG("--- Primitive Vertex Data Check ---");
		//CLOG("Total Vertices Loaded: " << primitive->_vertexCount);
		//// 불러온 정점 데이터의 첫 5개 위치 값을 출력해봅니다.
		//for (size_t i = 0; i < min((size_t)5, (size_t)primitive->_vertexCount); ++i)
		//{
		//	const auto& v = primitive->_vertices[i];
		//	CLOG("V[" << i << "] Position: (" << v._position.x << ", " << v._position.y << ", " << v._position.z << ")");
		//}

		if (primitiveJson.contains("indices")) {
			const json& accessor = gltfJson["accessors"][primitiveJson["indices"].get<size_t>()];
			const json& bufferView = gltfJson["bufferViews"][accessor["bufferView"].get<size_t>()];
			const char* data_ptr = binaryBuffer.data() + bufferView.value("byteOffset", 0) + accessor.value("byteOffset", 0);
			primitive->_indexCount = accessor["count"];
			primitive->_indices.resize(primitive->_indexCount);

			if (accessor["componentType"] == 5123) {
				const uint16_t* p_indices = reinterpret_cast<const uint16_t*>(data_ptr);
				for (size_t i = 0; i < primitive->_indexCount; ++i) {
					primitive->_indices[i] = static_cast<UINT>(p_indices[i]);
				}
			}
			else if (accessor["componentType"] == 5125) {
				memcpy(primitive->_indices.data(), data_ptr, primitive->_indexCount * sizeof(UINT));
			}
		}

		BoundingOrientedBox::CreateFromPoints(primitive->_orientedBoundingBox, primitive->_vertices.size(), &primitive->_vertices[0]._position, sizeof(GltfVertex));

		primitive->_materialIndex = primitiveJson.value("material", -1);

	}
}