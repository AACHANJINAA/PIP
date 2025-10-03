#include "stdafx.h"
#include "ReadGLTFMesh.h"


ReadGLTFMesh::ReadGLTFMesh(ID3D12Device* d3d_device, ID3D12GraphicsCommandList* d3d_commandList, const std::string str, Scene* pScene)
{
	json gltf_json; // .gltf파일의 JSON 데이터를 저장할 json객체
	std::vector<char> binary_data; // .bin파일의 바이너리 데이터를 저장할 벡터
	can_load_gltf_file(str, gltf_json, binary_data);

	GltfMeshData gltf_mesh_data{};
	can_Extract_mesh_data(gltf_json, binary_data, gltf_mesh_data);

	//TODO: 기존 Mesh 클래스에 맞게 수정하든지 아니면 메쉬 클래스를 수정하든지
	//TODO: 정점, 인덱스 버퍼의 생성->upload_gpu함수, 텍스쳐 로드의 경우 ResourceManager로 옮기기
	create_vertex_and_index_buffers(d3d_device, d3d_commandList, gltf_mesh_data);

	load_textures(d3d_device, d3d_commandList, gltf_json, str);
}

ReadGLTFMesh::~ReadGLTFMesh()
{

}

void ReadGLTFMesh::render(ID3D12GraphicsCommandList* pd3dCommandList)
{

}

bool ReadGLTFMesh::can_load_gltf_file(const std::string& filename, json& outJson, std::vector<char>& outBinBuffer)
{
	// DW설명 : 이 함수의 목적은 .gltf 파일 읽어서 Json 객체와 .bin 파일의 바이너리 데이터를 벡터에 담는 것
	namespace fs = std::filesystem;

	// 1. JSON 파일 열기 및 파싱
	std::ifstream gltf_file(filename);
	if (!gltf_file.is_open()) {
		std::cerr << "Error: Failed to open " << filename << std::endl;
		return false;
	}
	gltf_file >> outJson;
	gltf_file.close();

	// 2. .bin 파일 경로 찾기 및 로드
	// glTF 명세에 따라, buffer의 URI는 .gltf 파일에 대한 상대 경로일 수 있음
	// DW설명 : 윗 줄의 말을 풀어서 설명하면, 우리가 컨트롤 + O 눌렀을때 .vcxproj파일을 기준으로 폴더가 뜨는 것처럼 .gltf 파일을 기준으로 삼는다는 뜻
	if (outJson.contains("buffers") && outJson["buffers"][0].contains("uri")) {
		std::string bin_uri = outJson["buffers"][0]["uri"];
		fs::path gltf_path = filename;
		fs::path bin_path = gltf_path.parent_path() / bin_uri; // 상대 경로 계산

		std::ifstream bin_file(bin_path, std::ios::binary | std::ios::ate);
		if (!bin_file.is_open()) {
			std::cerr << "Error: Failed to open " << bin_path << std::endl;
			return false;
		}

		std::streamsize size = bin_file.tellg();
		bin_file.seekg(0, std::ios::beg);

		outBinBuffer.resize(size);
		if (!bin_file.read(outBinBuffer.data(), size)) {
			CERROR("Error: Failed to read binary data from " << bin_path);
			return false;
		}
		bin_file.close();
	}
	else {
		std::cerr << "Error: No buffer URI found in glTF file." << std::endl;
		return false;
	}

	return true;
}

