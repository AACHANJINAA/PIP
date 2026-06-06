#pragma once
#pragma once
#include "Component.h"

class Behavior : public Component
{
public:
	Behavior();
	Behavior(const std::string& name) : Component(name), _isEnabled{ true } {}
	virtual ~Behavior() = default;

	// --- Getter/Setter (코드 컨벤션 적용) ---
	bool is_enabled() const { return _isEnabled; }
	void set_enabled(bool isEnabled);

	// --- 생명주기 함수 ---
	// 이 함수들은 Script 클래스에서 구체적으로 오버라이드될 것입니다.

	// 씬에 오브젝트가 추가되고 나서 가장 처음 한 번 호출됩니다.
	virtual void awake() {}
	// 시작 시점에 한 번 호출됩니다.
	virtual void start() {}
	// 오브젝트가 활성화될 때 호출됩니다.
	virtual void on_enable() {}
	// 매 프레임 호출됩니다.
	virtual void update(float deltaTime) {}
	// 매 프레임 업데이트 이후에 호출됩니다.
	virtual void late_update(float deltaTime) {}
	// 고정된 시간 간격으로 호출됩니다. (예: 물리 업데이트)
	virtual void fixed_update(float deltaTime) {}
	// 오브젝트가 비활성화될 때 호출됩니다.
	virtual void on_disable() {}
	// 오브젝트가 파괴되기 직전에 호출됩니다.
	virtual void on_destroy() {}

protected:
	bool _isEnabled;
};