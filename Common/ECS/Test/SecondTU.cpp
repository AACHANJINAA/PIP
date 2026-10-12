// 두 번째 번역 단위
// - Pool.h를 맨 처음 include: 다른 헤더 없이 혼자 컴파일되는지 (헤더 자립성)
// - 이후 type_id<T>()가 생기면 main.cpp와 같은 번호를 받는지 여기서 확인한다 (ECS_Migration_KR.md 4.2)
#include "ECS/Pool.h"
#include "ECS/Registry.h"

int second_tu_smoke()
{
	ECS::Pool<int> p;
	const ECS::Entity e = ECS::make(0, 0);
	p.emplace(e, 1);
	return p.get(e);
}
