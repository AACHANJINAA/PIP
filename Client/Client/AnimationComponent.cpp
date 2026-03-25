#include "stdafx.h"
#include "AnimationComponent.h"
#include "ReadGLTFMesh.h"
#include "GameObject.h"
#include "GameFramework.h"
#include "ResourceManager.h"

AnimationComponent::AnimationComponent() : Behavior("AnimationComponent")
{
}

void AnimationComponent::late_update(float deltaTime)
{
	if (_currentName.empty())
	{
		return;
	}

	// 현재 애니메이션 시간 갱신
	_nowAnimationTime += deltaTime * _animationSpeed;
	float timeBeforeUpdate = _nowAnimationTime;

	// 애니메이션 리소스 찾기
	auto it = _animResources.find(_currentName);
	if (it == _animResources.end())
	{
		CERROR("<Animation Name mapping Mesh not found>  currentName: " << _currentName);
		return;
	}

	auto glTF_mesh = std::dynamic_pointer_cast<ReadGLTFMesh>(it->second.mesh);
	if (!glTF_mesh) 
	{
		return;
	}


	// 애니메이션 업데이트 및 본 행렬 계산
	if (_bone_palette_buffer) {
		glTF_mesh->update_animation(_nowAnimationTime, _nowAnimationName, _bone_palette_buffer, _isLoop);
	}
	else {
		//glTF_mesh->update_animation(_nowAnimationTime, _nowAnimationName, _isLoop);
	}

	// DW설명 : 이제 애니메이션 업데이트에 지금 들고있는 뼈대 행렬 벡터를 넘겨서 갱신하도록 함 -> 애니메이션 컴포넌트가 뼈대 행렬을 관리하는 형태로 변경
	glTF_mesh->update_animation(_nowAnimationTime, _nowAnimationName, _boneTransforms, _isLoop);


	// 종료 판정
	if (!_isLoop && _nowAnimationTime < timeBeforeUpdate) {
		_isFinished = true;
	}
}

void AnimationComponent::add_animation(const std::string& want_name, const std::shared_ptr<Mesh>& mesh,
                                       const std::string& actualAnimName)
{
	auto gltf_mesh = std::dynamic_pointer_cast<ReadGLTFMesh>(mesh);
	if (!gltf_mesh) {
		CERROR("Mesh is not a GLTF mesh: " << want_name);
		return;
	}

	// 1. 실제 애니메이션 이름 결정
	std::string targetName = actualAnimName.empty() ? want_name : actualAnimName;

	// [검증] 실제 애니메이션 이름이 있는지 확인
	if (!gltf_mesh->has_animation(targetName)) {
		auto names = gltf_mesh->get_animation_names();
		if (!names.empty()) {
			CLOG("Anim '" << targetName << "' not found. Using: " << names[0]);
			targetName = names[0];
		}
	}

	// 3. 최종 매핑 저장
	_animResources[want_name] = { mesh, targetName };
}

void AnimationComponent::play(const std::string& name, bool isLoop, float speed)
{
	// 이미 재생 중인 애니메이션이면 설정값만 업데이트하고 리턴
	if (_currentName == name) {
		_isLoop = isLoop;
		_animationSpeed = speed;
		return;
	}

	auto it = _animResources.find(name);
	if (it == _animResources.end()) {
		CERROR("Animation Alias not found: " << name);
		return;
	}

	_currentName = name;
	_nowAnimationName = it->second.actualName;
	_isLoop = isLoop;
	_animationSpeed = speed;
	_nowAnimationTime = 0.f;
	_isFinished = false;

	// 메쉬가 다르면 교체
	if (it->second.mesh != _bufferedMesh) {
		change_mesh(it->second.mesh);
	}
}

//void AnimationComponent::set_state(common::packet::OBJECT_STATE state, bool isLoop)
//{
//	if (_currentState == state) return;
//	_currentState = state;
//	_isFinished = false;
//	_nowAnimationTime = 0.f;
//	_isLoop = isLoop;
//
//	// 1. 메쉬 교체 (등록된 메쉬가 있을 경우만)
//	auto mIt = _stateMeshMap.find(state);
//	if (mIt != _stateMeshMap.end() && mIt->second) {
//		change_mesh(mIt->second);
//	}
//
//	// 2. 애니메이션 교체
//	auto aIt = _stateAnimMap.find(state);
//	if (aIt != _stateAnimMap.end()) {
//		change_animation(aIt->second);
//	}
//}

//void AnimationComponent::add_state_mapping(common::packet::OBJECT_STATE state, const std::string& animName,
//	std::shared_ptr<Mesh> mesh)
//{
//	_stateAnimMap[state] = animName;
//	if (mesh) _stateMeshMap[state] = mesh;
//}

