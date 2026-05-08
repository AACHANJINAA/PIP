#include "pch.h"
#include "GridMap.h"
#include "GameObject.h"
#include "NPC.h"
#include "Player.h"
#include "TransformComponent.h"
#include <algorithm>

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
		_objectCellIndex.clear();

		MYLOG("[GridMap] Optimized Initialized: " << _cols << "x" << _rows << " cells (Typed Storage)");
	}

	int GridMap::GetIndex(common::Vec3 pos) const
	{
		int x = static_cast<int>((pos.x - _minX) / _cellSize);
		int y = static_cast<int>((pos.z - _minZ) / _cellSize);

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
		auto& cell = _cells[idx];
		
		cell.objects.insert(obj);
		if (auto npc = dynamic_cast<NPC*>(obj)) {
			cell.npcs.push_back(npc);
		}
		else if (auto player = dynamic_cast<Player*>(obj)) {
			cell.players.push_back(player);
		}

		_objectCellIndex[obj] = idx;
	}

	void GridMap::Remove(GameObject* obj)
	{
		if (!obj) return;

		auto it = _objectCellIndex.find(obj);
		if (it != _objectCellIndex.end())
		{
			int idx = it->second;
			if (idx >= 0 && idx < (int)_cells.size()) {
				auto& cell = _cells[idx];
				cell.objects.erase(obj);
				
				if (auto npc = dynamic_cast<NPC*>(obj)) {
					cell.npcs.erase(std::remove(cell.npcs.begin(), cell.npcs.end(), npc), cell.npcs.end());
				}
				else if (auto player = dynamic_cast<Player*>(obj)) {
					cell.players.erase(std::remove(cell.players.begin(), cell.players.end(), player), cell.players.end());
				}
			}
			_objectCellIndex.erase(it);
		}
	}

	bool GridMap::UpdatePosition(GameObject* obj, common::Vec3 newPos)
	{
		if (!obj) return false;

		int newIdx = GetIndex(newPos);
		int oldIdx = -1;
		auto it = _objectCellIndex.find(obj);
		if (it != _objectCellIndex.end()) {
			oldIdx = it->second;
		}

		if (oldIdx == newIdx) return false;

		if (oldIdx != -1) {
			auto& oldCell = _cells[oldIdx];
			oldCell.objects.erase(obj);
			if (auto npc = dynamic_cast<NPC*>(obj)) {
				oldCell.npcs.erase(std::remove(oldCell.npcs.begin(), oldCell.npcs.end(), npc), oldCell.npcs.end());
			}
			else if (auto player = dynamic_cast<Player*>(obj)) {
				oldCell.players.erase(std::remove(oldCell.players.begin(), oldCell.players.end(), player), oldCell.players.end());
			}
		}

		auto& newCell = _cells[newIdx];
		newCell.objects.insert(obj);
		if (auto npc = dynamic_cast<NPC*>(obj)) {
			newCell.npcs.push_back(npc);
		}
		else if (auto player = dynamic_cast<Player*>(obj)) {
			newCell.players.push_back(player);
		}
		
		_objectCellIndex[obj] = newIdx;

		return true;
	}

	void GridMap::GetNearbyObjects(common::Vec3 center, std::vector<GameObject*>& outList, int typeFilter) const
	{
		int cx = static_cast<int>((center.x - _minX) / _cellSize);
		int cz = static_cast<int>((center.z - _minZ) / _cellSize);

		for (int y = cz - 1; y <= cz + 1; ++y)
		{
			for (int x = cx - 1; x <= cx + 1; ++x)
			{
				int idx = GetIndex(x, y);
				if (idx == -1) continue;

				for (auto* obj : _cells[idx].objects)
				{
					outList.push_back(obj);
				}
			}
		}
	}

	void GridMap::GetSameCellObjects(common::Vec3 center, std::vector<GameObject*>& outList, int typeFilter) const
	{
		int idx = GetIndex(center);
		for (auto* obj : _cells[idx].objects)
		{
			outList.push_back(obj);
		}
	}

	void GridMap::GetNearbyCellIndices(int cellIndex, std::vector<int>& outIndices) const
	{
		if (cellIndex < 0 || cellIndex >= static_cast<int>(_cells.size())) return;

		int cx = cellIndex % _cols;
		int cz = cellIndex / _cols;

		for (int y = cz - 1; y <= cz + 1; ++y)
		{
			for (int x = cx - 1; x <= cx + 1; ++x)
			{
				int idx = GetIndex(x, y);
				if (idx != -1)
				{
					outIndices.push_back(idx);
				}
			}
		}
	}
}
