#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include "glTFMeshLoader.h"
#include "../../Common/TerrainData.h"
#include "../../Common/Vector3.h"

// [수정] 기존 MapObject 구조체는 유지 (AABB 충돌용)
// 필요하다면 Common으로 이동 가능

namespace PIP
{
	struct MapObject
	{
		common::Vec3 _center;
		common::Quat _rotation;
		common::Vec3 _extent; // Half-extents (반폭)
	};
	struct TerrainTile {
		JPH::ShapeRefC shape; // 공유할 물리 모양 (레퍼런스 카운팅 포인터)
		common::TerrainData data;
	};
	struct StaticMeshTile {
		JPH::ShapeRefC shape;
		std::string name;
	};
	class MapDataManager : public Singleton<MapDataManager>
	{
		friend class Singleton<MapDataManager>;

	public:
		MapDataManager() = default;
		~MapDataManager() = default;

		void LoadMapData(std::string_view mapDataPath);
		void LoadHeightMapData(std::string_view heightMapDataJSONPath);
		void LoadMainLandscapeData(std::string_view landscapeDirPath);
		const std::vector<TerrainTile>& GetTerrainTiles() const { return _terrainTiles; }

		// glTF에서 Shape들을 생성하여 보관 (서버 시작 시 한 번만 호출)
		void LoadStaticMeshShapes(std::string_view gltfPath);
		// 각 방에서 참조할 Shape 리스트 반환
		const std::vector<StaticMeshTile>& GetStaticMeshTiles() const { return _staticMeshTiles; }



		float GetGroundHeight(float x, float z) const;
		common::Vec3 AdjustPositionToGround(common::Vec3 position);

		bool IsInsideMap(float x, float z) const;
		const std::vector<MapObject>& GetMapObjects() const { return _map_objects; }

		// [추가] 월드 경계 반환 (minX, maxX, minZ, maxZ)
		std::tuple<float, float, float, float> GetWorldBounds() const { return { _worldMinX, _worldMaxX, _worldMinZ, _worldMaxZ }; }
	private:
		std::vector<MapObject> _map_objects;
		std::vector<TerrainTile> _terrainTiles;
		std::vector<StaticMeshTile> _staticMeshTiles;
		float _worldMinX = std::numeric_limits<float>::max();
		float _worldMaxX = -std::numeric_limits<float>::max();
		float _worldMinZ = std::numeric_limits<float>::max();
		float _worldMaxZ = -std::numeric_limits<float>::max();
	};
}