float AnimationComponent::get_anim_duration() const
{
	auto it = _animResources.find(_currentName);
	if (it == _animResources.end()) return 0.0f;
	auto gltf = std::dynamic_pointer_cast<ReadGLTFMesh>(it->second.mesh);
	return gltf ? gltf->get_animation_duration(_nowAnimationName) : 0.0f;

	/*auto anim = _stateAnimMap.find(_currentState);
	if (anim == _stateAnimMap.end()) return 0.0f;

	auto mesh = _stateMeshMap.find(_currentState);
	if (mesh == _stateMeshMap.end()) return 0.0f;

	auto gltf_mesh = std::dynamic_pointer_cast<ReadGLTFMesh>(mesh->second);
	if (!gltf_mesh) return 0.0f;

	return gltf_mesh->get_animation_duration(anim->second);*/
}

void AnimationComponent::change_mesh(const std::shared_ptr<Mesh>& want_mesh)
{
	if (_currentMesh == want_mesh) return;
	_currentMesh = want_mesh;
	if (auto renderer = game_object()->get_component<RenderComponent>()) {
		renderer->set_mesh(want_mesh);
	}
	_nowAnimationTime = 0.f;

	// DW설명 : 여기서 애니메이션을 메쉬의 뼈의 개수에 맞게 초기화 해줌
	create_bone_palette_buffer(want_mesh);


	// 새로운 메쉬에 맞게 뼈대 행렬을 만들기
	auto gltf_mesh = std::dynamic_pointer_cast<ReadGLTFMesh>(want_mesh);
	DirectX::XMFLOAT4X4 identity_matrix;
	DirectX::XMStoreFloat4x4(&identity_matrix, DirectX::XMMatrixIdentity());
	_boneTransforms.assign(gltf_mesh->get_joint_count(), identity_matrix);	 // assign -> clear + resize

	// 애니메이션 컴포넌트에서 애니메이션 하기 위한 초기화 작업
	gltf_mesh->nodes_inout_set(_nodes);

	// 현재 버퍼가 이 메쉬용임을 기록
	_bufferedMesh = want_mesh;
}

void AnimationComponent::create_bone_palette_buffer(const std::shared_ptr<Mesh>& want_mesh)
{
	auto gltf_mesh = std::dynamic_pointer_cast<ReadGLTFMesh>(want_mesh);
	if (gltf_mesh == nullptr)
	{
		// 뼈가 없는 일반 메쉬이거나 캐스팅 실패 -> 버퍼 만들 필요 없음
		return;
	}

	size_t joint_size = gltf_mesh->get_joint_count();
	UINT element_size = sizeof(DirectX::XMFLOAT4X4);
	UINT buffer_size = (UINT)(joint_size * element_size);
	buffer_size = (buffer_size + 255) & ~255;

	if (_bone_palette_buffer != nullptr) {
		if (_bone_palette_buffer->GetDesc().Width >= buffer_size) {
			return;
		}
		//_bone_palette_buffer.Reset(); // 더 큰 공간이 필요할 때만 재할당 <- CJ 수정 : 이 코드로 인해 scene 전환시 GPU가 아직 이전 버퍼를 읽는 중인데도 CPU가 refcount를 0으로 만들어 버릴 수 있는 구조라 주석
	}

	// DW벼르기 : 뼈 행렬을 진짜 바꿔야 하는 경우에는 기다리고 생성하는 것이 안전하지만
	// 만약 바꿔야 하는 상황이 많아 문제가 발생한다면? -> 이 놈을 먼저 조져볼 예정
	//GameFramework::instance()->WaitForGpuComplete();

	//if (joint_size && !_bone_palette_buffer)
	//{
	//	// 임시 객체의 주소를 바로 딸 수 없으므로, 변수로 먼저 만들어두기
	//	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
	//	CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(buffer_size);

	//	HRESULT hr = GameFramework::instance()->device()->CreateCommittedResource(
	//		&heapProps,         // 이제 변수의 주소를 넘기므로 안전
	//		D3D12_HEAP_FLAG_NONE,
	//		&bufferDesc,        // 변수의 주소
	//		D3D12_RESOURCE_STATE_GENERIC_READ,
	//		nullptr,
	//		IID_PPV_ARGS(&_bone_palette_buffer) // 이거 comptr에 &연산자 오버로딩이 되어있어 동작함
	//	);

	//	if (FAILED(hr))
	//	{
	//		// 에러 처리
	//		return;
	//	}

	//	_bone_palette_buffer->SetName(L"BonePaletteBuffer");
	//}

	// 기존 버퍼는 즉시 Reset 금지: GPU가 아직 참조 중일 수 있음
	ComPtr<ID3D12Resource> old_buffer = _bone_palette_buffer;

	CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(buffer_size);

	ComPtr<ID3D12Resource> new_buffer;
	HRESULT hr = GameFramework::instance()->device()->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&new_buffer)
	);
	if (FAILED(hr))
	{
		return;
	}

	new_buffer->SetName(L"BonePaletteBuffer");
	_bone_palette_buffer = new_buffer;

	// 이전 버퍼는 fence 이후 해제
	if (old_buffer)
	{
		const UINT64 fenceValue = GameFramework::instance()->next_fence_value();
		ResourceManager::instance()->register_upload_buffer(old_buffer, fenceValue);
	}
}