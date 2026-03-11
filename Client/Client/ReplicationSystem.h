#pragma once
#include "INetSync.h"

class ReplicationSystem {
public:
    ReplicationSystem() = default;
    ~ReplicationSystem() = default;

    // 객체 등록/해제 (객체 생성/소멸 시 호출)
    void register_entity(int64_t id, INetSync* entity);
    void unregister_entity(int64_t id);

    // NetworkManager에서 호출: 시스템이 관리하는 엔티티를 찾아 데이터 전달
    bool on_packet_arrival(int64_t id, const NetSnapshot& snapshot);

    // GameFramework의 루프에서 일괄 처리
    void update(float dt);

private:
    std::unordered_map<int64_t, INetSync*> _entities;
};
