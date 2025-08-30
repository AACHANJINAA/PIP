#include "pch.h"
#include "MapDataManager.h"
namespace chess
{
	void MapDataManager::LoadMapData(std::string_view mapDataPath)
	{
		std::ifstream file(mapDataPath.data());
		if (not file)
		{
			MYERROR("Failed to open map data file: " << mapDataPath);
			return;
		}
		using namespace nlohmann;
		json data = json::parse(file);

		for (const auto& item : data)
		{
			if (!item.contains("AABB")) continue;

			Vec3 min_v;
			min_v.x = item["AABB"]["Min"]["X"];
			min_v.y = item["AABB"]["Min"]["Y"];
			min_v.z = item["AABB"]["Min"]["Z"];

			Vec3 max_v;
			max_v.x = item["AABB"]["Max"]["X"];
			max_v.y = item["AABB"]["Max"]["Y"];
			max_v.z = item["AABB"]["Max"]["Z"];

			_map_objects.push_back({ min_v, max_v });
		}
	}
	bool MapDataManager::CheckForCollision(Vec3 target_pos, Vec3 player_extents)
	{
		// 1. 플레이어의 AABB(경계 상자) 계산
		Vec3 player_min = { target_pos.x - player_extents.x, target_pos.y - player_extents.y, target_pos.z - player_extents.z };
		Vec3 player_max = { target_pos.x + player_extents.x, target_pos.y + player_extents.y, target_pos.z + player_extents.z };

		// 2. 모든 맵 오브젝트와 충돌 검사
		for (const auto& map_object : _map_objects)
		{
			// 3. AABB 충돌 검사 로직
			if (player_max.x > map_object._min.x &&
				player_min.x < map_object._max.x &&
				player_max.y > map_object._min.y &&
				player_min.y < map_object._max.y &&
				player_max.z > map_object._min.z &&
				player_min.z < map_object._max.z)
			{
				return true; // 충돌 발생
			}
		}

		return false; // 충돌 없음
	}
}
