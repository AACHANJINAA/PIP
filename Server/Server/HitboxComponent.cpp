#include "pch.h"
#include "HitboxComponent.h"
#include "Room.h"
#include "JoltHelper.h"
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/CollisionDispatch.h>
namespace PIP::GAME
{
	bool HitboxComponent::CheckCollision(   const JPH::PhysicsSystem* physics,
											const JPH::Shape* attackShape,
											const JPH::RMat44& attackTransform, 
											const common::ObjectSnapshot& pastData, 
											std::string& outHitPart) const
	{
        // 1. 과거 시점의 NPC 월드 행렬 구성
        JPH::RMat44 npcWorld = JPH::RMat44::sRotationTranslation(
            Utils::ToJolt(pastData._rotation), Utils::ToJolt(pastData._position));

        for (const auto& hb : _hitboxes) {
            // 2. 히트박스 로컬 -> 월드 변환
            JPH::RMat44 hbLocal = JPH::RMat44::sRotationTranslation(
                Utils::ToJolt(hb.localRotation), Utils::ToJolt(hb.localOffset));
            JPH::RMat44 hbWorld = npcWorld * hbLocal;

            JPH::CollideShapeSettings settings;
            JPH::AllHitCollisionCollector<JPH::CollideShapeCollector> collector;

            // [수정] Jolt의 공개 정적 함수를 사용하여 1:1 충돌 검사
            JPH::CollisionDispatch::sCollideShapeVsShape(
                attackShape, hb.shape,
                JPH::Vec3::sReplicate(1.0f), JPH::Vec3::sReplicate(1.0f),
                attackTransform, hbWorld,
                JPH::SubShapeIDCreator(), JPH::SubShapeIDCreator(),
                settings, collector);

            if (collector.HadHit()) {
                outHitPart = hb.name;
                return true;
            }
        }
        return false;
	}
}
