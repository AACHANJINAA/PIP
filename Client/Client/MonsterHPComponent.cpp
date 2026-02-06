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
	// 이 부분의 주석을 풀면 HP 변화 후 일정 시간 동안만 보여주는 로직이 활성화 됨
    // 보여주기 위해서 주석 걸어둠
    if (_isHpChanged)
    {
        if (_nowHpTimer < _chageHpTimer)
        {
            _nowHpTimer += deltaTime;
        }
        else
        {
            _nowHpTimer = 0.0f;
            _isHpChanged = false;
            _hpRatio = static_cast<float>(_currentHP) / static_cast<float>(_maxHP);
        }
    }
}
