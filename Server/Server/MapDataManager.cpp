#include "pch.h"
#include "MapDataManager.h"
#include "glTFMeshLoader.h"
#include "JoltHelper.h"
#include "JoltSetup.h"
#include "PhysicsManager.h"

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

			std::string folderName = entry.path().filename().string();
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
				tile.name = folderName;
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

	void MapDataManager::LoadStaticMeshShapes(const std::string& tileName, std::string_view gltfPath)
	{
		auto meshes = glTFMeshLoader::LoadStaticMesh(gltfPath.data());

		JPH::TriangleList allTriangles;
		for (const auto& meshData : meshes) {
			
			for (size_t i = 0; i < meshData.indices.size(); i += 3) {
				allTriangles.push_back(JPH::Triangle(
					PIP::Utils::ToJolt(meshData.vertices[meshData.indices[i]]),
					PIP::Utils::ToJolt(meshData.vertices[meshData.indices[i + 1]]),
					PIP::Utils::ToJolt(meshData.vertices[meshData.indices[i + 2]])
				));
			}

		}

		if (!allTriangles.empty()) {
			JPH::MeshShapeSettings settings(allTriangles);
			auto result = settings.Create();

			if (result.IsValid()) {
				// 2292개의 Body 대신, "Tile-1-1"이라는 이름의 거대한 Shape 딱 하나만 저장!
				_staticMeshTiles.push_back({ tileName, "Merged_StaticMeshes", result.Get() });
			}
		}
		MYLOG("Merged " << meshes.size() << " primitives into 1 Single Physics Shape for tile: " << tileName);
	}
	void MapDataManager::LoadAllStaticMeshes(std::string_view baseDirPath)
	{
		namespace fs = std::filesystem;
		for (const auto& entry : fs::directory_iterator(baseDirPath))
		{
			if (entry.is_directory())
			{
				std::string tileName = entry.path().filename().string();
				// 폴더명과 동일한 이름의 .gltf 파일이 있는지 확인 (Unreal Batch Export 규칙)
				fs::path gltfPath = entry.path() / (tileName + ".gltf");

				if (fs::exists(gltfPath))
				{
					LoadStaticMeshShapes(tileName, gltfPath.string());
				}
			}
		}
		MYLOG("All Static Mesh Tiles Loaded from: " << baseDirPath);
	}

	void MapDataManager::LoadExportedScene(const std::string& groupName, std::string_view jsonPath)
	{
		std::ifstream file(jsonPath.data());
		if (!file.is_open()) return;

		nlohmann::json sceneJson;
		file >> sceneJson;

		std::filesystem::path basePath = std::filesystem::path(jsonPath).parent_path();

		for (const auto& objJson : sceneJson) {
			std::string meshFile = objJson.value("MeshFile", "");
			if (meshFile.empty()) continue;

			// 1. Transform 행렬 계산 (Location, Rotation, Scale)
			const auto& transJson = objJson["Transform"];

			// 클라이언트와 동일하게 Z축 반전 적용하여 행렬 생성
			XMMATRIX scaleMat = XMMatrixScaling(transJson["Scale"]["X"], transJson["Scale"]["Y"],
				transJson["Scale"]["Z"]);
			XMMATRIX rotMat = XMMatrixRotationQuaternion(XMVectorSet(
				transJson["Rotation"]["X"], transJson["Rotation"]["Y"],
				transJson["Rotation"]["Z"], transJson["Rotation"]["W"])); // Z 반전
			XMMATRIX locMat = XMMatrixTranslation(
				transJson["Location"]["X"], transJson["Location"]["Y"],
				transJson["Location"]["Z"]); // Z 반전

			XMMATRIX worldMat = scaleMat * rotMat * locMat;
			XMFLOAT4X4 externalTransform;
			XMStoreFloat4x4(&externalTransform, worldMat);

			// 2. 개별 glTF 로드 (위에서 만든 행렬 적용)
			std::string fullPath = (basePath / meshFile).string();
			auto meshes = glTFMeshLoader::LoadStaticMeshWithTransform(fullPath, externalTransform);

			// 3. Jolt Shape 생성 및 저장
			for (auto& meshData : meshes) {
				JPH::TriangleList triangles;
				for (size_t i = 0; i < meshData.indices.size(); i += 3) {
					triangles.push_back(JPH::Triangle(
						PIP::Utils::ToJolt(meshData.vertices[meshData.indices[i]]),
						PIP::Utils::ToJolt(meshData.vertices[meshData.indices[i + 1]]),
						PIP::Utils::ToJolt(meshData.vertices[meshData.indices[i + 2]])
					));
				}

				auto result = JPH::MeshShapeSettings(triangles).Create();
				if (result.IsValid()) {
					// 그룹 이름(groupName)을 tileName으로 사용하여 나중에 한 번에 찾을 수 있게 함
					_staticMeshTiles.push_back({ groupName, meshData.name, result.Get() });
				}
			}
		}
		MYLOG("Loaded Exported Scene for [" << groupName << "] from " << jsonPath);
	}

	std::vector<const StaticMeshTile*> MapDataManager::GetStaticMeshGroup(const std::string& groupName) const
	{
		std::vector<const StaticMeshTile*> result;

		auto it = _manualGroups.find(groupName);
		if (it == _manualGroups.end()) return result;

		// 그룹에 등록된 타일 이름들 ("Tile_X-1_Y-1" 등)을 순회하며
		// 해당 타일에 속한 모든 StaticMeshTile의 포인터를 담음
		for (const auto& targetTileName : it->second) {
			for (const auto& smTile : _staticMeshTiles) {
				if (smTile.tileName == targetTileName) {
					result.push_back(&smTile);
				}
			}
		}
		return result;
	}


	void MapDataManager::AddTerrainGroup(const std::string& groupName, const std::vector<std::string>& tileNames)
	{
		_manualGroups[groupName] = tileNames;
		MYLOG("[MapData] Group '" << groupName << "' defined with " << tileNames.size() << " tiles.");
	}

	std::vector<const TerrainTile*> MapDataManager::GetTerrainGroup(const std::string& groupName) const
	{
		std::vector<const TerrainTile*> result;

		auto it = _manualGroups.find(groupName);
		if (it == _manualGroups.end()) {
			MYERROR("[MapData] Group '" << groupName << "' not found!");
			return result;
		}

		// 그룹 내 정의된 타일 이름을 순회하며 실제 타일 포인터를 찾아 담음
		for (const auto& tileName : it->second) {
			for (const auto& tile : _terrainTiles) {
				if (tile.name == tileName) {
					result.push_back(&tile);
					break;
				}
			}
		}
		return result;
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