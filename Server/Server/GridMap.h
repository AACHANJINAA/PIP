#pragma once
namespace PIP::GAME
{
	class GameObject;

	class GridMap
	{
	public:
		GridMap() = default;
		~GridMap() = default;

		// 초기화: 맵 전체 크기와 셀 하나 크기 설정
		// 예: 2000x2000 맵, 셀 크기 50 -> 40x40 격자 생성
		void Initialize(float minX, float maxX, float minZ, float maxZ, int cellSize);

		void Add(GameObject* obj);
		void Remove(GameObject* obj);

		// 객체가 이동했을 때 셀 갱신
		// return: 셀이 변경되었으면 true (AOI 패킷 전송 트리거용)
		bool UpdatePosition(GameObject* obj, common::Vec3 oldPos, common::Vec3 newPos);

		// 주변 객체 찾기 (플레이어 시야 처리용)
		// typeFilter: 특정 타입(NPC, Player 등)만 골라낼 때 사용 (0이면 전체)
		void GetNearbyObjects(common::Vec3 center, std::vector<GameObject*>& outList, int typeFilter = 0) const;
	private:
		// 좌표 -> 인덱스 변환
		int GetIndex(common::Vec3 pos) const;
		int GetIndex(int x, int y) const;

	private:
		float _minX = 0, _minZ = 0;
		float _maxX = 0, _maxZ = 0;
		int _cellSize = 1;
		int _cols = 0, _rows = 0;

		// 각 셀마다 객체 포인터들을 저장
		std::vector<std::unordered_set<GameObject*>> _cells;
	};
}
