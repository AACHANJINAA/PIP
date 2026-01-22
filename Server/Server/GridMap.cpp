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

        if (_cellSize <= 0) _cellSize = 50; // 안전장치

        float width = maxX - minX;
        float height = maxZ - minZ;

        _cols = static_cast<int>(width / _cellSize) + 1;
        _rows = static_cast<int>(height / _cellSize) + 1;

        _cells.clear();
        _cells.resize(_cols * _rows);

        MYLOG("[GridMap] Initialized: " << _cols << "x" << _rows << " cells (Size: " << _cellSize << ")");
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

        int idx = GetIndex(tc->GetPosition());
        _cells[idx].insert(obj);
    }

    void GridMap::Remove(GameObject* obj)
    {
        if (!obj) return;
        auto tc = obj->GetComponent<TransformComponent>();
        if (!tc) return;

        int idx = GetIndex(tc->GetPosition());
        _cells[idx].erase(obj);
    }

    bool GridMap::UpdatePosition(GameObject* obj, common::Vec3 oldPos, common::Vec3 newPos)
    {
        int oldIdx = GetIndex(oldPos);
        int newIdx = GetIndex(newPos);

        if (oldIdx == newIdx) return false; // 같은 셀 내에서의 이동

        _cells[oldIdx].erase(obj);
        _cells[newIdx].insert(obj);
        return true; // 셀 변경됨 (AOI 갱신 필요)
    }

    void GridMap::GetNearbyObjects(common::Vec3 center, std::vector<GameObject*>& outList, int typeFilter) const
    {
        int cx = static_cast<int>((center.x - _minX) / _cellSize);
        int cy = static_cast<int>((center.z - _minZ) / _cellSize);

        // 주변 9칸 (3x3) 검사
        for (int y = cy - 1; y <= cy + 1; ++y)
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
}
