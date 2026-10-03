#include "stdafx.h"
#include "PhysicsDebugCapture.h"

#ifdef JPH_DEBUG_RENDERER
#include <Jolt/Core/StreamWrapper.h>
#include <Jolt/Renderer/DebugRendererRecorder.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Collision/Shape/Shape.h>
#endif

#include "GameObject.h"
#include "ObjectManager.h"
#include "PhysicsManager.h"
#include "PhysicsColliderComponent.h"
#include "AnimationComponent.h"
#include "RenderComponent.h"
#include "ReadGLTFMesh.h"
#include "TransformComponent.h"

namespace
{
	constexpr const char* kDumpFileName = "client_physics_dump.bin";

#ifdef JPH_DEBUG_RENDERER
	// 스케일을 떼어낸 회전+위치 행렬 (뼈 축 표시용)
	JPH::RMat44 to_jolt_rotation_translation(const XMFLOAT4X4& m)
	{
		XMVECTOR scale, rotation, translation;
		XMMatrixDecompose(&scale, &rotation, &translation, XMLoadFloat4x4(&m));
		XMFLOAT4 q;
		XMFLOAT3 t;
		XMStoreFloat4(&q, XMQuaternionNormalize(rotation));
		XMStoreFloat3(&t, translation);
		return JPH::RMat44::sRotationTranslation(JPH::Quat(q.x, q.y, q.z, q.w), JPH::RVec3(t.x, t.y, t.z));
	}
#endif
}

PhysicsDebugCapture::PhysicsDebugCapture() = default;

PhysicsDebugCapture::~PhysicsDebugCapture()
{
	close_session();
}

void PhysicsDebugCapture::request_capture(const std::string& label)
{
#ifdef JPH_DEBUG_RENDERER
	_captureRequested = true;
	_label = label;
#endif
}

void PhysicsDebugCapture::process_end_of_frame()
{
#ifdef JPH_DEBUG_RENDERER
	if (!_captureRequested) return;
	_captureRequested = false;

	if (!open_session()) return;
	record_frame();
#endif
}

void PhysicsDebugCapture::add_reference_primitive(const std::shared_ptr<GameObject>& owner, const std::string& material_name,
                                                  const std::string& bone_name)
{
#ifdef JPH_DEBUG_RENDERER
	if (!owner) return;

	auto render = owner->get_component<RenderComponent>();
	auto gltf = render ? std::dynamic_pointer_cast<ReadGLTFMesh>(render->mesh()) : nullptr;
	if (!gltf)
	{
		CLOG("[PhysicsDebugCapture] glTF 메쉬가 없어 기준 형상을 등록하지 못함: " << owner->name());
		return;
	}

	ReferencePrimitive reference;
	reference.owner = owner;
	reference.bone_name = bone_name;
	if (!gltf->get_primitive_geometry(material_name, reference.positions, reference.indices))
	{
		CLOG("[PhysicsDebugCapture] 프리미티브를 찾지 못함: " << material_name << " (" << owner->name() << ")");
		return;
	}
	_references.push_back(std::move(reference));
#endif
}

void PhysicsDebugCapture::remove_reference_primitives(const GameObject* owner)
{
#ifdef JPH_DEBUG_RENDERER
	std::erase_if(_references, [owner](const ReferencePrimitive& reference) {
		auto locked = reference.owner.lock();
		return !locked || locked.get() == owner;
	});
#endif
}

void PhysicsDebugCapture::close_session()
{
#ifdef JPH_DEBUG_RENDERER
	// 기록기가 스트림을 참조하므로 기록기 → 스트림 → 파일 순서로 정리
	_recorder.reset();
	_stream.reset();
	if (_file.is_open()) _file.close();
#endif
}

#ifdef JPH_DEBUG_RENDERER
bool PhysicsDebugCapture::open_session()
{
	if (_recorder) return true;

	_file.open(kDumpFileName, std::ios::binary | std::ios::trunc);
	if (!_file.is_open())
	{
		CLOG("[PhysicsDebugCapture] 파일을 열 수 없음: " << kDumpFileName);
		return false;
	}

	_stream = std::make_unique<JPH::StreamOutWrapper>(_file);
	_recorder = std::make_unique<JPH::DebugRendererRecorder>(*_stream);
	_capturedFrames = 0;
	CLOG("[PhysicsDebugCapture] 기록 세션 시작: " << kDumpFileName);
	return true;
}

