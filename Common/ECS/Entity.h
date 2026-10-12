#pragma once
#include <cstdint>
namespace ECS
{
	using Entity = uint64_t;

	constexpr Entity make(uint32_t index, uint32_t version) {
		return static_cast<Entity>(version) << 32 | index;
	}
	constexpr uint32_t index(Entity e) {
		return static_cast<uint32_t>(e);
	}
	constexpr uint32_t version(Entity e) {
		return static_cast<uint32_t>(e >> 32);
	}
	constexpr Entity kNull = ~static_cast<Entity>(0);
}
