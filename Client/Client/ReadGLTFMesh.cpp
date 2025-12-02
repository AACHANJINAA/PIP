#include "stdafx.h"
#include "ReadGLTFMesh.h"
#include "ResourceManager.h"


ReadGLTFMesh::ReadGLTFMesh(const std::string& filePath, bool is_animated)
{
	_is_animated = is_animated;

	if (_is_animated)
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
	// 1. 각 프리미티브의 정점/인덱스 버퍼 생성 (기존 로직 유지)
	for (const auto& primitive : _primitives)
	{
		if (primitive->_vertexCount > 0)
		{
			// 스키닝 데이터 유무에 따라 데이터 포인터와 크기 결정
			void* data_ptr = nullptr;
			UINT stride_in_bytes = 0;
			UINT buffer_size = 0;

			if (!primitive->_skinned_vertices.empty())
			{
				// 애니메이션 메쉬 (Skinned)
				data_ptr = primitive->_skinned_vertices.data();
				stride_in_bytes = sizeof(GltfSkinnedVertex);
				buffer_size = stride_in_bytes * primitive->_vertexCount;
			}
			else
			{
				// 정적 메쉬 (Static)
				data_ptr = primitive->_vertices.data();
				stride_in_bytes = sizeof(GltfVertex);
				buffer_size = stride_in_bytes * primitive->_vertexCount;
			}

			// 정점 버퍼 생성
			primitive->_vertexBuffer = ::CreateBufferResource(
				device, commandList,
				data_ptr,
				buffer_size,
				D3D12_HEAP_TYPE_DEFAULT,
				D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
				&primitive->_vertexUploadBuffer
			);

			// 정점 버퍼 뷰 설정
			primitive->_vertexBufferView.BufferLocation = primitive->_vertexBuffer->GetGPUVirtualAddress();
			primitive->_vertexBufferView.StrideInBytes = stride_in_bytes;
			primitive->_vertexBufferView.SizeInBytes = buffer_size;
		}

		// 인덱스 버퍼 생성
		if (primitive->_indexCount > 0)
		{
			primitive->_indexBuffer = ::CreateBufferResource(
				device, commandList,
				primitive->_indices.data(),
				sizeof(UINT) * primitive->_indexCount,
				D3D12_HEAP_TYPE_DEFAULT,
				D3D12_RESOURCE_STATE_INDEX_BUFFER,
				&primitive->_indexUploadBuffer
			);

			// 인덱스 버퍼 뷰 설정
			primitive->_indexBufferView.BufferLocation = primitive->_indexBuffer->GetGPUVirtualAddress();
			primitive->_indexBufferView.Format = DXGI_FORMAT_R32_UINT;
			primitive->_indexBufferView.SizeInBytes = sizeof(UINT) * primitive->_indexCount;
		}
	}

	// 2. [추가] 애니메이션 뼈대용 상수 버퍼(Constant Buffer) 생성
	//    이 버퍼는 매 프레임 CPU에서 갱신되므로 D3D12_HEAP_TYPE_UPLOAD로 생성합니다.
	if (!_joints.empty() && !_bone_palette_buffer)
	{
		// 뼈대 개수 * 행렬 크기(64 byte)
		UINT element_size = sizeof(DirectX::XMFLOAT4X4);
		UINT buffer_size = (UINT)(_joints.size() * element_size);

		// 256바이트 정렬 (CBV 요구사항)
		buffer_size = (buffer_size + 255) & ~255;

		D3D12_HEAP_PROPERTIES heap_prop = {};
		heap_prop.Type = D3D12_HEAP_TYPE_UPLOAD;
		heap_prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
		heap_prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
		heap_prop.CreationNodeMask = 1;
		heap_prop.VisibleNodeMask = 1;

		D3D12_RESOURCE_DESC res_desc = {};
		res_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		res_desc.Alignment = 0;
		res_desc.Width = buffer_size;
		res_desc.Height = 1;
		res_desc.DepthOrArraySize = 1;
		res_desc.MipLevels = 1;
		res_desc.Format = DXGI_FORMAT_UNKNOWN;
		res_desc.SampleDesc.Count = 1;
		res_desc.SampleDesc.Quality = 0;
		res_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
		res_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

		// 버퍼 리소스 생성
		HRESULT hr = device->CreateCommittedResource(
			&heap_prop,
			D3D12_HEAP_FLAG_NONE,
			&res_desc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&_bone_palette_buffer)
		);

		if (FAILED(hr))
		{
			// 에러 처리 (로그 출력 등)
			// CERROR("Failed to create bone palette buffer.");
		}
		else
		{
			// 디버깅용 이름 설정
			_bone_palette_buffer->SetName(L"BonePaletteBuffer");
		}
	}

	// 3. 첫 번째 프리미티브 정보를 기본 클래스에 복사 (기존 로직)
	if (!_primitives.empty())
	{
		const auto& first_primitive = _primitives[0];
		_vertexBufferView = first_primitive->_vertexBufferView;
		_indexBufferView = first_primitive->_indexBufferView;

		_indices.resize(first_primitive->_indexCount);
		memcpy(_indices.data(), first_primitive->_indices.data(), sizeof(UINT) * first_primitive->_indexCount);
	}

	// 업로드 완료 플래그 설정
	_isUploaded = true;
}