void ReadGLTFMesh::copy_data_from_buffer(std::vector<char>& dest, const std::vector<char>& sourceBinBuffer, const json& gltfJson, int accessorIndex)
{
	// DW설명 : 이 함수의 목적은 glTF의 accessor 정보를 바탕으로 바이너리 버퍼에서 데이터를 복사해오는 것

	const auto& accessor = gltfJson["accessors"][accessorIndex];
	const auto& buffer_view = gltfJson["bufferViews"][accessor["bufferView"]];

	int count = accessor["count"];
	size_t byte_offset = buffer_view.value("byteOffset", 0) + accessor.value("byteOffset", 0);

	// 데이터 타입의 크기를 계산합니다. glTF는 componentType으로 명시함
	// 5126 (FLOAT), 5123 (UNSIGNED_SHORT), 5125 (UNSIGNED_INT) 등등
	size_t component_size = 0;
	if (accessor["componentType"] == 5126) component_size = sizeof(float);
	else if (accessor["componentType"] == 5125) component_size = sizeof(unsigned int);
	else if (accessor["componentType"] == 5123) component_size = sizeof(unsigned short);
	// ... 다른 타입 추가

	// accessor의 type ("VEC3", "SCALAR" 등)에 따라 컴포넌트 개수 결정
	size_t num_components = 0;
	if (accessor["type"] == "VEC3") num_components = 3;
	else if (accessor["type"] == "VEC2") num_components = 2;
	else if (accessor["type"] == "SCALAR") num_components = 1;

	size_t data_size = count * num_components * component_size;
	dest.resize(data_size);

	// 버퍼 뷰에 byteStride(바이트 보폭)가 명시되어 있다면, 데이터를 하나씩 복사해야 함
	if (buffer_view.contains("byteStride") && buffer_view["byteStride"] != (num_components * component_size)) {
		size_t byte_stride = buffer_view["byteStride"];
		for (int i = 0; i < count; ++i) {
			// DW생각 : 너무 길면 아래 memcpy처럼 적는게 더 편한거 같다. ()와 내용들의 줄을 분리해서 적는 방식
			memcpy(
				dest.data() + i * (num_components * component_size),
				sourceBinBuffer.data() + byte_offset + i * byte_stride,
				num_components * component_size
			);
		}
	}
	else { // 데이터가 연속적이라면 한 번에 복사 -> 연속적이라면 byteStride가 정의되어 있지 않음
		memcpy(dest.data(), sourceBinBuffer.data() + byte_offset, data_size);
	}

}

