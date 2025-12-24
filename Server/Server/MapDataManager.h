#pragma once

#include <string>
#include <vector>
#include <filesystem>

#include "../../Common/TerrainData.h"
#include "../../Common/Vector3.h"

// [수정] 기존 MapObject 구조체는 유지 (AABB 충돌용)
// 필요하다면 Common으로 이동 가능

namespace PIP
{
	struct MapObject
	{
		common::Vec3 _min;
		common::Vec3 _max;
	};

	class MapDataManager : public Singleton<MapDataManager>
	{
		friend class Singleton<MapDataManager>;

	public:
		MapDataManager() = default;
		~MapDataManager() = default;

		void LoadMapData(std::string_view mapDataPath);
		void LoadHeightMapData(std::string_view heightMapDataJSONPath);

		const common::TerrainData& GetTerrainData() const { return m_terrainData; }


		float GetGroundHeight(float x, float z);
		common::Vec3 AdjustPositionToGround(common::Vec3 position);

		bool CheckForCollision(common::Vec3 target_pos, common::Vec3 player_extents);
		bool IsInsideMap(float x, float z) const
		{
			const auto& info = m_terrainData.GetInfo();
			return (x >= info.min_x && x <= info.max_x && z >= info.min_z && z <= info.max_z);
		}

	private:
		std::vector<MapObject> _map_objects;
		common::TerrainData m_terrainData;
	};
}

