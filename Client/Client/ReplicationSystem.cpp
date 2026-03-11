#include "stdafx.h"
#include "ReplicationSystem.h"
void ReplicationSystem::register_entity(int64_t id, INetSync* entity)
{
	_entities[id] = entity;
}
void ReplicationSystem::unregister_entity(int64_t id)
{
	_entities.erase(id);
}
bool ReplicationSystem::on_packet_arrival(int64_t id, const NetSnapshot& snapshot)
{
	auto it = _entities.find(id);
	if (it != _entities.end()) {
		// 인터페이스를 통해 데이터만 전달 (구체적인 클래스는 모름)
		it->second->on_receive_snapshot(snapshot);
		return true;
	}
	return false;
}
void ReplicationSystem::update(float dt)
{
	// [Systemic Processing] 모든 엔티티의 로직을 일괄 실행
	// 이 루프는 나중에 병렬화(Parallel For)하기 매우 좋은 지점입니다.
	for (auto& [id, entity] : _entities) {
		entity->apply_snapshot();
	}
}