void ReadGLTFMesh::render(ID3D12GraphicsCommandList* commandList)
{
	if (!_isUploaded) return;

	if (_is_animated) // 애니메이션 메쉬인 경우 스키닝 렌더링 호출
	{
		render_skinned(commandList);
	}

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

void ReadGLTFMesh::update_animation(float& delta_time, int clip_index)
{
	// 유효하지 않은 클립 인덱스 체크
	if (clip_index < 0 || clip_index >= _animations.size()) return;

	const AnimationClip& clip = _animations[clip_index];

	// 1. 시간 갱신 (Looping 처리)
	//_current_animation_time += delta_time;
	if (clip._duration > 0.0f) {
		delta_time = fmod(delta_time, clip._duration);
	}

	// 2. 채널별 키프레임 보간 수행
	for (const auto& channel : clip._channels)
	{
		if (channel._keyframes.empty()) continue;

		NodeInfo& target_node = _nodes[channel._node_index];

		// 현재 시간에 맞는 키프레임 인덱스 찾기
		size_t prev_idx = 0;
		size_t next_idx = 0;

		if (channel._keyframes.size() == 1) {
			prev_idx = next_idx = 0;
		}
		else {
			for (size_t i = 0; i < channel._keyframes.size() - 1; ++i) {
				if (delta_time >= channel._keyframes[i]._time &&
					delta_time < channel._keyframes[i + 1]._time) {
					prev_idx = i;
					next_idx = i + 1;
					break;
				}
			}
			if (delta_time >= channel._keyframes.back()._time) {
				prev_idx = next_idx = channel._keyframes.size() - 1;
			}
		}

		const Keyframe& prev_key = channel._keyframes[prev_idx];
		const Keyframe& next_key = channel._keyframes[next_idx];

		float duration = next_key._time - prev_key._time;
		float t = (duration > 0.0f) ? (delta_time - prev_key._time) / duration : 0.0f;

		XMVECTOR final_value;

		// 보간 방식에 따른 계산
		if (channel._interpolation == AnimationInterpolation::Step)
		{
			final_value = XMLoadFloat4(&prev_key._value);
		}
		else if (channel._interpolation == AnimationInterpolation::CubicSpline)
		{
			float t2 = t * t;
			float t3 = t2 * t;

			XMVECTOR p0 = XMLoadFloat4(&prev_key._value);
			XMVECTOR p1 = XMLoadFloat4(&next_key._value);
			XMVECTOR m0 = XMLoadFloat4(&prev_key._out_tangent) * duration;
			XMVECTOR m1 = XMLoadFloat4(&next_key._in_tangent) * duration;

			final_value = (2 * t3 - 3 * t2 + 1) * p0 +
				(t3 - 2 * t2 + t) * m0 +
				(-2 * t3 + 3 * t2) * p1 +
				(t3 - t2) * m1;
		}
		else // Linear
		{
			XMVECTOR v0 = XMLoadFloat4(&prev_key._value);
			XMVECTOR v1 = XMLoadFloat4(&next_key._value);

			if (channel._path == "rotation") {
				final_value = XMQuaternionSlerp(v0, v1, t);
			}
			else {
				final_value = XMVectorLerp(v0, v1, t);
			}
		}

		// 3. 노드 상태 업데이트
		if (channel._path == "translation") {
			XMStoreFloat3(&target_node._translation, final_value);
		}
		else if (channel._path == "rotation") {
			XMStoreFloat4(&target_node._rotation, final_value);
		}
		else if (channel._path == "scale") {
			XMStoreFloat3(&target_node._scale, final_value);
		}
	}

	// 4. 노드 계층 구조 전체 갱신 (Local -> Global)
	for (size_t i = 0; i < _nodes.size(); ++i) {
		if (_nodes[i]._parent_index == -1) {
			update_node_hierarchy((int)i, XMMatrixIdentity());
		}
	}

	// 5. 스키닝 행렬(Matrix Palette) 계산
	for (size_t i = 0; i < _joints.size(); ++i)
	{
		int node_idx = _joints[i];
		XMMATRIX global_transform = XMLoadFloat4x4(&_nodes[node_idx]._global_transform);
		XMMATRIX inverse_bind_matrix = XMLoadFloat4x4(&_skeleton[i]._inverse_bind_matrix);

		// Final = InverseBindMatrix * GlobalTransform
		XMMATRIX final_matrix = inverse_bind_matrix * global_transform;

		// GPU 전송을 위해 Transpose (Row-Major)
		XMStoreFloat4x4(&_final_bone_transforms[i], XMMatrixTranspose(final_matrix));
		//XMStoreFloat4x4(&_final_bone_transforms[i], final_matrix);
	}

	// 6. GPU 상수 버퍼 업로드
	if (_bone_palette_buffer)
	{
		void* mapped_data = nullptr;
		D3D12_RANGE read_range = { 0, 0 };

		if (SUCCEEDED(_bone_palette_buffer->Map(0, &read_range, &mapped_data)))
		{
			memcpy(mapped_data, _final_bone_transforms.data(), _final_bone_transforms.size() * sizeof(DirectX::XMFLOAT4X4));
			_bone_palette_buffer->Unmap(0, nullptr);
		}
	}
}

void ReadGLTFMesh::render_skinned(ID3D12GraphicsCommandList* commandList)
{
	// 뼈대 행렬 팔레드 GPU 상수 버퍼에 바인딩
	// SkinnedRootSignatureGenerator에서 뼈대 버퍼는 8번 파라미터 (b4)로 정의
	if (_bone_palette_buffer)
	{
		commandList->SetGraphicsRootConstantBufferView(8, _bone_palette_buffer->GetGPUVirtualAddress());
	}
}

void ReadGLTFMesh::read_static_mesh(const std::string& filePath)
{
	// DW설명 : 파일 경로를 이름으로 설정
	set_name(filePath);

	json gltf_json;
	std::vector<char> binary_buffer;

	//CLOG("현재 디렉토리: " << std::filesystem::current_path() << std::endl);
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
	// skins 배열을 파싱 -> 뼈대 정보 관련 파싱
	load_skins(gltf_json, binary_buffer);

	// animations 배열을 파싱하여 키프레임 데이터를
	// _animations 멤버 변수에 저장
	load_animations(gltf_json, binary_buffer);
	load_nodes(gltf_json);

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
				// T-포즈를 유지하고 JOINTS_0, WEIGHTS_0를 읽는 헬퍼 함수
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

	// 우리는 이 예제에서 0번 스킨만 로드한다고 가정 -> 이것도 수정예정, 아직 여러개 있는 경우를 못찾음
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
	// 이제 72개 뿐만 아니라 다르게 불러오기 가능 수정함
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
	std::vector<DirectX::XMFLOAT4X4> inverse_bind_matrices = get_attribute_data<DirectX::XMFLOAT4X4>(gltf_json, binary_buffer, ibm_accessor_index);

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


		// DW설명 : 이 부분 절대 놓치지 말 것 이거 때문에 후..
		// 1. 파일에서 읽은 원본 행렬 (오른손 좌표계, Column-Major)
		// 메모리 레이아웃 상 Translation이 끝에 있으므로, DXMath(Row-Major)로 읽으면 포맷은 맞음
		DirectX::XMMATRIX gltf_matrix = DirectX::XMLoadFloat4x4(&inverse_bind_matrices[i]);

		// 2. [추가] 좌표계 변환 (Right-Handed -> Left-Handed)
		// Z축을 반전시키는 스케일 행렬 생성
		DirectX::XMMATRIX z_flip = DirectX::XMMatrixScaling(1.0f, 1.0f, -1.0f);

		// 변환 공식: LHS_Matrix = Scale(1,1,-1) * RHS_Matrix * Scale(1,1,-1)
		// 이렇게 하면 Z와 관련된 회전/이동 성분들의 부호가 올바르게 반전됩니다.
		DirectX::XMMATRIX converted_matrix = z_flip * gltf_matrix * z_flip;

		// 3. 변환된 행렬 저장 (전치 X)
		DirectX::XMStoreFloat4x4(&_skeleton[i]._inverse_bind_matrix, converted_matrix);
		
		//// glTF는 열 우선(Column-Major), DirectX는 행 우선(Row-Major)
		//// 로드한 행렬을 전치(Transpose)하여 저장
		//DirectX::XMMATRIX col_major_matrix = DirectX::XMLoadFloat4x4(&inverse_bind_matrices[i]);
		//DirectX::XMMATRIX row_major_matrix = DirectX::XMMatrixTranspose(col_major_matrix);
		////DirectX::XMStoreFloat4x4(&_skeleton[i]._inverse_bind_matrix, row_major_matrix);
		//DirectX::XMStoreFloat4x4(&_skeleton[i]._inverse_bind_matrix, col_major_matrix);	

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
	if (!gltf_json.contains("animations")) return;

	const json& animations_node = gltf_json["animations"];
	_animations.resize(animations_node.size());

	for (size_t i = 0; i < animations_node.size(); ++i)
	{
		const json& anim_node = animations_node[i];
		AnimationClip& clip = _animations[i];

		clip._name = anim_node.value("name", "anim_" + std::to_string(i));
		clip._duration = 0.0f;

		// 1. Samplers 데이터 미리 로드
		struct SamplerInfo {
			std::vector<float> times;
			std::vector<float> values;
			AnimationInterpolation interpolation;
			int type_component_count; // VEC3=3, VEC4=4
		};
		std::vector<SamplerInfo> samplers_data;

		const json& samplers_json = anim_node["samplers"];
		samplers_data.resize(samplers_json.size());

		for (size_t s = 0; s < samplers_json.size(); ++s)
		{
			const json& sampler_node = samplers_json[s];

			// Input (Time) Accessor 읽기
			int input_accessor_index = sampler_node["input"].get<int>();
			samplers_data[s].times = get_attribute_data<float>(gltf_json, binary_buffer, input_accessor_index);

			if (!samplers_data[s].times.empty())
			{
				float max_time = samplers_data[s].times.back();
				if (max_time > clip._duration) clip._duration = max_time;
			}

			// Interpolation 모드 설정
			std::string interpolation_str = sampler_node.value("interpolation", "LINEAR");
			samplers_data[s].interpolation = string_to_interpolation(interpolation_str);

			// Output (Values) Accessor 읽기
			int output_accessor_index = sampler_node["output"].get<int>();
			const json& output_accessor = gltf_json["accessors"][output_accessor_index];

			std::string type_str = output_accessor["type"].get<std::string>();
			int component_count = 0;

			// 타입에 따라 데이터를 올바르게 읽어서 flat float vector로 변환
			if (type_str == "SCALAR")
			{
				component_count = 1;
				samplers_data[s].values = get_attribute_data<float>(gltf_json, binary_buffer, output_accessor_index);
			}
			else if (type_str == "VEC3")
			{
				component_count = 3;
				std::vector<XMFLOAT3> vec3_data = get_attribute_data<XMFLOAT3>(gltf_json, binary_buffer, output_accessor_index);
				samplers_data[s].values.resize(vec3_data.size() * 3);
				memcpy(samplers_data[s].values.data(), vec3_data.data(), vec3_data.size() * sizeof(XMFLOAT3));
			}
			else if (type_str == "VEC4")
			{
				component_count = 4;
				std::vector<XMFLOAT4> vec4_data = get_attribute_data<XMFLOAT4>(gltf_json, binary_buffer, output_accessor_index);
				samplers_data[s].values.resize(vec4_data.size() * 4);
				memcpy(samplers_data[s].values.data(), vec4_data.data(), vec4_data.size() * sizeof(XMFLOAT4));
			}

			samplers_data[s].type_component_count = component_count;
		}

		// 2. Channels 파싱 및 데이터 매핑
		const json& channels_json = anim_node["channels"];
		for (const auto& channel_node : channels_json)
		{
			AnimationChannel channel;

			channel._node_index = channel_node["target"]["node"].get<int>();
			channel._path = channel_node["target"]["path"].get<std::string>();

			int sampler_index = channel_node["sampler"].get<int>();
			const SamplerInfo& sampler = samplers_data[sampler_index];

			channel._interpolation = sampler.interpolation;

			size_t keyframe_count = sampler.times.size();
			channel._keyframes.resize(keyframe_count);

			int values_per_keyframe = (channel._interpolation == AnimationInterpolation::CubicSpline) ? 3 : 1;
			int stride = sampler.type_component_count;

			// [수정] Weights 채널을 위한 Stride 재계산
			// Weights는 SCALAR 타입이지만, 모프 타겟 개수(N)만큼 값이 연속됨
			if (channel._path == "weights" && keyframe_count > 0)
			{
				// 전체 값 개수 / (프레임 수 * 키프레임당 세트 수) = 모프 타겟 개수
				size_t total_values = sampler.values.size();
				stride = (int)(total_values / (keyframe_count * values_per_keyframe));
			}

			for (size_t k = 0; k < keyframe_count; ++k)
			{
				channel._keyframes[k]._time = sampler.times[k];

				// 현재 키프레임의 데이터 시작 위치
				size_t base_data_index = k * values_per_keyframe * stride;

				auto read_and_convert = [&](size_t offset_index) -> DirectX::XMFLOAT4 {
					DirectX::XMFLOAT4 result(0, 0, 0, 0);

					// Translation, Rotation, Scale 처리
					if (channel._path != "weights")
					{
						float x = sampler.values[offset_index + 0];
						float y = (stride > 1) ? sampler.values[offset_index + 1] : 0.0f;
						float z = (stride > 2) ? sampler.values[offset_index + 2] : 0.0f;
						float w = 0.0f;
						if (stride > 3) w = sampler.values[offset_index + 3];

						if (channel._path == "translation")
						{
							//result = DirectX::XMFLOAT4(x, y, -z, 0.0f); // Z 반전
							result = DirectX::XMFLOAT4(x, y, -z, 0.0f); // Z 반전
						}
						else if (channel._path == "rotation")
						{
							//result = DirectX::XMFLOAT4(-x, -y, z, w); // 회전축 반전
							result = DirectX::XMFLOAT4(-x, -y, z, w); // 회전축 반전
						}
						else if (channel._path == "scale")
						{
							result = DirectX::XMFLOAT4(x, y, z, 0.0f);
						}
					}
					// [수정] Weights 처리 (최대 4개 모프 타겟 지원)
					else if (channel._path == "weights")
					{
						float w0 = (stride > 0) ? sampler.values[offset_index + 0] : 0.0f;
						float w1 = (stride > 1) ? sampler.values[offset_index + 1] : 0.0f;
						float w2 = (stride > 2) ? sampler.values[offset_index + 2] : 0.0f;
						float w3 = (stride > 3) ? sampler.values[offset_index + 3] : 0.0f;

						// 모프 타겟 가중치는 좌표계 변환 불필요
						result = DirectX::XMFLOAT4(w0, w1, w2, w3);
					}
					return result;
					};

				if (channel._interpolation == AnimationInterpolation::CubicSpline)
				{
					channel._keyframes[k]._in_tangent = read_and_convert(base_data_index + 0 * stride);
					channel._keyframes[k]._value = read_and_convert(base_data_index + 1 * stride);
					channel._keyframes[k]._out_tangent = read_and_convert(base_data_index + 2 * stride);
				}
				else
				{
					channel._keyframes[k]._value = read_and_convert(base_data_index);
				}
			}

			clip._channels.push_back(channel);
		}
	}
}

void ReadGLTFMesh::process_skinned_mesh(const json& gltf_json, const std::vector<char>& binary_buffer, const json& mesh)
{
	for (const auto& primitive_json : mesh["primitives"])
	{
		_primitives.emplace_back(std::make_unique<GltfPrimitive>());
		auto& primitive = _primitives.back();

		// 1. 기본 속성 읽기 (Position, Normal, UV, Tangent)
		// get_attribute_data는 내부적으로 Stride를 처리하므로 안전합니다.
		std::vector<XMFLOAT3> positions = get_attribute_data<XMFLOAT3>(gltf_json, binary_buffer, primitive_json["attributes"]["POSITION"]);

		std::vector<XMFLOAT3> normals;
		if (primitive_json["attributes"].contains("NORMAL"))
			normals = get_attribute_data<XMFLOAT3>(gltf_json, binary_buffer, primitive_json["attributes"]["NORMAL"]);

		std::vector<XMFLOAT2> texcoords;
		if (primitive_json["attributes"].contains("TEXCOORD_0"))
			texcoords = get_attribute_data<XMFLOAT2>(gltf_json, binary_buffer, primitive_json["attributes"]["TEXCOORD_0"]);

		std::vector<XMFLOAT4> tangents;
		if (primitive_json["attributes"].contains("TANGENT"))
			tangents = get_attribute_data<XMFLOAT4>(gltf_json, binary_buffer, primitive_json["attributes"]["TANGENT"]);

		// 2. 스키닝 속성 읽기 (JOINTS_0, WEIGHTS_0)
		std::vector<XMUINT4> joint_indices_vec;
		std::vector<XMFLOAT4> weights_vec;

		// [수정] JOINTS_0 읽기 (u8, u16 대응 + Stride 적용)
		if (primitive_json["attributes"].contains("JOINTS_0"))
		{
			int accessor_idx = primitive_json["attributes"]["JOINTS_0"];
			const json& accessor = gltf_json["accessors"][accessor_idx];
			const json& buffer_view = gltf_json["bufferViews"][accessor["bufferView"].get<int>()];

			const char* buffer_start = binary_buffer.data() + buffer_view.value("byteOffset", 0) + accessor.value("byteOffset", 0);
			int count = accessor["count"];
			int component_type = accessor["componentType"]; // 5121(u8), 5123(u16)

			// [핵심] 버퍼 뷰의 Stride를 가져옵니다. (없으면 0)
			int byte_stride = buffer_view.value("byteStride", 0);

			joint_indices_vec.resize(count);

			for (int i = 0; i < count; ++i)
			{
				const char* current_ptr = nullptr;

				if (component_type == 5123) // Unsigned Short (2 byte)
				{
					// Stride가 0이면(Tight) 요소 크기(2*4=8) 사용, 아니면 Stride 사용
					int step = (byte_stride > 0) ? byte_stride : (sizeof(uint16_t) * 4);

					// i번째 요소의 정확한 메모리 위치 계산
					current_ptr = buffer_start + i * step;

					const uint16_t* ptr = reinterpret_cast<const uint16_t*>(current_ptr);
					joint_indices_vec[i] = XMUINT4(ptr[0], ptr[1], ptr[2], ptr[3]);
				}
				else if (component_type == 5121) // Unsigned Byte (1 byte)
				{
					// Stride가 0이면 요소 크기(1*4=4) 사용
					int step = (byte_stride > 0) ? byte_stride : (sizeof(uint8_t) * 4);

					// i번째 요소의 정확한 메모리 위치 계산
					current_ptr = buffer_start + i * step;

					const uint8_t* ptr = reinterpret_cast<const uint8_t*>(current_ptr);
					joint_indices_vec[i] = XMUINT4(ptr[0], ptr[1], ptr[2], ptr[3]);
				}
			}
		}

		// WEIGHTS_0 읽기
		if (primitive_json["attributes"].contains("WEIGHTS_0"))
		{
			weights_vec = get_attribute_data<XMFLOAT4>(gltf_json, binary_buffer, primitive_json["attributes"]["WEIGHTS_0"]);
		}

		// 3. 정점 조립 및 좌표계 변환 (Right -> Left Handed)
		primitive->_vertexCount = (UINT)positions.size();
		primitive->_skinned_vertices.resize(primitive->_vertexCount);

		for (size_t i = 0; i < primitive->_vertexCount; ++i)
		{
			auto& v = primitive->_skinned_vertices[i];

			// Position: Z 반전 (x, y, -z) -> 왼손 좌표계로 바꾸는 과정임
			v._position = XMFLOAT3(positions[i].x, positions[i].y, -positions[i].z);

			// Normal: Z 반전
			if (i < normals.size())
				v._normal = XMFLOAT3(normals[i].x, normals[i].y, -normals[i].z);
			else
				v._normal = XMFLOAT3(0.0f, 1.0f, 0.0f);

			// Tangent: Z 반전
			if (i < tangents.size())
				v._tangent = XMFLOAT4(tangents[i].x, tangents[i].y, -tangents[i].z, tangents[i].w);
			else
				v._tangent = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);

			// UV
			if (i < texcoords.size())
				v._texCoord = texcoords[i];
			else
				v._texCoord = XMFLOAT2(0.0f, 0.0f);

			// Skinning Data (배열에 값 대입)
			if (i < joint_indices_vec.size()) {
				v._boneIndices[0] = joint_indices_vec[i].x;
				v._boneIndices[1] = joint_indices_vec[i].y;
				v._boneIndices[2] = joint_indices_vec[i].z;
				v._boneIndices[3] = joint_indices_vec[i].w;
			}
			else {
				v._boneIndices[0] = v._boneIndices[1] = v._boneIndices[2] = v._boneIndices[3] = 0;
			}

			if (i < weights_vec.size()) {
				v._boneWeights[0] = weights_vec[i].x;
				v._boneWeights[1] = weights_vec[i].y;
				v._boneWeights[2] = weights_vec[i].z;
				v._boneWeights[3] = weights_vec[i].w;
			}
			else {
				v._boneWeights[0] = v._boneWeights[1] = v._boneWeights[2] = v._boneWeights[3] = 0.0f;
			}
		}

		// 4. 인덱스 버퍼 처리 (Winding Order Flip 포함)
		if (primitive_json.contains("indices"))
		{
			const json& accessor = gltf_json["accessors"][primitive_json["indices"].get<size_t>()];
			const json& buffer_view = gltf_json["bufferViews"][accessor["bufferView"].get<size_t>()];
			const char* data_ptr = binary_buffer.data() + buffer_view.value("byteOffset", 0) + accessor.value("byteOffset", 0);

			primitive->_indexCount = accessor["count"];
			primitive->_indices.resize(primitive->_indexCount);

			if (accessor["componentType"] == 5123) { // unsigned short
				const uint16_t* p_indices = reinterpret_cast<const uint16_t*>(data_ptr);
				for (size_t i = 0; i < primitive->_indexCount; ++i) {
					primitive->_indices[i] = static_cast<UINT>(p_indices[i]);
				}
			}
			else if (accessor["componentType"] == 5125) { // unsigned int -> 너무 헷갈리는데 이거 나중에 enum으로 바꿔야 하나?
				memcpy(primitive->_indices.data(), data_ptr, primitive->_indexCount * sizeof(UINT));
			}

			// DX12 Winding Order Flip (0, 1, 2 -> 0, 2, 1) -> gltf가 기본적으로 CCW이므로 CW로 바꿔줘야 함
			for (size_t i = 0; i < primitive->_indexCount; i += 3) {
				std::swap(primitive->_indices[i + 1], primitive->_indices[i + 2]);
			}
		}

		BoundingOrientedBox::CreateFromPoints(primitive->_orientedBoundingBox, primitive->_skinned_vertices.size(), &primitive->_skinned_vertices[0]._position, sizeof(GltfSkinnedVertex));
		primitive->_materialIndex = primitive_json.value("material", -1);
	}
}

