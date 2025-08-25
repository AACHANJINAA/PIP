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
			// Transform 정보 파싱
			std::string locStr = item["Transform"]["Location"];
			std::string scaleStr = item["Transform"]["Scale"];

			Vec3 location, scale;
			sscanf_s(locStr.c_str(), "X=%f Y=%f Z=%f", &location.x, &location.y, &location.z);
			sscanf_s(scaleStr.c_str(), "X=%f Y=%f Z=%f", &scale.x, &scale.y, &scale.z);

			// CollisionVertices가 없으면 건너뛰기
			if (!item.contains("CollisionVertices")) continue;

			Vec3 min_v = { FLT_MAX, FLT_MAX, FLT_MAX };
			Vec3 max_v = { -FLT_MAX, -FLT_MAX, -FLT_MAX };

			for (const auto& vertStr : item["CollisionVertices"])
			{
				std::string vStr = vertStr;
				Vec3 local_v;
				sscanf_s(vStr.c_str(), "X=%f Y=%f Z=%f", &local_v.x, &local_v.y, &local_v.z);

				// 로컬 정점 좌표에 스케일과 위치 적용 (회전은 일단 제외)
				Vec3 world_v;
				world_v.x = local_v.x * scale.x + location.x;
				world_v.y = local_v.y * scale.y + location.y;
				world_v.z = local_v.z * scale.z + location.z;

				// AABB 계산을 위해 최소/최대값 갱신
				// std::minmax를 사용하면 한 번에 최소/최대값을 구할 수 있습니다.
				// 아래처럼 각 축별로 std::minmax를 적용하면 됩니다.

				auto [min_x, max_x] = std::minmax(min_v.x, world_v.x);
				auto [min_y, max_y] = std::minmax(min_v.y, world_v.y);
				auto [min_z, max_z] = std::minmax(min_v.z, world_v.z);

				min_v.x = min_x;
				min_v.y = min_y;
				min_v.z = min_z;

				max_v.x = max_x;
				max_v.y = max_y;
				max_v.z = max_z;
			}

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