bool ReadGLTFMesh::can_Extract_mesh_data(const json& gltf_json, const std::vector<char>& bin_buffer, GltfMeshData& out_mesh_data)
{
	// 예시: 첫 번째 씬의 첫 번째 노드의 메쉬를 사용
	const auto& scene = gltf_json["scenes"][gltf_json["scene"]];
	const auto& node = gltf_json["nodes"][scene["nodes"][0]];
	const auto& mesh = gltf_json["meshes"][node["mesh"]];
	const auto& primitive = mesh["primitives"][0];

	// 정점 데이터 추출

	// 위치(POSITION)
	int position_accessor_index = primitive["attributes"]["POSITION"];
	std::vector<char> position_data;
	copy_data_from_buffer(position_data, bin_buffer, gltf_json, position_accessor_index);

	// 법선벡터(NORMAL)
	int normal_accessor_index = primitive["attributes"]["NORMAL"];
	std::vector<char> normal_data;
	copy_data_from_buffer(normal_data, bin_buffer, gltf_json, normal_accessor_index);

	// 탄젠트벡터(TANGENT)
	int tangent_accessor_index = primitive["attributes"]["TANGENT"];
	std::vector<char> tangent_data;
	copy_data_from_buffer(tangent_data, bin_buffer, gltf_json, tangent_accessor_index);

	// 텍스쳐좌표(TEXCOORD_0)
	int texcoord_accessor_index = primitive["attributes"]["TEXCOORD_0"];
	std::vector<char> texcoord_data;
	copy_data_from_buffer(texcoord_data, bin_buffer, gltf_json, texcoord_accessor_index);

	// Vertex 구조체에 맞게 데이터 재구성
	int vertex_count = gltf_json["accessors"][position_accessor_index]["count"];
	out_mesh_data._vertices.resize(vertex_count);

	// 각 데이터 버퍼를 float 포인터로 변환하여 다루기 쉽게 만들기
	// DW설명 : 부득이 하게 p를 붙임 float*를 이용하기 위한 변수들이기 때문
	float* p_positions = reinterpret_cast<float*>(position_data.data());
	float* p_normals = reinterpret_cast<float*>(normal_data.data());
	float* p_tangent = reinterpret_cast<float*>(tangent_data.data());
	float* p_texcoords = reinterpret_cast<float*>(texcoord_data.data());

	for (int i = 0; i < vertex_count; ++i) {
		// Position 복사 (float 3개)
		memcpy(&out_mesh_data._vertices[i]._position,   // 목적지: i번째 정점의 position 필드
			p_positions + (i * 3),               // 원본: 전체 위치 데이터에서 i번째 위치 (vec3)
			sizeof(float) * 3);                 // 크기: float 3개

		// Normal 복사 (float 3개)
		memcpy(&out_mesh_data._vertices[i]._normal,     // 목적지: i번째 정점의 normal 필드
			p_normals + (i * 3),                 // 원본: 전체 법선 데이터에서 i번째 법선 (vec3)
			sizeof(float) * 3);                 // 크기: float 3개

		// Tangent 복사 (float 4개)
		memcpy(&out_mesh_data._vertices[i]._tangent,   // 목적지: i번째 정점의 tangent 필드
			p_tangent + (i * 4),               // 원본:전체 탄젠트 데이터에서 i번째 탄젠트 (vec4)
			sizeof(float) * 4);                 // 크기: float 4개

		// TexCoord 복사 (float 2개)
		memcpy(&out_mesh_data._vertices[i]._texCoord,   // 목적지: i번째 정점의 texCoord 필드
			p_texcoords + (i * 2),               // 원본: 전체 UV 데이터에서 i번째 UV (vec2)
			sizeof(float) * 2);                 // 크기: float 2개
	}

	// 인덱스(indices) 데이터 추출
	int indices_accessor_index = primitive["indices"];
	std::vector<char> indices_data;
	copy_data_from_buffer(indices_data, bin_buffer, gltf_json, indices_accessor_index);

	// glTF는 인덱스 타입으로 UNSIGNED_SHORT (2바이트) 또는 UNSIGNED_INT (4바이트)를 주로 사용
	// 여기서는 uint32_t로 변환하여 저장
	int index_count = gltf_json["accessors"][indices_accessor_index]["count"];
	out_mesh_data._indices.resize(index_count);

	const auto& index_accessor = gltf_json["accessors"][indices_accessor_index];
	if (index_accessor["componentType"] == 5123) { // UNSIGNED_SHORT
		std::vector<uint16_t> short_indices(index_count);
		memcpy(short_indices.data(), indices_data.data(), indices_data.size());
		for (int i = 0; i < index_count; ++i) {
			out_mesh_data._indices[i] = static_cast<uint32_t>(short_indices[i]);
		}
	}
	else if (index_accessor["componentType"] == 5125) { // UNSIGNED_INT
		memcpy(out_mesh_data._indices.data(), indices_data.data(), indices_data.size());
	}

	return true;
}

void ReadGLTFMesh::create_vertex_and_index_buffers(ID3D12Device* d3d_device, ID3D12GraphicsCommandList* d3d_commandList, const GltfMeshData& gltf_mesh_data)
{
	// --- 1. 정점 버퍼 생성 ---
	UINT vertex_buffer_size = sizeof(IlluminatedVertex) * gltf_mesh_data._vertices.size(); // GltfVertex는 직접 정의한 정점 구조체

	// 최종 버퍼는 DEFAULT 힙에, 최종 상태는 VERTEX_BUFFER로 지정
	_d3dVertexBuffer = CreateBufferResource(d3d_device, d3d_commandList, (void*)gltf_mesh_data._vertices.data(),
		vertex_buffer_size, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, &_d3dVertexUploadBuffer);

	// --- 2. 정점 버퍼 뷰 생성 ---
	_d3dVertexBufferView.BufferLocation = _d3dVertexBuffer->GetGPUVirtualAddress();
	_d3dVertexBufferView.StrideInBytes = sizeof(IlluminatedVertex);
	_d3dVertexBufferView.SizeInBytes = vertex_buffer_size;

	// --- 3. 인덱스 버퍼 생성 ---
	UINT nIndexBufferSize = sizeof(UINT) * gltf_mesh_data._indices.size();

	// 인덱스 버퍼 리소스를 생성, 최종 상태는 INDEX_BUFFER
	_d3dIndexBuffer = CreateBufferResource(d3d_device, d3d_commandList, (void*)gltf_mesh_data._indices.data(),
		nIndexBufferSize, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_INDEX_BUFFER, &_d3dIndexUploadBuffer);

	// --- 4. 인덱스 버퍼 뷰 생성 ---
	_d3dIndexBufferView.BufferLocation = _d3dIndexBuffer->GetGPUVirtualAddress();
	_d3dIndexBufferView.Format = DXGI_FORMAT_R32_UINT; // 32비트 인덱스 사용
	_d3dIndexBufferView.SizeInBytes = nIndexBufferSize;

	// --- 5. 렌더링에 사용할 인덱스 개수 저장 ---
	_indexCount = gltf_mesh_data._indices.size();
}

