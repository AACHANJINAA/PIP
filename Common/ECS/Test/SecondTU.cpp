// 두 번째 번역 단위
// - Pool.h를 맨 처음 include: 다른 헤더 없이 혼자 컴파일되는지 (헤더 자립성)
// - type_id<T>()가 main.cpp와 같은 번호를 받는지 확인 (ECS_Migration_KR.md 4.2)
#include "ECS/Pool.h"
#include "ECS/Registry.h"
#include "ECS/TypeId.h"
#include "TestComponents.h"

int second_tu_smoke()
{
	ECS::Pool<int> p;
	const ECS::Entity e = ECS::make(0, 0);
	p.emplace(e, 1);
	return p.get(e);
}

// main.cpp와 다른 순서로 번호를 받는다: Position → Dead → Health
//
// 주의: 카운터가 파일마다 따로인 잘못된 구현(익명 namespace, static 함수)이어도 Position·Dead·Health 비교만으로는 못 잡는다.
// 세 타입은 main.cpp도 type_id를 부르므로, 링커가 같은 이름의 type_id<Health> 등을 하나로 합쳐(COMDAT) 두 파일이 같은 카운터를
// 쓰게 되어 우연히 맞는다 (2026-10-13 MSVC에서 실제로 통과함. ODR 위반이라 결과는 링커 마음대로).
// 그래서 이 파일에서만 쓰는 타입(익명 namespace라 이 파일 전용)도 하나 번호를 받는다. 이 번호는 합쳐질 상대가 없어 이 파일의
// type_id_impl·카운터를 반드시 거치므로, 카운터가 따로면 0부터 다시 세서 main.cpp가 받은 번호와 겹친다 (main.cpp의 핵심 검사)
namespace { struct OnlyInSecondTu {}; }

SecondTuIds second_tu_type_ids()
{
	SecondTuIds ids{};
	ids.position = ECS::type_id<Position>();
	ids.dead = ECS::type_id<Dead>();
	ids.health = ECS::type_id<Health>();
	ids.onlyHere = ECS::type_id<OnlyInSecondTu>();
	return ids;
}
