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

		_map_objects.clear(); // 기존 데이터 초기화

		for (const auto& item : data)
		{
			// 핵심 필드인 Center가 있는지 확인
			if (!item.contains("Center")) continue;

			MapObject obj;

			// 1. Center 파싱 (x, y, z 소문자)
			obj._center.x = item["Center"]["x"];
			obj._center.y = item["Center"]["y"];
			obj._center.z = item["Center"]["z"];

			// 2. Rotation 파싱 (x, y, z, w 소문자)
			obj._rotation.x = item["Rotation"]["x"];
			obj._rotation.y = item["Rotation"]["y"];
			obj._rotation.z = item["Rotation"]["z"];
			obj._rotation.w = item["Rotation"]["w"];

			// 3. Extent 파싱 (x, y, z 소문자)
			obj._extent.x = item["Extent"]["x"];
			obj._extent.y = item["Extent"]["y"];
			obj._extent.z = item["Extent"]["z"];

			_map_objects.push_back(obj);
		}
		MYLOG("Map Data Loaded: " << _map_objects.size() << " objects (OBB)");
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

}