#include "stdafx.h"
#include "AnimationComponent.h"
#include "ReadGLTFMesh.h"
#include "GameObject.h"
#include "GameFramework.h"

AnimationComponent::AnimationComponent() : Behavior("AnimationComponent")
{
}

void AnimationComponent::late_update(float deltaTime)
{
	_nowAnimationTime += deltaTime * _animationSpeed;
	float timeBeforeUpdate = _nowAnimationTime;

	// 현재 렌더링 중인 메쉬를 가져와서 애니메이션 업데이트
	auto renderComp = game_object()->get_component<RenderComponent>();
	if (!renderComp) return;

	//auto mesh = std::dynamic_pointer_cast<ReadGLTFMesh>(renderComp->mesh());
	//if (mesh && !_nowAnimationName.empty())
	//{
	//	mesh->update_animation(_nowAnimationTime, _nowAnimationName);
	//}

	auto mesh = _stateMeshMap.find(_currentState);
	auto anim = _stateAnimMap.find(_currentState);

	// [추가된 부분] 맵에 상태가 등록되어 있지 않으면 에러 로그를 띄우고 리턴합니다.
	if (mesh == _stateMeshMap.end() || anim == _stateAnimMap.end())
	{
		// _currentState를 int로 변환하여 어떤 상태가 누락되었는지 확인
		CERROR("Animation state not found: " << static_cast<int>(_currentState));
		return;
	}

	std::shared_ptr<Mesh> targetMesh = mesh->second;

	// 사용하려는 메쉬(targetMesh)가 현재 버퍼의 주인(_bufferedMesh)과 다른가?
	// 다르다면 버퍼 크기가 맞지 않을 수 있으므로 버퍼를 재생성해야 함!
	if (targetMesh != _bufferedMesh)
	{
		change_mesh(targetMesh);
	}

	auto glTF_mesh = std::dynamic_pointer_cast<ReadGLTFMesh>(targetMesh);
	if (!glTF_mesh) return;

	if(_bone_palette_buffer)
	{
		std::dynamic_pointer_cast<ReadGLTFMesh>(mesh->second)->update_animation(_nowAnimationTime, anim->second, _bone_palette_buffer, _isLoop);
	}
	else
	{
		std::dynamic_pointer_cast<ReadGLTFMesh>(mesh->second)->update_animation(_nowAnimationTime, anim->second, _isLoop);
	}

	// [핵심] 시간이 줄어들었다면 리셋된 것이므로 종료 플래그 설정
	if (_nowAnimationTime < timeBeforeUpdate) {
		_isFinished = true;
	}
}

void AnimationComponent::set_state(common::packet::OBJECT_STATE state, bool isLoop)
{
	if (_currentState == state) return;
	_currentState = state;
	_isFinished = false;
	_nowAnimationTime = 0.f;
	_isLoop = isLoop;

	// 1. 메쉬 교체 (등록된 메쉬가 있을 경우만)
	auto mIt = _stateMeshMap.find(state);
	if (mIt != _stateMeshMap.end() && mIt->second) {
		change_mesh(mIt->second);
	}

	// 2. 애니메이션 교체
	auto aIt = _stateAnimMap.find(state);
	if (aIt != _stateAnimMap.end()) {
		change_animation(aIt->second);
	}
}

void AnimationComponent::add_state_mapping(common::packet::OBJECT_STATE state, const std::string& animName,
	std::shared_ptr<Mesh> mesh)
{
	_stateAnimMap[state] = animName;
	if (mesh) _stateMeshMap[state] = mesh;
}

float AnimationComponent::get_anim_duration() const
{
	auto anim = _stateAnimMap.find(_currentState);
	if (anim == _stateAnimMap.end()) return 0.0f;

	auto mesh = _stateMeshMap.find(_currentState);
	if (mesh == _stateMeshMap.end()) return 0.0f;

	auto gltf_mesh = std::dynamic_pointer_cast<ReadGLTFMesh>(mesh->second);
	if (!gltf_mesh) return 0.0f;

	return gltf_mesh->get_animation_duration(anim->second);
}

bool AnimationComponent::is_anim_finished() const
{
	return _isFinished;
}

void AnimationComponent::set_anim_speed(float wantSpeed)
{
	if (wantSpeed < 0.f)
	{
		wantSpeed = 0.f;
	}
	_animationSpeed = wantSpeed;
}

void AnimationComponent::change_animation(std::string name)
{
	if (_nowAnimationName == name)
	{
		return;
	}
	_nowAnimationName = name;
	_nowAnimationTime = 0.f;
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
		_bone_palette_buffer.Reset(); // 더 큰 공간이 필요할 때만 재할당
	}

	// DW벼르기 : 뼈 행렬을 진짜 바꿔야 하는 경우에는 기다리고 생성하는 것이 안전하지만
	// 만약 바꿔야 하는 상황이 많아 문제가 발생한다면? -> 이 놈을 먼저 조져볼 예정
	GameFramework::instance()->WaitForGpuComplete();

	if (joint_size && !_bone_palette_buffer)
	{
		// 임시 객체의 주소를 바로 딸 수 없으므로, 변수로 먼저 만들어두기
		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(buffer_size);

		HRESULT hr = GameFramework::instance()->device()->CreateCommittedResource(
			&heapProps,         // 이제 변수의 주소를 넘기므로 안전
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,        // 변수의 주소
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&_bone_palette_buffer) // 이거 comptr에 &연산자 오버로딩이 되어있어 동작함
		);

		if (FAILED(hr))
		{
			// 에러 처리
			return;
		}

		_bone_palette_buffer->SetName(L"BonePaletteBuffer");
	}
}