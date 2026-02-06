#pragma once
#include "stdafx.h"
#include "Behavior.h"


class MonsterHPComponent : public Behavior
{
public:
	MonsterHPComponent();
	virtual ~MonsterHPComponent();
	void late_update(float deltaTime) override;

	void set_max_hp(int max_hp) { _maxHP = max_hp; _currentHP = max_hp; }
	int get_max_hp() const { return _maxHP; }

	bool get_is_changed_hp() const { return _isHpChanged; }

	bool is_dead() const { return _isDead; }

	int get_hp() const{ return _currentHP; }
	float get_hp_ratio() const { return _hpRatio; }

	void set_current_hp(int current_hp) 
	{ 
		if (current_hp == _currentHP) return;
		_currentHP = std::clamp(current_hp, 0, _maxHP);
		_isHpChanged = true;
		if (_currentHP <= 0)
		{
			_isDead = true;
		}
		_hpRatio = static_cast<float>(_currentHP) / static_cast<float>(_maxHP);
	}
	int get_current_hp() const { return _currentHP; }
private:

	int _maxHP{ 100 };
	int _currentHP{ 00 };
	bool _isDead{ false };
	
	bool _isHpChanged{ false }; // false로 초기화인 상태여야 함 true면 수정할것
	float _chageHpTimer{ 3.0f }; // HP 변화 후 잠시 대기 타이머
	float _nowHpTimer{ 0.0f }; // 현재 타이머

	float _hpRatio{ 1.f }; // 0.0 ~ 1.0 현재 HP 비율
};

