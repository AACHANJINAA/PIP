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
		common::TerrainData new_terrain_data;
		if (!new_terrain_data.LoadFromJSON(heightMapDataJSONPath.data(),false))
		{
			MYERROR("Failed to load height map via Common::TerrainData: " << heightMapDataJSONPath);
		}
		else
		{
			const auto& info = new_terrain_data.GetInfo();
			const auto& heightMap = new_terrain_data.GetHeightData();

			MYLOG("[TerrainData] Info: X[" << info.min_x << " ~ " << info.max_x
				<< "], Z[" << info.min_z << " ~ " << info.max_z << "]" << std::endl);

			MYLOG("Height map Loaded via Common: " << info.width << " * " << info.height
				<< ", Scale Y: " << info.height_scale);


			// --- [핵심] 여기서 딱 한 번 Shape을 생성함 ---
			JPH::HeightFieldShapeSettings settings;
			settings.mOffset = JPH::Vec3(info.min_x, 0.0f, info.min_z);

			float dx = (info.max_x - info.min_x) / (info.width - 1);
			float dz = (info.max_z - info.min_z) / (info.height - 1);
			settings.mScale = JPH::Vec3(dx, 1.0f, dz);
			settings.mSampleCount = static_cast<JPH::uint32>(info.width);

			settings.mHeightSamples.resize(heightMap.size());
			memcpy(settings.mHeightSamples.data(), heightMap.data(), heightMap.size() * sizeof(float));

			auto result = settings.Create();

			TerrainTile tile;
			tile.data = std::move(new_terrain_data);
			if (result.IsValid()) {
				tile.shape = result.Get(); // 생성된 Shape 저장 (Ref Count 증가)
			}

			// 전역 경계 갱신
			_worldMinX = (std::min)(_worldMinX, info.min_x);
			_worldMaxX = (std::max)(_worldMaxX, info.max_x);
			_worldMinZ = (std::min)(_worldMinZ, info.min_z);
			_worldMaxZ = (std::max)(_worldMaxZ, info.max_z);

			_terrainTiles.push_back(std::move(tile));
		}
	}

	void MapDataManager::LoadMainLandscapeData(std::string_view landscapeDirPath)
	{
		std::filesystem::path landscapeBaseDir(landscapeDirPath);
		if (!std::filesystem::exists(landscapeBaseDir)) {
			MYERROR("MainLandscape directory not found: " << landscapeDirPath);
			return;
		}

		_terrainTiles.clear();

		for (const auto& entry : std::filesystem::directory_iterator(landscapeBaseDir))
		{
			if (!entry.is_directory()) continue;
			if (entry.path().filename().string().find("Landscape") != 0) continue;

			std::filesystem::path metadataPath = entry.path() / "metadata.json";
			if (!std::filesystem::exists(metadataPath)) continue;

			common::TerrainData data;
			// 다중 지형(MainLandscape)은 절대 높이를 유지해야 하므로 false 전달
			if (data.LoadFromJSON(metadataPath.string(), false)) {
				const auto& info = data.GetInfo();
				const auto& heightMap = data.GetHeightData();

				// --- [핵심] 여기서 딱 한 번 Shape을 생성함 ---
				JPH::HeightFieldShapeSettings settings;
				settings.mOffset = JPH::Vec3(info.min_x, 0.0f, info.min_z);

				float dx = (info.max_x - info.min_x) / (info.width - 1);
				float dz = (info.max_z - info.min_z) / (info.height - 1);
				settings.mScale = JPH::Vec3(dx, 1.0f, dz);
				settings.mSampleCount = static_cast<JPH::uint32>(info.width);

				settings.mHeightSamples.resize(heightMap.size());
				memcpy(settings.mHeightSamples.data(), heightMap.data(), heightMap.size() * sizeof(float));

				auto result = settings.Create();

				TerrainTile tile;
				tile.data = std::move(data);
				if (result.IsValid()) {
					tile.shape = result.Get(); // 생성된 Shape 저장 (Ref Count 증가)
				}

				// 전역 경계 갱신
				_worldMinX = (std::min)(_worldMinX, info.min_x);
				_worldMaxX = (std::max)(_worldMaxX, info.max_x);
				_worldMinZ = (std::min)(_worldMinZ, info.min_z);
				_worldMaxZ = (std::max)(_worldMaxZ, info.max_z);

				_terrainTiles.push_back(std::move(tile));
			}
		}
		MYLOG("Total Landscapes & Shapes Loaded: " << _terrainTiles.size());
	}



	common::Vec3 MapDataManager::AdjustPositionToGround(common::Vec3 position)
	{
		position.y = GetGroundHeight(position.x, position.z);
		return position;
	}

	bool MapDataManager::IsInsideMap(float x, float z) const
	{
		return x >= _worldMinX && x <= _worldMaxX && z >= _worldMinZ && z <= _worldMaxZ;
	}

	float MapDataManager::GetGroundHeight(float x, float z) const
	{
		// 모든 지형 타일을 순회하며 해당 좌표가 포함된 타일의 높이를 반환
		for (const auto& tile : _terrainTiles) {
			if (tile.data.IsInsideMap(x, z)) {
				return tile.data.GetHeightAt(x, z);
			}
		}
		return 0.0f;
	}

}