AnimationInterpolation ReadGLTFMesh::string_to_interpolation(const std::string& str)
{
	if (str == "LINEAR") 
	{
		return AnimationInterpolation::Linear;
	}
	if (str == "STEP") 
	{
		return AnimationInterpolation::Step;
	}
	if (str == "CUBICSPLINE") 
	{
		return AnimationInterpolation::CubicSpline;
	}
	return AnimationInterpolation::Linear;
}

void ReadGLTFMesh::load_nodes(const json& gltf_json)
{
	const auto& nodes_json = gltf_json["nodes"];
	_nodes.resize(nodes_json.size());

	for (size_t i = 0; i < nodes_json.size(); ++i)
	{
		const auto& node_json = nodes_json[i];
		NodeInfo& node_info = _nodes[i];

		// 1. 초기 TRS 설정
		// glTF(Right-Handed) -> DX12(Left-Handed) 좌표계 변환 적용

		// Translation: Z 반전
		if (node_json.contains("translation")) {
			node_info._translation = {
				node_json["translation"][0].get<float>(),
				node_json["translation"][1].get<float>(),
				-node_json["translation"][2].get<float>()
			};
		}

		// Rotation: X, Y 반전 (Quaternion)
		if (node_json.contains("rotation")) {
			node_info._rotation = {
				-node_json["rotation"][0].get<float>(),
				-node_json["rotation"][1].get<float>(),
				node_json["rotation"][2].get<float>(),
				node_json["rotation"][3].get<float>()
			};
		}

		// Scale: 변환 없음
		if (node_json.contains("scale")) {
			node_info._scale = {
				node_json["scale"][0].get<float>(),
				node_json["scale"][1].get<float>(),
				node_json["scale"][2].get<float>()
			};
		}

		// 2. 계층 구조 설정 (자식 -> 부모 연결)
		if (node_json.contains("children")) {
			for (const auto& child_idx_json : node_json["children"]) {
				int child_idx = child_idx_json.get<int>();
				node_info._children.push_back(child_idx);

				// 자식 노드에게 부모 인덱스 설정
				_nodes[child_idx]._parent_index = (int)i;
			}
		}

		// 초기 전역 행렬은 단위 행렬로 설정 (나중에 update_animation에서 계산됨)
		XMStoreFloat4x4(&node_info._global_transform, XMMatrixIdentity());
	}
}

