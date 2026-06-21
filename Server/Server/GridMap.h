#pragma once
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include "Vector3.h"

namespace PIP::GAME
{
	class GameObject;
	class NPC;
	class Player;

	class GridMap
	{
	public:
		GridMap() = default;
		~GridMap() = default;

		void Initialize(float minX, float maxX, float minZ, float maxZ, int cellSize);
		void Clear(); // 씬 전환 등에서 모든 오브젝트 제거

		void Add(GameObject* obj);
		void Remove(GameObject* obj);

		// 위치 업데이트 시 셀이 바뀌었는지 체크
		// return: 셀이 바뀌었으면 true (AOI 갱신 등이 필요한 경우)
		bool UpdatePosition(GameObject* obj, common::Vec3 newPos);

		// 주변 객체 찾기 (3x3 셀 탐색)
		void GetNearbyObjects(common::Vec3 center, std::vector<GameObject*>& outList, int typeFilter = 0) const;

		void GetSameCellObjects(common::Vec3 center, std::vector<GameObject*>& outList, int typeFilter = 0) const;

		// --- [최적화용] 셀 단위 접근 인터페이스 ---
		int GetCellIndex(common::Vec3 pos) const { return GetIndex(pos); }
		void GetNearbyCellIndices(int cellIndex, std::vector<int>& outIndices) const;
		
		// 타입별 직접 접근 (dynamic_cast 제거 목적)
		const std::unordered_set<GameObject*>& GetObjectsInCell(int cellIndex) const { return _cells[cellIndex].objects; }
		const std::vector<NPC*>& GetNpcsInCell(int cellIndex) const { return _cells[cellIndex].npcs; }
		const std::vector<Player*>& GetPlayersInCell(int cellIndex) const { return _cells[cellIndex].players; }

		int GetCellCount() const { return static_cast<int>(_cells.size()); }

	private:
		int GetIndex(common::Vec3 pos) const;
		int GetIndex(int x, int y) const;

	private:
		struct Cell {
			std::unordered_set<GameObject*> objects;
			std::vector<NPC*> npcs;
			std::vector<Player*> players;
		};

		float _minX = 0, _minZ = 0;
		float _maxX = 0, _maxZ = 0;
		int _cellSize = 1;
		int _cols = 0, _rows = 0;

		// 각 셀에 포함된 객체들 (타입별 분리 관리)
		std::vector<Cell> _cells;

		// 각 객체가 어떤 셀에 있는지 저장 (Remove/Update 시 활용)
		std::unordered_map<GameObject*, int> _objectCellIndex;
	};
}
