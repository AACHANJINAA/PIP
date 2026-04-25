#include "stdafx.h"
#include "TargetingComponent.h"

#include "GameObject.h"
#include "transformComponent.h"
#include "LayerManager.h"
#include "NPCScript.h"
#include "ObjectManager.h"

void TargetingComponent::update(float deltaTime)
{
    if (!_isLockedOn)
    {
        find_best_target();
    }
    else
    {
        // 락온 중일 때 타겟이 멀어지거나 죽으면 해제
        if (!is_valid_target(_currentTargetId))
        {
            _isLockedOn = false;
            _currentTargetId = -1;
        }
    }
}

bool TargetingComponent::is_valid_target(int64_t id)
{
    auto npc_obj = ObjectManager::instance()->find_npc(id);
    if (!npc_obj || !npc_obj->is_enable()) return false;

    auto script = npc_obj->get_component<NPCScript>();
    if (!script || script->hp() <= 0) return false;

    float dist = common::Length(npc_obj->transform()->local_position() - game_object()->transform()->local_position());
    if (dist > _maxDistance * 1.5f) return false; // 락온 유지 거리는 조금 더 넉넉하게

    return true;
}

void TargetingComponent::find_best_target()
{
    // 1. 메인 카메라 컴포넌트 가져오기
    auto mainCamera = CameraComponent::get_main();
    if (!mainCamera) return;

    auto camTransform = mainCamera->game_object()->transform();
    common::Vec3 camPos = camTransform->position();
    common::Vec3 camForward = camTransform->forward();
    auto frustum = mainCamera->frustum(); // CameraComponent에 정의된 프러스텀

    auto enemy_layer = LayerManager::instance()->get_layer_value("Enemy");
    auto enemies = ObjectManager::instance()->find_by_layer(enemy_layer);

    int64_t bestId = -1;
    float bestScore = -1.0f;

    for (auto& enemy : enemies)
    {
        auto npc_script = enemy->get_component<NPCScript>();
        if (!npc_script || npc_script->hp() <= 0) continue;

        common::Vec3 enemyPos = enemy->transform()->position();

        // --- 프러스텀 체크 (화면 안에 있는가?) ---
        // BoundingFrustum::Contains는 점/박스가 시야 안에 있는지 확인합니다.
        if (frustum.Contains(XMLoadFloat3(&enemyPos)) == DirectX::DISJOINT) continue;

        // --- 거리 체크 ---
        float dist = common::Length(enemyPos - camPos);
        if (dist > _maxDistance) continue;

        // --- 화면 중앙 점수 계산 ---
        common::Vec3 toEnemy = common::Normalize(enemyPos - camPos);
        float dot = common::Dot(camForward, toEnemy);

        // 내적값이 클수록(1.0에 가까울수록) 화면 중앙에 있는 것임
        if (dot > bestScore)
        {
            bestScore = dot;
            bestId = npc_script->id();
        }
    }

    _currentTargetId = bestId;
}

void TargetingComponent::toggle_lock_on()
{
    if (_isLockedOn)
    {
        _isLockedOn = false;
        _currentTargetId = -1;
    }
    else if (_currentTargetId != -1)
    {
        _isLockedOn = true;
    }
}
