#include "pch.h"
#include "MapDataManager.h"

// stb_image는 이제 필요 없을 수 있음 (Common에서 처리하거나 안 쓴다면)
// #define STB_IMAGE_IMPLEMENTATION
// #include "stb_image.h"

namespace PIP
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

			common::Vec3 min_v;
			min_v.x = item["AABB"]["Min"]["X"];
			min_v.y = item["AABB"]["Min"]["Y"];
			min_v.z = item["AABB"]["Min"]["Z"];

			common::Vec3 max_v;
			max_v.x = item["AABB"]["Max"]["X"];
			max_v.y = item["AABB"]["Max"]["Y"];
			max_v.z = item["AABB"]["Max"]["Z"];

			_map_objects.push_back({ min_v, max_v });
		}
	}

	void MapDataManager::LoadHeightMapData(std::string_view heightMapDataJSONPath)
	{
		// Common::TerrainData 로드
		if (!_terrainData.LoadFromJSON(heightMapDataJSONPath.data()))
		{
			MYERROR("Failed to load height map via Common::TerrainData: " << heightMapDataJSONPath);
		}
		else
		{
			const auto& info = _terrainData.GetInfo();	
			MYLOG("[TerrainData] Info: X[" << info.min_x << " ~ " << info.max_x
				<< "], Z[" << info.min_z << " ~ " << info.max_z << "]" << std::endl);

			MYLOG("Height map Loaded via Common: " << info.width << " * " << info.height
				<< ", Scale Y: " << info.height_scale);
		}
	}

	// 구버전 로드 함수들 제거 (구현부도 제거)

	common::Vec3 MapDataManager::AdjustPositionToGround(common::Vec3 position)
	{
		position.y = GetGroundHeight(position.x, position.z);
		return position;
	}

	float MapDataManager::GetGroundHeight(float x, float z)
	{
		// Common::TerrainData 위임
		return _terrainData.GetHeightAt(x, z);
	}

	bool MapDataManager::CheckForCollision(common::Vec3 target_pos, common::Vec3 player_extents)
	{
		// 1. 플레이어의 AABB(경계 상자) 계산
		common::Vec3 player_min = { target_pos.x - player_extents.x, target_pos.y - player_extents.y, target_pos.z - player_extents.z };
		common::Vec3 player_max = { target_pos.x + player_extents.x, target_pos.y + player_extents.y, target_pos.z + player_extents.z };

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