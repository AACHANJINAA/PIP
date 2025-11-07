#include "stdafx.h"
#include "ReadGLTFMesh.h"
#include "ResourceManager.h"


ReadGLTFMesh::ReadGLTFMesh(const std::string& filePath, bool ishave_animate)
{
	if (ishave_animate)
	{
		read_skinned_animation_mesh(filePath);
	}
	else
	{
		read_static_mesh(filePath);
	}
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

	// 첫 번째 프리미티브의 버퍼 정보를 기본 클래스의 멤버에 복사합니다.
	if (!_primitives.empty())
	{
		const auto& first_primitive = _primitives[0];
	    _vertexBufferView = first_primitive->_vertexBufferView;
        _indexBufferView = first_primitive->_indexBufferView;
	
	 // 기본 클래스의 render 함수가 사용할 수 있도록 인덱스 카운트도 복사합니다.
		 _indices.resize(first_primitive->_indexCount);
		 memcpy(_indices.data(), first_primitive->_indices.data(), sizeof(UINT) * first_primitive->_indexCount);
	}
}

//void ReadGLTFMesh::render(ID3D12GraphicsCommandList* commandList)
//{
//	if (!_isUploaded) return;
//
//	commandList->IASetPrimitiveTopology(_primitiveTopology);
//
//	for (const auto& primitive : _primitives)
//	{
//		commandList->IASetVertexBuffers(0, 1, &primitive->_vertexBufferView);
//		if (primitive->_indexCount > 0) {
//			commandList->IASetIndexBuffer(&primitive->_indexBufferView);
//			commandList->DrawIndexedInstanced(primitive->_indexCount, 1, 0, 0, 0);
//		}
//		else if (primitive->_vertexCount > 0) {
//			commandList->DrawInstanced(primitive->_vertexCount, 1, 0, 0);
//		}
//	}
//}