void ReadGLTFMesh::update_node_hierarchy(int node_index, const DirectX::XMMATRIX& parent_transform)
{
	NodeInfo& node = _nodes[node_index];

	// Local Transform (Scale * Rotation * Translation)
	XMMATRIX translation_mat = XMMatrixTranslation(node._translation.x, node._translation.y, node._translation.z);
	XMMATRIX rotation_mat = XMMatrixRotationQuaternion(XMLoadFloat4(&node._rotation));
	XMMATRIX scale_mat = XMMatrixScaling(node._scale.x, node._scale.y, node._scale.z);

	XMMATRIX local_transform = scale_mat * rotation_mat * translation_mat;

	// Global Transform (Local * Parent)
	XMMATRIX global_transform = local_transform * parent_transform;
	XMStoreFloat4x4(&node._global_transform, global_transform);

	// 자식 노드들에게 전파
	for (int child_idx : node._children) 
	{
		update_node_hierarchy(child_idx, global_transform);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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
		XMFLOAT4X4 mat4x4 = XMFLOAT4X4(
			mat[0], mat[1], mat[2], mat[3],
			mat[4], mat[5], mat[6], mat[7],
			mat[8], mat[9], mat[10], mat[11],
			mat[12], mat[13], mat[14], mat[15]
		);
		local_matrix = XMLoadFloat4x4(&mat4x4);
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
		//// 불러온 정점 데이터의 첫 5개 위치 값을 출력
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
