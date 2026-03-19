#include "stdafx.h"
#include "LongswordScript.h"

//void LongswordScript::awake()
//{
//    WeaponScript::awake(); // 부모의 awake를 호출하여 _collider를 가져옵니다.
//
//    WeaponInfo longsword;
//    longsword.name = "Longsword";
//    longsword.damage = 10.0f;
//    longsword.halfHeight = 0.8f;  // 검날 길이 약 1.6m (절반인 0.8m)
//    longsword.radius = 0.15f;     // 검날 두께
//    longsword.skillCooldown = 2.0f;
//    longsword.skillDamage = 20.0f;
//    longsword.skillChargeTime = 2.0f;
//
//    set_weapon_info(longsword);
//
//    // 캡슐 콜라이더 초기화
//    // Jolt Capsule은 (half_height, radius)를 인자로 받습니다.
//    if (_collider) {
//        _collider->initialize(
//            PhysicsColliderComponent::ShapeType::Capsule,
//            { _info.radius, _info.halfHeight, 0.0f }, // size.x = radius, size.y = halfHeight
//            { 0.0f, _info.halfHeight, 0.0f },         // 오프셋: 손잡이(0,0,0)에서 검날 방향(Y)으로 절반만큼 이동
//            { 0.0f, 0.0f, 0.0f },
//            true // 센서(Trigger) 모드
//        );
//        _collider->set_active(false); // 초기 상태는 비활성화
//    }
//}