void PhysicsDebugCapture::record_frame()
{
	JPH::DebugRendererRecorder* renderer = _recorder.get();

	// 1. Jolt 바디 전체 (기존 Body 모드 콜라이더 비교용)
	if (auto* physics_system = PhysicsManager::instance()->get_physics_system())
	{
		JPH::BodyManager::DrawSettings settings;
		settings.mDrawShape = true;
		settings.mDrawShapeWireframe = true;
		settings.mDrawShapeColor = JPH::BodyManager::EShapeColor::ShapeTypeColor;
		physics_system->DrawBodies(settings, renderer);
	}

	// 2. QueryOnly 콜라이더 (현재: 초록, 직전 프레임: 어두운 초록) + 부착 뼈 축
	const JPH::Color prev_color(0, 100, 0);
	for (const auto& object : ObjectManager::instance()->get_all_game_objects())
	{
		if (!object || !object->is_enable() || object->is_destroyed()) continue;

		for (const auto& component : object->components())
		{
			auto collider = std::dynamic_pointer_cast<PhysicsColliderComponent>(component);
			if (!collider || collider->body_mode() != PhysicsColliderComponent::BodyMode::QueryOnly) continue;
			if (!collider->has_world_transform() || !collider->get_shape()) continue;

			const JPH::Shape* shape = collider->get_shape();
			shape->Draw(renderer, collider->prev_world_transform(), JPH::Vec3::sReplicate(1.0f), prev_color, false, true);
			shape->Draw(renderer, collider->world_transform(), JPH::Vec3::sReplicate(1.0f), JPH::Color::sGreen, false, true);

			if (collider->is_attached_to_bone())
			{
				XMFLOAT4X4 bone_world;
				auto anim = object->get_component<AnimationComponent>();
				if (anim && anim->try_get_bone_world_matrix(collider->attached_bone(), bone_world))
				{
					renderer->DrawCoordinateSystem(to_jolt_rotation_translation(bone_world), 0.3f);
				}
			}
		}
	}

	// 3. 기준 프리미티브 (실제 메쉬 와이어, 흰색). 렌더링과 같게 스케일 포함 행렬로 변환
	std::erase_if(_references, [](const ReferencePrimitive& reference) { return reference.owner.expired(); });
	for (const auto& reference : _references)
	{
		auto owner = reference.owner.lock();
		auto anim = owner->get_component<AnimationComponent>();
		XMFLOAT4X4 bone_world;
		if (!anim || !anim->try_get_bone_world_matrix(reference.bone_name, bone_world)) continue;

		const XMMATRIX m = XMLoadFloat4x4(&bone_world);
		auto to_world = [&](UINT index) {
			XMFLOAT3 p;
			XMStoreFloat3(&p, XMVector3TransformCoord(XMLoadFloat3(&reference.positions[index]), m));
			return JPH::RVec3(p.x, p.y, p.z);
		};
		for (size_t i = 0; i + 2 < reference.indices.size(); i += 3)
		{
			renderer->DrawWireTriangle(to_world(reference.indices[i]), to_world(reference.indices[i + 1]),
			                           to_world(reference.indices[i + 2]), JPH::Color::sWhite);
		}

		// 라벨 (예: "attack 30%")
		if (!_label.empty() && owner->transform())
		{
			XMFLOAT3 pos = owner->transform()->get_world_position();
			renderer->DrawText3D(JPH::RVec3(pos.x, pos.y + 2.2f, pos.z), _label, JPH::Color::sYellow, 0.2f);
		}
	}

	renderer->EndFrame();
	_file.flush(); // 게임 실행 중에도 뷰어로 열 수 있게 바로 기록

	++_capturedFrames;
	CLOG("[PhysicsDebugCapture] 프레임 기록 " << _capturedFrames << (_label.empty() ? "" : " : ") << _label);
}
#endif
