#pragma once
// [ECS Ready] 동기화에 필요한 순수 데이터만 담은 POD 구조체
// 나중에 이 구조체 배열만 따로 모으면 ECS의 Component Data가 됩니다.
struct NetSnapshot {
    common::Vec3 pos{};
    common::Vec3 vel{};
    common::Quat rot{};
    common::packet::EntityState state {common::packet::EntityState::IDLE};
    uint32_t timestamp {};
    int32_t action_id{};
};

// [Interface] 종속성 역전(DIP)을 위한 추상화 레이어
class INetSync {
public:
    virtual ~INetSync() = default;

    // 핸들러가 호출: 데이터만 빠르게 복사 (Write)
    virtual void on_receive_snapshot(const NetSnapshot& snapshot) = 0;

    // 시스템이 호출: 메인 스레드에서 로직 적용 (Read & Execute)
    virtual void apply_snapshot() = 0;
};