void ReadGLTFMesh::load_textures(ID3D12Device* d3d_device, ID3D12GraphicsCommandList* d3d_commandList, const json& gltf_json, const std::string& base_path)
{
	// glTF 파일에 images, textures, materials가 없으면 로드할 텍스처가 없음
	if (!gltf_json.contains("images") || !gltf_json.contains("textures") || !gltf_json.contains("materials"))
	{
		return;
	}

	// 예시: 첫 번째 머티리얼의 BaseColor 텍스처만 로드
	const auto& material = gltf_json["materials"][0];
	if (!material.contains("pbrMetallicRoughness") || !material["pbrMetallicRoughness"].contains("baseColorTexture"))
	{
		return;
	}

	int texture_index = material["pbrMetallicRoughness"]["baseColorTexture"]["index"];
	int image_index = gltf_json["textures"][texture_index]["source"];
	std::string texture_uri = gltf_json["images"][image_index]["uri"];

	// .gltf 파일에 대한 상대 경로이므로, 전체 경로를 만들어 줌
	std::filesystem::path full_path = std::filesystem::path(base_path).parent_path() / texture_uri;

	// DirectXTex를 사용하여 이미지 파일 로드
	DirectX::ScratchImage image;
	HRESULT hr = DirectX::LoadFromWICFile(full_path.c_str(), DirectX::WIC_FLAGS_NONE, nullptr, image);
	if (FAILED(hr))
	{
		// 이미지 로드 실패 처리
		return;
	}

	// 텍스처 리소스 생성 (DEFAULT 힙)
	ID3D12Resource* texture_buffer = nullptr;
	D3D12_RESOURCE_DESC texture_desc = {};
	texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texture_desc.Alignment = 0;
	texture_desc.Width = image.GetMetadata().width;
	texture_desc.Height = image.GetMetadata().height;
	texture_desc.DepthOrArraySize = 1;
	texture_desc.MipLevels = 1;
	texture_desc.Format = image.GetMetadata().format;
	texture_desc.SampleDesc.Count = 1;
	texture_desc.SampleDesc.Quality = 0;
	texture_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texture_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

	D3D12_HEAP_PROPERTIES heap_default_props = {};
	heap_default_props.Type = D3D12_HEAP_TYPE_DEFAULT;
	heap_default_props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heap_default_props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heap_default_props.CreationNodeMask = 1;
	heap_default_props.VisibleNodeMask = 1;

	d3d_device->CreateCommittedResource(
		&heap_default_props,
		D3D12_HEAP_FLAG_NONE,
		&texture_desc,
		D3D12_RESOURCE_STATE_COPY_DEST, // 업로드를 위해 COPY_DEST 상태로 생성
		nullptr,
		IID_PPV_ARGS(&texture_buffer));

	// 업로드 힙을 통해 텍스처 데이터 복사
	UINT64 upload_buffer_size;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT placed_footprint;
	d3d_device->GetCopyableFootprints(&texture_desc, 0, 1, 0, &placed_footprint, nullptr, nullptr, &upload_buffer_size);

	ID3D12Resource* texture_upload_buffer = nullptr;
	//auto heapUpload = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	D3D12_HEAP_PROPERTIES heap_upload_props = {};
	heap_upload_props.Type = D3D12_HEAP_TYPE_UPLOAD;
	heap_upload_props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heap_upload_props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heap_upload_props.CreationNodeMask = 1;
	heap_upload_props.VisibleNodeMask = 1;

	//auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(nUploadBufferSize);
	D3D12_RESOURCE_DESC upload_buffer_desc = {};
	upload_buffer_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	upload_buffer_desc.Alignment = 0;
	upload_buffer_desc.Width = upload_buffer_size;
	upload_buffer_desc.Height = 1;
	upload_buffer_desc.DepthOrArraySize = 1;
	upload_buffer_desc.MipLevels = 1;
	upload_buffer_desc.Format = DXGI_FORMAT_UNKNOWN;
	upload_buffer_desc.SampleDesc.Count = 1;
	upload_buffer_desc.SampleDesc.Quality = 0;
	upload_buffer_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	upload_buffer_desc.Flags = D3D12_RESOURCE_FLAG_NONE;


	d3d_device->CreateCommittedResource(
		&heap_upload_props,
		D3D12_HEAP_FLAG_NONE,
		&upload_buffer_desc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&texture_upload_buffer));

	//UpdateSubresources(d3d_commandList, pTextureBuffer, pTextureUploadBuffer, 0, 0, 1, &textureData);

	// 1. 업로드 버퍼를 CPU에서 쓸 수 있도록 매핑(Map)
	void* mapped_data;
	texture_upload_buffer->Map(0, nullptr, &mapped_data);

	// 2. 원본 이미지 데이터를 업로드 버퍼로 한 줄씩 복사
	//    GPU는 메모리 정렬을 위해 각 라인 끝에 패딩(padding)을 추가할 수 있으므로,
	//    단순 memcpy가 아닌 라인별 복사가 필요
	D3D12_SUBRESOURCE_DATA texture_data = {};
	texture_data.pData = image.GetPixels();
	texture_data.RowPitch = image.GetImage(0, 0, 0)->rowPitch;
	texture_data.SlicePitch = image.GetImage(0, 0, 0)->slicePitch;

	BYTE* dest_slice = reinterpret_cast<BYTE*>(mapped_data);
	const BYTE* src_slice = reinterpret_cast<const BYTE*>(texture_data.pData);
	for (UINT y = 0; y < texture_desc.Height; ++y)
	{
		memcpy(dest_slice + y * placed_footprint.Footprint.RowPitch, // 목적지: GPU가 요구하는 RowPitch 사용
			src_slice + y * texture_data.RowPitch,             // 원본: 이미지 파일의 RowPitch 사용
			texture_data.RowPitch);
	}

	// 3. CPU 쓰기가 끝났으므로 매핑을 해제(Unmap)
	texture_upload_buffer->Unmap(0, nullptr);

	// 4. 커맨드 리스트에 GPU 복사 명령을 기록합니다.
	D3D12_TEXTURE_COPY_LOCATION dest_location = {};
	dest_location.pResource = texture_buffer;
	dest_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dest_location.SubresourceIndex = 0;

	D3D12_TEXTURE_COPY_LOCATION src_location = {};
	src_location.pResource = texture_upload_buffer;
	src_location.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	src_location.PlacedFootprint = placed_footprint;

	d3d_commandList->CopyTextureRegion(&dest_location, 0, 0, 0, &src_location, nullptr);


	// 텍스처 상태를 셰이더에서 읽을 수 있도록 변경
	// auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(pTextureBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
	D3D12_RESOURCE_BARRIER barrier = {};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = texture_buffer;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	d3d_commandList->ResourceBarrier(1, &barrier);

	// 생성된 리소스들을 멤버 변수에 저장
	_TextureResources.push_back(texture_buffer);
	_TextureUploadBuffers.push_back(texture_upload_buffer); // 나중에 해제해야 함 -> Comptr이라 이제 안해도 되지롱~
}
