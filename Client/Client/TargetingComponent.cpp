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
    auto enemy_layer = LayerManager::instance()->get_layer_value("Enemy");
    auto enemies = ObjectManager::instance()->find_by_layer(enemy_layer);

    int64_t bestId = -1;
    float bestScore = -1.0f; // 1.0에 가까울수록 정면

    auto transform = game_object()->transform();

    common::Vec3 playerPos = transform->local_position();
    common::Vec3 playerForward = transform->forward();

    for (auto& enemy : enemies)
    {
        auto npc_script = enemy->get_component<NPCScript>();
        if (!npc_script || npc_script->hp() <= 0) continue;

        common::Vec3 enemyPos = enemy->transform()->local_position();
        common::Vec3 toEnemy = enemyPos - playerPos;
        float dist = common::Length(toEnemy);

        if (dist > _maxDistance) continue;

        toEnemy = common::Normalize(toEnemy);
        float dot = common::Dot(playerForward, toEnemy);

        // 시야각 체크 (dot은 cos값)
        if (dot > cosf(XMConvertToRadians(_targetingFov * 0.5f)))
        {
            // 정면에 더 가까운 적을 우선순위로 함
            if (dot > bestScore)
            {
                bestScore = dot;
                bestId = npc_script->id();
            }
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
