#include "stdafx.h"
#include "WeaponScript.h"
#include "GameObject.h"
#include "DebugDrawManager.h"
#include "JoltHelper.h"
#include "PhysicsManager.h"
#include "TransformComponent.h"

void WeaponScript::awake()
{
    _collider = game_object()->get_component<PhysicsColliderComponent>();
}

void WeaponScript::set_attack_active(bool active) const
{
    if (_collider) _collider->set_active(active);
}

void WeaponScript::update(float deltaTime)
{
    if (_skillCooldownTimer > 0.0f) _skillCooldownTimer -= deltaTime;
    if (_isCharging) _skillChargeTimer += deltaTime;

    // Debug Draw: 활성화 상태일 때 매 프레임 히트박스 그리기
    if (_collider && _collider->is_active())
    {
        auto trans = game_object()->transform();
        XMFLOAT3 pos = trans->get_world_position();
        
        // 월드 행렬에서 회전 쿼터니언 추출
        XMMATRIX worldMat = XMLoadFloat4x4(&trans->world_matrix());
        XMVECTOR scale, rotQuat, translation;
        XMMatrixDecompose(&scale, &rotQuat, &translation, worldMat);
        
        XMFLOAT4 rot;
        XMStoreFloat4(&rot, rotQuat);
        
        // 캡슐 정보 (radius, halfHeight)
        DebugDrawManager::instance()->AddDebugShape(
            common::packet::DebugShapeType::CAPSULE,
            pos,
            rot,
            { _info.radius, _info.halfHeight, 0.0f },
            10.0f 
        );
		XMFLOAT4X4 debugWorld;
		XMStoreFloat4x4(&debugWorld, worldMat);
        //// [추가] 수동 충돌 쿼리 (NarrowPhaseQuery)
        //auto* physicsSystem = PhysicsManager::instance()->get_physics_system();
        //if (physicsSystem) {
        //    JPH::RShapeCast shapeCast(
        //        _collider->get_shape(),
        //        JPH::Vec3::sOne(),
        //        PIP::Utils::ToJolt(debugWorld),
        //        JPH::Vec3::sZero()
        //    );

        //    JPH::ShapeCastSettings settings;
        //    JPH::CastShapeCollector<JPH::CastShapeCollector> collector;

        //    physicsSystem->GetNarrowPhaseQuery().CastShape(
        //        shapeCast,
        //        settings,
        //        JPH::RVec3::sZero(),
        //        collector,
        //        physicsSystem->GetDefaultBroadPhaseLayerFilter(PIP::Layers::MOVING),
        //        physicsSystem->GetDefaultLayerFilter(PIP::Layers::MOVING)
        //    );

        //    if (collector.()) {
        //        for (const auto& hit : collector.mHits) {
        //            JPH::BodyLockRead lock(physicsSystem->GetBodyLockInterface(), hit.mBodyID2);
        //            if (lock.Succeeded()) {
        //                const JPH::Body& hitBody = lock.GetBody();
        //                uint64_t userData = hitBody.GetUserData();
        //                if (userData != 0) {
        //                    GameObject* hitObj = reinterpret_cast<GameObject*>(userData);
        //                    // 자기 자신 제외
        //                    if (hitObj != game_object()->parent()->game_object().get()) {
        //                        on_collision_enter(hitObj->shared_from_this());
        //                    }
        //                }
        //            }
        //        }
        //    }
        //}
    }
}

void WeaponScript::on_collision_enter(std::shared_ptr<GameObject> other)
{
    CLOG("[Weapon Hit] Target: " << other->name() << " with " << _info.name << std::endl);
}
