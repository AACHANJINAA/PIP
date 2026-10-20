#pragma once
#include <cstdint>
namespace ECS
{
	// 용어 (ECS_Migration_KR.md 4.1)
	// - 핸들(Entity): 슬롯 + 세대를 합친 64비트 전체. 밖에서 들고 다니는 값
	// - 슬롯(slot): 하위 32비트. Registry::_versions와 모든 Pool::_sparse의 칸 번호 (레지스트리 공용)
	// - 세대(version): 상위 32비트. 그 슬롯이 몇 번 지워졌는지. 옛 핸들을 거르는 데 씀
	// - 위치(pos): 저장소(Pool) 하나 안에서 몇 번째 칸인지. 저장소마다 따로라 핸들에는 들어 있지 않음
	using Entity = uint64_t;

	constexpr Entity make(uint32_t slot, uint32_t version) {
		return static_cast<Entity>(version) << 32 | slot;
	}
	constexpr uint32_t slot(Entity e) {
		return static_cast<uint32_t>(e);
	}
	constexpr uint32_t version(Entity e) {
		return static_cast<uint32_t>(e >> 32);
	}
	constexpr Entity kNull = ~static_cast<Entity>(0);
}
