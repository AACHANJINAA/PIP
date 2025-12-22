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
		
		// [수정] JSON 로드만 지원하도록 변경 (Common 사용)
		void LoadHeightMapData(std::string_view heightMapDataJSONPath);

		
		// [삭제] 구버전 로드 함수들 제거 (혼동 방지)
		// void LoadHeightMapData(std::string_view r16FilePath, ...);
		// void LoadHeightMapDataPNG(...);
		// void LoadHeightMapFromRawFile(...);

		float GetGroundHeight(float x, float z);
		common::Vec3 AdjustPositionToGround(common::Vec3 position);

		bool CheckForCollision(common::Vec3 target_pos, common::Vec3 player_extents);
		bool IsInsideMap(float x, float z) const
		{
			const auto& info = m_terrainData.GetInfo();
			return (x >= info.min_x && x <= info.max_x && z >= info.min_z && z <= info.max_z);
		}
	private:
		// [수정] 내부 구현이었던 InterpolateHeight는 TerrainData로 이동되었으므로 제거 가능
		// 하지만 필요하다면 wrapper로 남겨둘 수 있음. (여기선 제거)

	private:
		std::vector<MapObject> _map_objects;
		
		// [수정] Common::TerrainData 사용
		common::TerrainData m_terrainData;

		// [삭제] _height_map_data 구조체 및 멤버 제거
	};
}

