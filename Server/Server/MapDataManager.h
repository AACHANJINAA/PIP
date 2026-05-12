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
		std::string name;
		JPH::ShapeRefC shape; // 공유할 물리 모양 (레퍼런스 카운팅 포인터)
		common::TerrainData data;
	};
	// StaticMeshTile 구조체 변경
	struct StaticMeshTile {
		std::string tileName; // e.g., "Tile_X-1_Y-1"
		std::string meshName; // glTF 내부의 메쉬 이름
		JPH::ShapeRefC shape;
		JPH::Vec3 position;
		JPH::Quat rotation;
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
		void LoadStaticMeshShapes(const std::string& tileName, std::string_view jsonPath, bool enableBinSave = false);
		void LoadAllStaticMeshes(std::string_view baseDirPath); ;// [추가] 특정 폴더 내의 모든 Tile_... 형식의 glTF를 자동 로드
		void LoadExportedScene(const std::string& groupName, std::string_view jsonPath);

		// [추가] 서버 전용: 클라이언트에서 Export한 JSON을 로드하여 StaticMeshTile 리스트로 변환 (맵 데이터와 별개로 관리)
		void LoadServerExportData(const std::string& groupName, std::string_view jsonPath, bool enableBinSave = false); 

		std::vector<const StaticMeshTile*> GetStaticMeshGroup(const std::string& groupName) const;// 각 방에서 참조할 Shape 리스트 반환

		// [추가] 수동 그룹화 정의: "그룹명"과 "포함될 타일 이름들"을 매핑
		// 예: AddTerrainGroup("VillageStage", {"Landscape01", "Landscape02", "Landscape05"})
		void AddTerrainGroup(const std::string& groupName, const std::vector<std::string>& tileNames);

		// [추가] 그룹명을 넣어 해당 그룹에 속한 타일 포인터 리스트를 반환
		// 예: GetTerrainGroup("VillageStage") -> 포인터 리스트 반환
		std::vector<const TerrainTile*> GetTerrainGroup(const std::string& groupName) const;

		float GetGroundHeight(float x, float z) const;
		common::Vec3 AdjustPositionToGround(common::Vec3 position);

		bool IsInsideMap(float x, float z) const;
		const std::vector<MapObject>& GetMapObjects() const { return _map_objects; }

		const std::vector<StaticMeshTile>& get_find_mesh() const { return _findMeshShape; }
		// [추가] 월드 경계 반환 (minX, maxX, minZ, maxZ)
		std::tuple<float, float, float, float> GetWorldBounds() const { return { _worldMinX, _worldMaxX, _worldMinZ, _worldMaxZ }; }
	private:
		std::vector<MapObject> _map_objects;
		std::vector<TerrainTile> _terrainTiles;
		std::vector<StaticMeshTile> _staticMeshTiles;
		// [추가] 그룹 정의를 보관하는 맵 (그룹명 -> 타일 이름 리스트)
		std::unordered_map<std::string, std::vector<std::string>> _manualGroups;
		float _worldMinX = std::numeric_limits<float>::max();
		float _worldMaxX = -std::numeric_limits<float>::max();
		float _worldMinZ = std::numeric_limits<float>::max();
		float _worldMaxZ = -std::numeric_limits<float>::max();
		std::vector<StaticMeshTile> _findMeshShape;
	};
}