void ReadGLTFMesh::render(ID3D12GraphicsCommandList* commandList)
{
	if (!_isUploaded) return;

	commandList->IASetPrimitiveTopology(_primitiveTopology);

	for (const auto& primitive : _primitives)
	{
		if (primitive->_materialIndex >= 0 && primitive->_materialIndex < _material_names.size())
		{
			ResourceManager::instance()->bind_material(_material_names[primitive->_materialIndex], commandList);
		}

		//else
		//{
			// 유효하지 않은 재질 인덱스 또는 재질 이름이 없는 경우 기본 재질 등을 바인딩할 수 있습니다.
			//CWARNING("Invalid material index or no material name for primitive.");
			// TODO: 기본 재질 바인딩 로직 추가 (예: ResourceManager::instance()->bind_default_material(commandList);)
		//}

		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		commandList->IASetVertexBuffers(0, 1, &primitive->_vertexBufferView);
		commandList->IASetIndexBuffer(&primitive->_indexBufferView);
		commandList->DrawIndexedInstanced(primitive->_indexCount, 1, 0, 0, 0);
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

void ReadGLTFMesh::update_animation(float delta_time, int clip_index)
{
}

void ReadGLTFMesh::render_skinned(ID3D12GraphicsCommandList* commandList)
{
}

void ReadGLTFMesh::read_static_mesh(const std::string& filePath)
{
	// DW설명 : 파일 경로를 이름으로 설정
	set_name(filePath);

	json gltf_json;
	std::vector<char> binary_buffer;

	if (!load_gltf_file(filePath, gltf_json, binary_buffer))
	{
		CERROR("glTF 파일 로딩에 실패했습니다.");
		return;
	}

	// [추가] ResourceManager를 통해 재질 로드 및 이름 목록 채우기
	_material_names = ResourceManager::instance()->load_materials_from_gltf(filePath);

	int scene_idx = gltf_json.value("scene", 0);
	const json& scene = gltf_json["scenes"][scene_idx];

	XMFLOAT4X4 identity_matrix;
	XMStoreFloat4x4(&identity_matrix, XMMatrixIdentity());

	for (const auto& node_idx : scene["nodes"])
	{
		process_node(gltf_json, binary_buffer, node_idx.get<int>(), identity_matrix);
	}


	bounding_box_merge();

	_primitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

void ReadGLTFMesh::read_skinned_animation_mesh(const std::string& filePath)
{
	// --- 1. 공통 코드 (파일 로드) ---
	set_name(filePath);

	json gltf_json;
	std::vector<char> binary_buffer;

	if (!load_gltf_file(filePath, gltf_json, binary_buffer))
	{
		CERROR("glTF 파일 로딩에 실패했습니다.");
		return;
	}

	// 재질 로드
	_material_names = ResourceManager::instance()->load_materials_from_gltf(filePath);

	// --- 2. [신규] 스키닝/애니메이션 데이터 로드 ---
	// 
	// skins 배열을 파싱하여 뼈대 계층, joints 목록, 
	// InverseBindMatrix를 _skeleton, _joints 등의 멤버 변수에 저장
	load_skins(gltf_json, binary_buffer);

	// animations 배열을 파싱하여 키프레임 데이터를
	// _animations 멤버 변수에 저장
	load_animations(gltf_json, binary_buffer);

	// --- 3. 메쉬 노드 처리 ---
	// ReadStaticMesh의 process_node와 달리, 
	// T-포즈를 유지하고 뼈대 데이터를 로드하기 위해 씬을 직접 순회

	int scene_idx = gltf_json.value("scene", 0);

	const json& scene = gltf_json["scenes"][scene_idx];

	std::stack<int> node_stack;
	for (const auto& node_idx_json : scene["nodes"])
	{
		node_stack.push(node_idx_json.get<int>());
	}

	while (!node_stack.empty())
	{
		int node_index = node_stack.top();
		node_stack.pop();

		const json& node = gltf_json["nodes"][node_index];

		// 메쉬 노드를 찾으면, 스키닝용 메쉬 처리 함수를 호출
		if (node.contains("mesh"))
		{
			// 이 메쉬가 스키닝을 사용하는지 확인 (중요)
			if (node.contains("skin"))
			{
				// T-포즈를 유지하고 JOINTS_0, WEIGHTS_0를 읽는
				// 헬퍼 함수 (snake_case)
				process_skinned_mesh(gltf_json, binary_buffer, gltf_json["meshes"][node["mesh"].get<int>()]);
			}
			// else 
			// {
			//     (선택) 스키닝이 없는 메쉬도 T-포즈로 로드 (예: 무기)
			//     process_static_mesh_tpose(...);
			// }
		}

		if (node.contains("children"))
		{
			for (const auto& child_index_json : node["children"])
			{
				node_stack.push(child_index_json.get<int>());
			}
		}
	}

	// --- 4. 바운딩 박스 계산 ---
	bounding_box_merge();

	// 프리미티브 토폴로지 설정
	_primitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
}

void ReadGLTFMesh::load_skins(const json& gltf_json, const std::vector<char>& binary_buffer)
{
	// 스킨이 없으면 애니메이션 모델이 아님 (또는 스키닝이 없음)
	if (!gltf_json.contains("skins"))
	{
		CLOG("glTF 파일에 'skins' 배열이 없습니다. 스키닝 로드를 건너뜁니다.");
		return;
	}

	// 우리는 이 예제에서 0번 스킨만 로드한다고 가정 -> 이것도 수정예정
	const json& skin = gltf_json["skins"][0];

	// --- 1. 'joints' 배열 로드 (GPU 행렬 팔레트 순서) ---
	// _joints 멤버 변수에 뼈대로 사용될 'node' 인덱스 목록을 복사
	_joints = skin["joints"].get<std::vector<int>>();

	size_t num_joints = _joints.size();
	if (num_joints == 0)
	{
		CERROR("Skin에 'joints'가 없습니다.");
		return;
	}

	// --- 2. 뼈대 정보 컨테이너 크기 초기화 ---
	// _skeleton 멤버 변수의 크기를 뼈대 개수(72개)만큼 설정 -> 여기서 72라는 숫자는 일단 BruteHi가 72개라 적어둔것임
	_skeleton.resize(num_joints);
	// _final_bone_transforms 멤버 변수의 크기도 뼈대 개수만큼 설정하고,
	// 모두 단위 행렬로 초기화
	_final_bone_transforms.resize(num_joints);
	DirectX::XMFLOAT4X4 identity_matrix;
	DirectX::XMStoreFloat4x4(&identity_matrix, DirectX::XMMatrixIdentity());
	std::fill(_final_bone_transforms.begin(), _final_bone_transforms.end(), identity_matrix);


	// --- 3. 'inverseBindMatrices' 로드 ---
	int ibm_accessor_index = skin["inverseBindMatrices"].get<int>();
	// get_attribute_data 헬퍼 함수를 사용하여 바이너리 데이터 로드
	std::vector<DirectX::XMFLOAT4X4> inverse_bind_matrices =
		get_attribute_data<DirectX::XMFLOAT4X4>(gltf_json, binary_buffer, ibm_accessor_index);

	// --- 4. 뼈대 계층 구조 (부모-자식 관계) 구축 ---
	// 뼈대의 부모 인덱스를 찾기 위해 전체 'nodes' 배열을 순회하며 맵을 만든다람쥐
	std::unordered_map<int, int> node_to_parent_map; // key: 자식 노드 인덱스, value: 부모 노드 인덱스

	for (size_t i = 0; i < gltf_json["nodes"].size(); ++i)
	{
		const json& node = gltf_json["nodes"][i];
		if (node.contains("children"))
		{
			for (const auto& child_index_json : node["children"])
			{
				int child_node_index = child_index_json.get<int>();
				node_to_parent_map[child_node_index] = static_cast<int>(i); // 'i'가 부모 노드의 인덱스
			}
		}
	}

	// _skeleton 배열(0~71)의 인덱스를 쉽게 찾기 위해
	// 'node' 인덱스를 'joint' 인덱스로 변환하는 맵을 만든다.
	std::unordered_map<int, int> node_to_joint_map; // key: 노드 인덱스, value: joint 인덱스(0~71)
	for (size_t i = 0; i < num_joints; ++i)
	{
		node_to_joint_map[_joints[i]] = static_cast<int>(i);
	}

	// --- 5. 최종 _skeleton 멤버 변수 채우기 ---
	for (size_t i = 0; i < num_joints; ++i)
	{
		int node_index = _joints[i]; // glTF 'nodes' 배열에서의 실제 인덱스 (예: 72)
		const json& node = gltf_json["nodes"][node_index];

		_skeleton[i]._node_index = node_index;
		_skeleton[i]._name = node.value("name", "Bone_" + std::to_string(node_index));

		// glTF는 열 우선(Column-Major), DirectX는 행 우선(Row-Major)
		// 로드한 행렬을 전치(Transpose)하여 저장
		DirectX::XMMATRIX col_major_matrix = DirectX::XMLoadFloat4x4(&inverse_bind_matrices[i]);
		DirectX::XMMATRIX row_major_matrix = DirectX::XMMatrixTranspose(col_major_matrix);
		DirectX::XMStoreFloat4x4(&_skeleton[i]._inverse_bind_matrix, row_major_matrix);

		// 위에서 만든 맵을 사용하여 부모 뼈대의 인덱스(_skeleton 기준)를 찾는다.
		if (node_to_parent_map.find(node_index) != node_to_parent_map.end())
		{
			int parent_node_index = node_to_parent_map[node_index];

			// 부모 노드도 뼈대('joints' 목록에 포함)인지 확인
			if (node_to_joint_map.find(parent_node_index) != node_to_joint_map.end())
			{
				// _skeleton 배열 내의 부모 인덱스를 저장
				_skeleton[i]._parent_index = node_to_joint_map[parent_node_index];
			}
			else
			{
				// 부모가 뼈대가 아닌 경우 (예: 일반 노드 밑에 뼈대가 있음)
				_skeleton[i]._parent_index = -1; // 루트 뼈대로 취급
			}
		}
		else
		{
			// 부모가 없는 경우 (씬의 루트 노드이거나 스켈레톤의 루트)
			_skeleton[i]._parent_index = -1; // 루트 뼈대
		}
	}
}

void ReadGLTFMesh::load_animations(const json& gltf_json, const std::vector<char>& binary_buffer)
{
}

void ReadGLTFMesh::process_skinned_mesh(const json& gltf_json, const std::vector<char>& binary_buffer, const json& mesh)
{
}

bool ReadGLTFMesh::load_gltf_file(const std::string& filename, json& outJson, std::vector<char>& outBinBuffer)
{
	namespace fs = std::filesystem;

	std::ifstream gltf_file(filename);
	if (!gltf_file.is_open()) {
		std::cerr << "Error: Failed to open " << filename << std::endl;
		return false;
	}
	try {
		gltf_file >> outJson;
	}
	catch (json::parse_error& e) {
		std::cerr << "JSON parse error: " << e.what() << std::endl;
		return false;
	}
	gltf_file.close();

	if (outJson.contains("buffers") && !outJson["buffers"].empty() && outJson["buffers"][0].contains("uri")) {
		std::string bin_uri = outJson["buffers"][0]["uri"];
		fs::path gltf_path = filename;
		fs::path bin_path = gltf_path.parent_path() / bin_uri;

		std::ifstream bin_file(bin_path, std::ios::binary | std::ios::ate);
		if (!bin_file.is_open()) {
			std::cerr << "Error: Failed to open binary file " << bin_path << std::endl;
			return false;
		}

		std::streamsize size = bin_file.tellg();
		bin_file.seekg(0, std::ios::beg);

		outBinBuffer.resize(size);
		if (!bin_file.read(outBinBuffer.data(), size)) {
			std::cerr << "Error: Failed to read binary data from " << bin_path << std::endl;
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

void ReadGLTFMesh::process_node(const json& gltfJson, const std::vector<char>& binaryBuffer, int nodeIndex, const DirectX::XMFLOAT4X4& parentTransform)
{
	const json& node = gltfJson["nodes"][nodeIndex];

	XMMATRIX local_matrix = XMMatrixIdentity();
	if (node.contains("matrix")) {
		float mat[16];
		for (int i = 0; i < 16; ++i) mat[i] = node["matrix"][i].get<float>();
		local_matrix = XMLoadFloat4x4(&XMFLOAT4X4(mat));
	}
	else {
		XMMATRIX translation_matrix = XMMatrixIdentity();
		if (node.contains("translation")) {
			translation_matrix = XMMatrixTranslation(node["translation"][0].get<float>(), node["translation"][1].get<float>(), node["translation"][2].get<float>());
		}
		XMMATRIX rotation_matrix = XMMatrixIdentity();
		if (node.contains("rotation")) {
			rotation_matrix = XMMatrixRotationQuaternion(XMVectorSet(node["rotation"][0].get<float>(), node["rotation"][1].get<float>(), node["rotation"][2].get<float>(), node["rotation"][3].get<float>()));
		}
		XMMATRIX scale_matrix = XMMatrixIdentity();
		if (node.contains("scale")) {
			scale_matrix = XMMatrixScaling(node["scale"][0].get<float>(), node["scale"][1].get<float>(), node["scale"][2].get<float>());
		}
		local_matrix = scale_matrix * rotation_matrix * translation_matrix;
	}

	XMMATRIX world_matrix = local_matrix * XMLoadFloat4x4(&parentTransform);
	XMFLOAT4X4 world_transform;
	XMStoreFloat4x4(&world_transform, world_matrix);

	if (node.contains("mesh")) {
		const json& mesh = gltfJson["meshes"][node["mesh"].get<int>()];
		process_mesh(gltfJson, binaryBuffer, mesh, world_transform);
	}

	if (node.contains("children")) {
		for (const auto& child_index : node["children"]) {
			process_node(gltfJson, binaryBuffer, child_index.get<int>(), world_transform);
		}
	}
}

void ReadGLTFMesh::process_mesh(const json& gltfJson, const std::vector<char>& binaryBuffer, const json& mesh, const DirectX::XMFLOAT4X4& transform)
{
	for (const auto& primitive_json : mesh["primitives"])
	{

		_primitives.emplace_back(std::make_unique<GltfPrimitive>());

		auto& primitive = _primitives.back();


		std::vector<XMFLOAT3> positions = get_attribute_data<XMFLOAT3>(gltfJson, binaryBuffer, primitive_json["attributes"]["POSITION"]);
		std::vector<XMFLOAT3> normals = primitive_json["attributes"].contains("NORMAL") ? get_attribute_data<XMFLOAT3>(gltfJson, binaryBuffer, primitive_json["attributes"]["NORMAL"]) : std::vector<XMFLOAT3>();
		std::vector<XMFLOAT2> texcoords = primitive_json["attributes"].contains("TEXCOORD_0") ? get_attribute_data<XMFLOAT2>(gltfJson, binaryBuffer, primitive_json["attributes"]["TEXCOORD_0"]) : std::vector<XMFLOAT2>();
		if (texcoords.empty())
		{
			std::string mesh_name = mesh.contains("name") ? mesh["name"].get<std::string>() : "Unnamed";
			CLOG("Warning: Mesh '" + name() + "', Primitive in mesh '" + mesh_name + "' has no texture coordinates(TEXCOORD_0)."); 
		}
		std::vector<XMFLOAT4> tangents = primitive_json["attributes"].contains("TANGENT") ? get_attribute_data<XMFLOAT4>(gltfJson, binaryBuffer, primitive_json["attributes"]["TANGENT"]) : std::vector<XMFLOAT4>();

		primitive->_vertexCount = (UINT)positions.size();
		primitive->_vertices.resize(primitive->_vertexCount);
		XMMATRIX world_mat = XMLoadFloat4x4(&transform);

		for (size_t i = 0; i < primitive->_vertexCount; ++i) {
			XMVECTOR pos = XMLoadFloat3(&positions[i]);
			pos = XMVector3Transform(pos, world_mat);
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

		if (primitive_json.contains("indices")) {
			const json& accessor = gltfJson["accessors"][primitive_json["indices"].get<size_t>()];
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

		primitive->_materialIndex = primitive_json.value("material", -1);

	}
}

void ReadGLTFMesh::bounding_box_merge()
{
	if (!_primitives.empty())
	{
		// [명명법] 함수 내부 변수를 snake_case로 적용
		BoundingOrientedBox merged_obb = _primitives[0]->_orientedBoundingBox;
		for (size_t i = 1; i < _primitives.size(); ++i)
		{
			std::array<XMFLOAT3, 8> corners_a, corners_b;
			merged_obb.GetCorners(corners_a.data());
			_primitives[i]->_orientedBoundingBox.GetCorners(corners_b.data());

			std::vector<XMFLOAT3> all_points;
			all_points.reserve(16);
			all_points.insert(all_points.end(), corners_a.begin(), corners_a.end());
			all_points.insert(all_points.end(), corners_b.begin(), corners_b.end());

			BoundingOrientedBox::CreateFromPoints(merged_obb, all_points.size(), all_points.data(), sizeof(XMFLOAT3));
		}
		_orientedBoundingBox = merged_obb;
	}
}
