#pragma once
#include "Entity.h"
#include <vector>
#include <cassert>
#include <cstdint>
#include <utility>

namespace ECS
{
	struct IPool 
	{
		virtual ~IPool() = default;
		virtual bool contains(Entity e) const = 0;
		virtual void remove(Entity e) = 0;
		virtual size_t size() const = 0;
	};

	template<class T>
	class Pool : public IPool
	{
	public:
		static constexpr uint32_t kInvalid = UINT32_MAX;   // sparse의 "없음" 표시
		
		bool contains(Entity e) const override
		{
			return index(e) < _sparse.size() && _sparse[index(e)] != kInvalid && _dense[_sparse[index(e)]] == e;
			//      ← sparse 배열 범위 안인가	     ← 그 칸에 위치가 적혀 있나         ← 그 위치의 엔티티가 정말 e인가(세대까지 비교)
		}

		template<class ...Args>
		T& emplace(Entity e, Args&&... args)
		{
			assert(!contains(e));
			if (index(e) >= _sparse.size())
				_sparse.resize(index(e) + 1, kInvalid);
			_sparse[index(e)] = static_cast<uint32_t>(_dense.size());
			_dense.push_back(e);
			_data.emplace_back(std::forward<Args>(args)...);
			return _data.back();
		}

		void remove(Entity e) override
		{
			assert(contains(e));
			size_t pos = _sparse[index(e)];           // 지울 위치
			size_t last = _dense.size() - 1;           // 맨 끝 위치

			if (pos != last) {
				_dense[pos] = _dense[last];                 // 맨 끝 엔티티를 빈자리로
				_data[pos] = std::move(_data[last]);       // 컴포넌트도 같이
				_sparse[index(_dense[pos])] = static_cast<uint32_t>(pos);          // 옮긴 엔티티의 색인 갱신
			}
			_dense.pop_back();
			_data.pop_back();
			_sparse[index(e)] = kInvalid;                   // 지운 엔티티의 색인은 맨 마지막에 지움
		}
		size_t size() const override { return _dense.size(); }
		const std::vector<Entity>& entities() const { return _dense; }
		std::vector<T>& data() { return _data; }

		T& get(Entity e)
		{
			assert(contains(e));
			return _data[_sparse[index(e)]];
		}
		const T& get(Entity e) const
		{
			assert(contains(e)); 
			return _data[_sparse[index(e)]];
		}
		T* try_get(Entity e)
		{
			return contains(e) ? &_data[_sparse[index(e)]] : nullptr;
		}
		const T* try_get(Entity e) const
		{
			return contains(e) ? &_data[_sparse[index(e)]] : nullptr;
		}
		
	private:
		std::vector<uint32_t>	_sparse;   // Entity index → dense index (kInvalid이면 없음)
		std::vector<Entity>		_dense;    // 위치 → 엔티티 (세대 포함 전체 ID)
		std::vector<T>			_data;     // 위치 → 컴포넌트
		// 주의 _dense와 _data는 항상 같은 길이, 같은 순서 _dense[i]의 컴포넌트가 _data[i]. 모든 함수가 이 규칙을 지켜야 함.
		
	};
}
