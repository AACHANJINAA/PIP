#include "stdafx.h"
#include "MonsterHPComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"

MonsterHPComponent::MonsterHPComponent()
{

}

MonsterHPComponent::~MonsterHPComponent()
{

}

void MonsterHPComponent::late_update(float deltaTime)
{
    //if (_isHpChanged)
    //{
    //    if (_nowHpTimer < _chageHpTimer)
    //    {
    //        _nowHpTimer += deltaTime;
    //    }
    //    else
    //    {
    //        _nowHpTimer = 0.0f;
    //        _isHpChanged = false;
    //        _hpRatio = static_cast<float>(_currentHP) / static_cast<float>(_maxHP);
    //    }
    //}
}

XMFLOAT3 MonsterHPComponent::get_world_position()
{
    return game_object().get()->transform().get()->local_position();
}
