#pragma once
#include "Component.h"
#include "JoltSetup.h"
#include <Jolt/Physics/Character/CharacterVirtual.h>
namespace PIP::GAME
{
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
	private:
		float _halfHeight {};

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
