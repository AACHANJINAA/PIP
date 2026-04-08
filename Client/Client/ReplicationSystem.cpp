#include "stdafx.h"
#include "ReplicationSystem.h"

#include "NPCScript.h"

void ReplicationSystem::register_entity(int64_t id, INetSync* entity)
{
	if (_entities.contains(id))
	{
		CLOG("Entity with ID " << id << " is already registered. Overwriting.");
	}
	_entities[id] = entity;
}
void ReplicationSystem::unregister_entity(int64_t id)
{
	if (!_entities.contains(id))
	{
		CLOG("Attempted to unregister non-existent entity with ID " << id);
	}
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
    // Iterator invalidation 방지: ID 목록을 미리 복사
    std::vector<int64_t> entity_ids;
    entity_ids.reserve(_entities.size());

    for (const auto& [id, entity] : _entities)
    {
        entity_ids.push_back(id);
    }

    // [Systemic Processing] 복사된 ID 리스트로 안전하게 순회
    for (int64_t id : entity_ids)
    {
        auto it = _entities.find(id);
        if (it != _entities.end())  // 중간에 삭제되었을 수도 있음
        {
            it->second->apply_snapshot();
        }
    }
}
