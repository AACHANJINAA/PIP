#pragma once
#include "Component.h"
#include "JoltSetup.h"
#include <Jolt/Physics/Character/CharacterVirtual.h>

#include "Vector3.h"

namespace PIP::GAME
{
	constexpr float ImpactFriction = 35.0f; // 넉백 감쇄 속도
	class CharacterControllerComponent : public Component{
	public:
		CharacterControllerComponent(GameObject* owner) : Component(owner) {}
		~CharacterControllerComponent() override;

		// 캐릭터 컨트롤러 초기화 (캡슐 모양 설정 및 Jolt 등록)
		void Initialize() override {}
		void Initialize(JPH::PhysicsSystem* physicsSystem, float height = 1.8f, float radius = 0.3f);

		// 기본 PhysicsUpdate는 인터페이스 유지를 위해 오버라이드 (내용은 비움)
		void PhysicsUpdate(float deltaTime) override {}

		// [중요] 스레드 공유 Allocator를 사용하여 실제 물리 업데이트를 수행하는 전용 함수
		void PhysicsUpdate(float deltaTime, JPH::TempAllocator* allocator);

		// 이동 및 상태 제어 함수
		void SetVelocity(const common::Vec3& velocity);
		common::Vec3 GetVelocity() const;

		void SetPosition(const common::Vec3& position);
		common::Vec3 GetPosition() const;

		void AddImpulse(const common::Vec3& impulse);

		bool IsGrounded() const;

		// [추가] Jolt의 Shape 정보를 가져오는 함수
		const JPH::Shape* GetShape() const {
			if (_character) return _character->GetShape();
			return nullptr;
		}

		// [추가] CharacterVirtual 객체 자체를 가져오는 함수 (AOI 업데이트 제어용)
		JPH::CharacterVirtual* GetCharacter() { return _character.GetPtr(); }

		// AI가 결정한 이동 속도 (매 프레임 덮어씌워짐)
		void SetAIMovement(const common::Vec3& vel) { _aiVelocity = vel; }

		// 외부에서 가해진 힘 (넉백 등, 서서히 감쇄됨)
		void AddImpact(const common::Vec3& force)
		{
			using namespace common::VectorHelper;
			_impactVelocity += force;
		}

	private:
		float _halfHeight {};

		common::Vec3 _aiVelocity = { 0,0,0 };
		common::Vec3 _impactVelocity = { 0,0,0 };
		

		JPH::PhysicsSystem* _physicsSystem = nullptr;
		JPH::Ref<JPH::CharacterVirtual> _character;
		JPH::Ref<JPH::CharacterVirtualSettings> _settings;

		// 캐릭터 접촉 리스너 (충돌 이벤트 처리용)
		class CharacterContactListener : public JPH::CharacterContactListener
		{
		public:
			virtual void OnContactAdded(const JPH::CharacterVirtual* inCharacter, 
				const JPH::BodyID& inBodyID2, 
				const JPH::SubShapeID& inSubShapeID2,
				JPH::RVec3Arg inContactPosition, 
				JPH::Vec3Arg inContactNormal,
				JPH::CharacterContactSettings& ioSettings)
			override {}
		}	_contactListener;
	};
}
