#include "pch.h"
#include "GridMap.h"
#include "GameObject.h"
#include "TransformComponent.h"

namespace PIP::GAME
{
    void GridMap::Initialize(float minX, float maxX, float minZ, float maxZ, int cellSize)
    {
        _minX = minX; _maxX = maxX;
        _minZ = minZ; _maxZ = maxZ;
        _cellSize = cellSize;

        if (_cellSize <= 0) _cellSize = 50;

        float width = maxX - minX;
        float height = maxZ - minZ;

        _cols = static_cast<int>(width / _cellSize) + 1;
        _rows = static_cast<int>(height / _cellSize) + 1;

        _cells.clear();
        _cells.resize(_cols * _rows);
        _objectCellIndex.clear(); // 맵 초기화

        MYLOG("[GridMap] Initialized: " << _cols << "x" << _rows << " cells");
    }

    int GridMap::GetIndex(common::Vec3 pos) const
    {
        int x = static_cast<int>((pos.x - _minX) / _cellSize);
        int y = static_cast<int>((pos.z - _minZ) / _cellSize);

        // 맵 밖으로 나가면 클램핑
        x = std::max(0, std::min(x, _cols - 1));
        y = std::max(0, std::min(y, _rows - 1));

        return y * _cols + x;
    }

    int GridMap::GetIndex(int x, int y) const
    {
        if (x < 0 || x >= _cols || y < 0 || y >= _rows) return -1;
        return y * _cols + x;
    }

    void GridMap::Add(GameObject* obj)
    {
        if (!obj) return;
        auto tc = obj->GetComponent<TransformComponent>();
        if (!tc) return;

		int idx = GetIndex(tc->GetPosition()); // TODO: 주의!! 물리 위치가 아닐수 있다. Transform 위치를 기준으로 셀에 넣는다면, 물리 객체가 있는 경우 위치 불일치 가능성 있음. (NPC 등)
        _cells[idx].insert(obj);

        // [수정] 이 객체가 어떤 셀에 들어갔는지 기록해야 나중에 Remove에서 찾을 수 있습니다.
        _objectCellIndex[obj] = idx;
    }

    void GridMap::Remove(GameObject* obj)
    {
        if (!obj) return;

        // [수정] 재귀 호출 삭제하고 바로 처리
        auto it = _objectCellIndex.find(obj);
        if (it != _objectCellIndex.end())
        {
            int idx = it->second;

            // 셀 범위 체크
            if (idx >= 0 && idx < _cells.size()) {
                _cells[idx].erase(obj); // 실제 GridCell에서 제거
            }

            // 인덱스 맵에서 제거
            _objectCellIndex.erase(it);
        }
    }

    bool GridMap::UpdatePosition(GameObject* obj, common::Vec3 newPos)
    {
        if (!obj) return false;

        int newIdx = GetIndex(newPos);

        // 현재 어느 셀에 있는지 조회
        int oldIdx = -1;
        auto it = _objectCellIndex.find(obj);
        if (it != _objectCellIndex.end()) {
            oldIdx = it->second;
        }

        // 셀이 안 바뀌었으면 패스
        if (oldIdx == newIdx) return false;

        // 바뀌었으면 이동 (Remove -> Add 최적화)
        if (oldIdx != -1) _cells[oldIdx].erase(obj);

        _cells[newIdx].insert(obj);
        _objectCellIndex[obj] = newIdx; // 갱신

        return true; // AOI 갱신 필요함 알림
    }

    void GridMap::GetNearbyObjects(common::Vec3 center, std::vector<GameObject*>& outList, int typeFilter) const
    {
        int cx = static_cast<int>((center.x - _minX) / _cellSize);
        int cz = static_cast<int>((center.z - _minZ) / _cellSize);

        // 주변 9칸 (3x3) 검사
        for (int y = cz - 1; y <= cz + 1; ++y)
        {
            for (int x = cx - 1; x <= cx + 1; ++x)
            {
                int idx = GetIndex(x, y);
                if (idx == -1) continue;

                for (auto* obj : _cells[idx])
                {
                    // (옵션) 여기서 거리 체크를 한 번 더 하면 원형 AOI가 됨
                    // 지금은 사각형(Grid) 방식이라 그냥 다 넣음
                    outList.push_back(obj);
                }
            }
        }
    }

    void GridMap::GetSameCellObjects(common::Vec3 center, std::vector<GameObject*>& outList, int typeFilter) const
    {
        int cx = static_cast<int>((center.x - _minX) / _cellSize);
        int cz = static_cast<int>((center.z - _minZ) / _cellSize);

        // 주변 9칸 (3x3) 검사
        int idx = GetIndex(cx, cz);

        for (auto* obj : _cells[idx])
        {
            // (옵션) 여기서 거리 체크를 한 번 더 하면 원형 AOI가 됨
            // 지금은 사각형(Grid) 방식이라 그냥 다 넣음
            outList.push_back(obj);
        }

    }
}
