#pragma once
#include "GameObject.h"
#include "PhysicsColliderComponent.h"
#include "ScriptComponent.h"

struct WeaponInfo {
    std::string name = "None";
    float damage = 10.0f;
    float range = 2.0f;
    float skillCooldown = 2.0f;
    float skillDamage = 20.0f;
    float skillChargeTime = 2.0f;
    float halfHeight = 0.75f;
	float radius = 0.15f;
};


class WeaponScript : public ScriptComponent {
public:
	using required_components = std::tuple<PhysicsColliderComponent>; // 무기는 별도의 컴포넌트 의존성이 없습니다.
    WeaponScript() = default;
    virtual ~WeaponScript() override = default;

    // 기본 로직 (상속 가능)
    virtual void awake() override;
    virtual void update(float deltaTime) override;

    // 데이터 제어 인터페이스
    void set_weapon_info(const WeaponInfo& info) { _info = info; }
    const WeaponInfo& info() const { return _info; }

    // 공격 판정 활성화/비활성화 (애니메이션 프레임에 맞춰 호출)
    void set_attack_active(bool active) const;

    // 스킬 관련 상태 제어
    bool can_use_skill() const { return _skillCooldownTimer <= 0.0f; }
    void start_charge() { _isCharging = true; _skillChargeTimer = 0.0f; }
    void stop_charge() { _isCharging = false; _skillChargeTimer = 0.0f; }

    // 스킬 사용 시 쿨타임 적용
    void use_skill() {
        _skillCooldownTimer = _info.skillCooldown;
        _isCharging = false;
    }

    // 진행도 및 상태 확인
    float charge_progress() const { return std::min(1.0f, _skillChargeTimer / _info.skillChargeTime); }
    bool is_charge_complete() const { return _skillChargeTimer >= _info.skillChargeTime; }
    float cooldown_remaining() const { return std::max(0.0f, _skillCooldownTimer); }

    // 타격 이벤트 (서버 전송 전 클라이언트 로그용)
    void on_collision_enter(std::shared_ptr<GameObject> other) override;

protected:
    WeaponInfo _info;
    std::shared_ptr<PhysicsColliderComponent> _collider = nullptr;
    float _skillCooldownTimer = 0.0f;
    float _skillChargeTimer = 0.0f;
    bool _isCharging = false;
};

// -------------------------------------------------------------------
// 롱소드 구체화 클래스
// -------------------------------------------------------------------
class LongswordScript : public WeaponScript {
public:
    void awake() override {
        WeaponScript::awake(); // 부모의 awake를 호출하여 _collider를 가져옵니다.

        WeaponInfo longsword;
        longsword.name = "Longsword";
        longsword.damage = 10.0f;
        longsword.halfHeight = 0.8f;  // 검날 길이 약 1.6m (절반인 0.8m)
        longsword.radius = 0.15f;     // 검날 두께
        longsword.skillCooldown = 2.0f;
        longsword.skillDamage = 20.0f;
        longsword.skillChargeTime = 2.0f;

        set_weapon_info(longsword);

        // 대검 스킬 판정 캡슐 (HitFeel_Plan_KR.md 3단계)
        // 무기 오브젝트는 SM_Weapon_Sword__10을 10배로 키운 것(화면에는 파티클로 보이는 대검). 메쉬는 노드 이동 포함 로컬 Y -0.149 ~ 0.848
        // 중심은 오브젝트 스케일(10배)이 곱해지는 로컬 단위, 캡슐 크기는 스케일이 적용되지 않으므로 미터 단위
        // 판정은 MainPlayerScript가 스킬 휘두르는 동안 begin/end_hit_query로 켜고, 결과는 set_hit_receiver로 플레이어가 받음
        if (_collider) {
            constexpr float kSkillBladeRadius = 0.5f;       // 칼날 폭 약 1.1m의 절반 정도
            constexpr float kSkillBladeHalfLength = 5.0f;   // 길이 약 10m
            _collider->initialize(
                PhysicsColliderComponent::ShapeType::Capsule,
                { kSkillBladeRadius, kSkillBladeHalfLength - kSkillBladeRadius, 0.0f }, // x = 반지름, y = 원기둥 반높이
                { 0.0f, 0.35f, 0.0f },  // 칼 메쉬 중점 (로컬, 10배 하면 손에서 3.5m)
                { 0.0f, 0.0f, 0.0f },
                true,
                PhysicsColliderComponent::BodyMode::QueryOnly);
            _collider->set_role(PhysicsColliderComponent::Role::Hitbox);
        }
    }
};