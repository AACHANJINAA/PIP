#pragma once
#include <cassert>
#include <vector>
#include <deque>
#include "Entity.h"

namespace ECS
{
	struct IterGuard {
		int& n;
		IterGuard(int& n) : n(n) { ++n; }
		~IterGuard() { --n; }          // 어떻게 빠져나가든 반드시 실행
	};

	class Registry
	{
	public:
		Entity create();
		void destroy(Entity e);
		bool valid(Entity e) const;
	private:
		int _iterating = 0;	// 순회 중첩 깊이 (0이면 순회 중 아님). 순회 중에 create/destroy 불가

		std::vector<uint32_t>	_versions;	// 사물함 번호별 "지금 몇 번째 사용자인지"
		std::deque<uint32_t>	_free;		// 비어 있는 사물함 번호 목록
	};

	// 정의는 헤더에 둔다 (Common은 헤더 전용: 클라 stdafx.h·서버 pch.h 중 어느 pch도 강제하지 않기 위해, ECS_Migration_KR.md 4.2)

	inline Entity Registry::create()
	{
		assert(_iterating == 0);

		if (_free.empty())
		{
			//	없으면 → 새 번호 = _versions.size(), _versions에 0을 추가하고(번호, 0)으로 ID를 만듦
			uint32_t newIndex = static_cast<uint32_t>(_versions.size());
			_versions.push_back(0);
			return make(newIndex, 0);
		}
		//빈 사물함(_free)이 있으면 → 그 번호를 꺼내고, 그 칸의 현재 세대로 ID를 만듦
		uint32_t newIndex = _free.front();
		_free.pop_front();
		return make(newIndex, _versions[newIndex]);
	}

	inline void Registry::destroy(Entity e)
	{
		assert(_iterating == 0);
		assert(valid(e));

		// TODO: Pool에 들어 있는 경우 제거. Pool이 없으면 그냥 무시
		//for (auto& p : _pools)
		//	if (p && p->contains(e)) p->remove(e);

		++_versions[index(e)];            // 옛 ID 전부 무효
		_free.push_back(index(e));        // 빈칸으로 반납
	}
	inline bool Registry::valid(Entity e) const
	{
		return index(e) < _versions.size() && _versions[index(e)] == version(e);
		//index(e)가 _versions 범위 안이고
		//	&& _versions[index(e)] == version(e)
	}
}
