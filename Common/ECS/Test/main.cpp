// ECS 단독 테스트 (ECS_Migration_KR.md 4.3 M1~M5)
// - pch 없이 빌드한다: ECS 헤더가 스스로 필요한 include를 갖고 있는지 확인
// - assert는 Release에서 사라지므로 테스트는 CHECK를 쓴다 (Release에서도 동작)
// - 테스트는 의도적으로 assert에 걸리는 호출(중복 추가, 없는 엔티티 get 등)을 하지 않는다
#include <cstdio>
#include <memory>
#include <string>

// 콘솔 한글 출력용 (소스는 /utf-8이라 UTF-8로 출력되는데, 콘솔 기본 코드 페이지는 CP949)
// 클라·서버처럼 windows.h를 ECS 헤더보다 먼저 포함해서, 같은 매크로 환경에서도 컴파일되는지 같이 확인한다
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "ECS/Registry.h"
#include "ECS/Pool.h"
#include "ECS/TypeId.h"
#include "TestComponents.h"

static int g_failed = 0;

#define CHECK(expr)                                                         \
	do {                                                                    \
		if (!(expr)) {                                                      \
			std::printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr);   \
			++g_failed;                                                     \
		}                                                                   \
	} while (0)

// SecondTU.cpp: Pool.h를 맨 처음 include하는 다른 번역 단위 (헤더 자립성, type_id 교차 확인)
// 선언은 TestComponents.h (second_tu_smoke, second_tu_type_ids)

using namespace ECS;

// 테스트용 컴포넌트(Health, Dead, Name, Owned, Position)는 TestComponents.h

// Pool 불변식: dense[i]의 컴포넌트가 data[i]이고, contains·get이 그 칸을 가리킨다
template<class T>
static bool pool_consistent(Pool<T>& pool)
{
	const auto& ents = pool.entities();
	auto& data = pool.data();
	if (ents.size() != data.size() || ents.size() != pool.size()) return false;
	for (size_t i = 0; i < ents.size(); ++i)
	{
		if (!pool.contains(ents[i])) return false;
		if (&pool.get(ents[i]) != &data[i]) return false;
	}
	return true;
}

// ---------------------------------------------------------------- Entity
static void test_entity()
{
	std::printf("[Entity]\n");

	const Entity e = make(7, 3);
	CHECK(index(e) == 7);
	CHECK(version(e) == 3);

	// 비트 경계: 32비트 끝값이 서로 섞이지 않는지
	const Entity big = make(UINT32_MAX - 1, UINT32_MAX - 1);
	CHECK(index(big) == UINT32_MAX - 1);
	CHECK(version(big) == UINT32_MAX - 1);

	CHECK(make(0, 0) != make(0, 1));					// 같은 칸, 다른 세대는 다른 ID
	CHECK(index(kNull) == UINT32_MAX);
	CHECK(version(kNull) == UINT32_MAX);

	static_assert(sizeof(Entity) == 8, "Entity는 64비트");
	static_assert(index(make(5, 9)) == 5, "constexpr로 쓸 수 있어야 함");
}

// ---------------------------------------------------------------- Pool
static void test_pool_basic()
{
	std::printf("[Pool] 추가·조회\n");
	Pool<int> p;
	const Entity a = make(0, 0), b = make(1, 0);

	CHECK(p.size() == 0);
	CHECK(!p.contains(a));
	CHECK(p.try_get(a) == nullptr);

	int& ref = p.emplace(a, 10);						// 명세: emplace는 추가한 컴포넌트의 참조를 돌려줌
	CHECK(ref == 10);
	p.emplace(b, 20);

	CHECK(p.size() == 2);
	CHECK(p.contains(a) && p.contains(b));
	CHECK(p.get(a) == 10 && p.get(b) == 20);

	p.get(a) += 5;										// get이 원본 참조인지 (복사본이면 15가 안 됨)
	CHECK(p.get(a) == 15);
	*p.try_get(b) = 99;									// try_get도 원본 포인터인지
	CHECK(p.get(b) == 99);

	CHECK(pool_consistent(p));
}

static void test_pool_sparse_growth()
{
	std::printf("[Pool] 순서 없는 인덱스\n");
	Pool<int> p;
	const Entity e7 = make(7, 0), e2 = make(2, 0), e100 = make(100, 0);

	p.emplace(e7, 7);									// 0번이 아니라 7번부터: sparse가 한 번에 늘어나야 함
	p.emplace(e2, 2);									// 이미 늘어난 범위 안
	p.emplace(e100, 100);								// 다시 크게 건너뜀

	CHECK(p.get(e7) == 7 && p.get(e2) == 2 && p.get(e100) == 100);
	CHECK(!p.contains(make(3, 0)));						// 사이에 비어 있는 칸
	CHECK(!p.contains(make(101, 0)));					// sparse 범위 밖
	CHECK(pool_consistent(p));
}

static void test_pool_remove()
{
	std::printf("[Pool] 삭제 (swap and pop)\n");
	Pool<int> p;
	const Entity a = make(0, 0), b = make(1, 0), c = make(2, 0), d = make(3, 0);
	p.emplace(a, 1); p.emplace(b, 2); p.emplace(c, 3); p.emplace(d, 4);

	p.remove(a);										// 맨 앞 삭제 → 마지막(d)이 0번 자리로
	CHECK(!p.contains(a));
	CHECK(p.try_get(a) == nullptr);
	CHECK(p.get(d) == 4);								// 옮겨진 원소를 찾을 수 있는지
	CHECK(p.size() == 3);
	CHECK(pool_consistent(p));

	p.remove(b);										// 가운데 삭제
	CHECK(!p.contains(b));
	CHECK(p.get(c) == 3 && p.get(d) == 4);
	CHECK(pool_consistent(p));

	const Entity last = p.entities().back();
	p.remove(last);										// 마지막 원소 삭제 (pos == last)
	CHECK(!p.contains(last));
	CHECK(p.size() == 1);
	CHECK(pool_consistent(p));

	const Entity only = p.entities().back();
	p.remove(only);										// 하나 남은 것 삭제
	CHECK(p.size() == 0);
	CHECK(!p.contains(only));

	p.emplace(a, 50);									// 지운 뒤 같은 엔티티를 다시 추가
	CHECK(p.contains(a) && p.get(a) == 50);
	CHECK(pool_consistent(p));
}

static void test_pool_stale()
{
	std::printf("[Pool] 옛 ID 거르기\n");
	Pool<int> p;
	const Entity oldE = make(5, 0);
	const Entity newE = make(5, 1);						// 같은 칸, 다음 세대

	p.emplace(newE, 42);
	CHECK(p.contains(newE));
	CHECK(!p.contains(oldE));							// sparse는 같은 칸을 가리키지만 dense의 세대가 다름
	CHECK(p.try_get(oldE) == nullptr);
}

static void test_pool_const()
{
	std::printf("[Pool] const 접근\n");
	Pool<int> p;
	const Entity a = make(0, 0);
	p.emplace(a, 3);

	const Pool<int>& cp = p;
	CHECK(cp.get(a) == 3);
	CHECK(cp.try_get(a) != nullptr && *cp.try_get(a) == 3);
	CHECK(cp.try_get(make(1, 0)) == nullptr);
	CHECK(cp.contains(a));
	CHECK(cp.size() == 1);
	CHECK(cp.entities().size() == 1);
}

static void test_pool_types()
{
	std::printf("[Pool] 컴포넌트 종류\n");

	Pool<Health> health;								// 생성자 없는 struct를 재료로 바로 생성
	const Entity a = make(0, 0), b = make(1, 0), c = make(2, 0);
	health.emplace(a, 100, 100);
	health.emplace(b, Health{ 50, 80 });				// 이미 만든 값 넘기기
	Health h{ 1, 2 };
	health.emplace(c, h);								// lvalue 넘기기 (복사)
	CHECK(health.get(a).cur == 100 && health.get(b).max == 80 && health.get(c).cur == 1);
	health.remove(a);
	CHECK(health.get(c).cur == 1 && health.get(c).max == 2);
	CHECK(pool_consistent(health));

	Pool<Dead> dead;									// 빈 태그
	dead.emplace(a);
	dead.emplace(b);
	dead.remove(a);
	CHECK(!dead.contains(a) && dead.contains(b));
	CHECK(pool_consistent(dead));

	Pool<Name> names;									// 삭제 시 이동으로 내용이 옮겨지는지
	names.emplace(a, "alpha");
	names.emplace(b, "beta");
	names.emplace(c, "gamma");
	names.remove(a);
	CHECK(names.get(c).value == "gamma");
	CHECK(names.get(b).value == "beta");
	CHECK(pool_consistent(names));

	Pool<Owned> owned;									// 이동만 되는 타입
	owned.emplace(a, std::make_unique<int>(1));
	owned.emplace(b, std::make_unique<int>(2));
	owned.emplace(c, std::make_unique<int>(3));
	owned.remove(a);
	CHECK(owned.get(c).ptr && *owned.get(c).ptr == 3);
	CHECK(owned.get(b).ptr && *owned.get(b).ptr == 2);
	CHECK(pool_consistent(owned));
}

static void test_pool_via_interface()
{
	std::printf("[Pool] IPool로 접근 (Registry::destroy가 쓰는 방식)\n");
	Pool<int> p;
	const Entity a = make(0, 0), b = make(1, 0);
	p.emplace(a, 1);
	p.emplace(b, 2);

	IPool& ip = p;
	CHECK(ip.contains(a));
	CHECK(ip.size() == 2);
	ip.remove(a);
	CHECK(!ip.contains(a) && ip.size() == 1);
	CHECK(p.get(b) == 2);
}

// ---------------------------------------------------------------- Registry: 엔티티
static void test_registry_entity()
{
	std::printf("[Registry] 엔티티 발급·삭제\n");
	Registry r;

	const Entity a = r.create();
	const Entity b = r.create();
	const Entity c = r.create();
	CHECK(index(a) == 0 && index(b) == 1 && index(c) == 2);
	CHECK(version(a) == 0 && version(b) == 0 && version(c) == 0);
	CHECK(r.valid(a) && r.valid(b) && r.valid(c));
	CHECK(!r.valid(kNull));
	CHECK(!r.valid(make(3, 0)));						// 아직 발급 안 된 칸

	r.destroy(b);
	CHECK(!r.valid(b));
	CHECK(r.valid(a) && r.valid(c));

	const Entity d = r.create();						// 1번 칸 재사용, 세대 1
	CHECK(index(d) == index(b));
	CHECK(version(d) == version(b) + 1);
	CHECK(r.valid(d));
	CHECK(!r.valid(b));									// 옛 ID는 계속 무효
}

static void test_registry_free_queue()
{
	std::printf("[Registry] 반납 목록은 큐 (먼저 비워진 칸부터)\n");
	Registry r;
	const Entity a = r.create();						// 0
	const Entity b = r.create();						// 1
	const Entity c = r.create();						// 2

	r.destroy(c);										// 2번이 먼저 반납
	r.destroy(a);										// 그다음 0번

	const Entity first = r.create();
	const Entity second = r.create();
	CHECK(index(first) == index(c));					// 먼저 비워진 2번부터
	CHECK(index(second) == index(a));
	const Entity fresh = r.create();					// 반납 목록이 비면 새 칸
	CHECK(index(fresh) == 3);
	CHECK(r.valid(b));
}

static void test_registry_many_generations()
{
	std::printf("[Registry] 같은 칸 여러 번 재사용\n");
	Registry r;
	Entity e = r.create();
	const Entity first = e;
	for (uint32_t i = 1; i <= 1000; ++i)
	{
		r.destroy(e);
		e = r.create();
		if (index(e) != index(first) || version(e) != i) { CHECK(false); break; }
	}
	CHECK(r.valid(e));
	CHECK(!r.valid(first));
}

// ---------------------------------------------------------------- type_id
// remove_cvref_t 동작은 컴파일 시간에 확인 (틀리면 빌드 실패)
static_assert(std::is_same_v<std::remove_cvref_t<const Health&>, Health>);
static_assert(std::is_same_v<std::remove_cvref_t<Health&&>, Health>);
static_assert(std::is_same_v<std::remove_cvref_t<const volatile Health>, Health>);
static_assert(std::is_same_v<std::remove_cvref_t<const Health*>, const Health*>);	// 포인터가 가리키는 쪽의 const는 그대로

// main()에서 다른 테스트보다 먼저, SecondTU보다 먼저 불러야 한다 (번호를 받는 순서를 이 파일이 먼저 정하도록)
static void test_type_id()
{
	std::printf("[TypeId]\n");

	// 이 파일에서 먼저 받는 순서: Health → Name → Owned → Dead (SecondTU는 Position → Dead → Health)
	const uint32_t health = type_id<Health>();
	const uint32_t name = type_id<Name>();
	const uint32_t owned = type_id<Owned>();
	const uint32_t dead = type_id<Dead>();

	// 같은 타입은 몇 번을 불러도 같은 번호
	CHECK(type_id<Health>() == health);
	CHECK(type_id<Health>() == type_id<Health>());

	// 다른 타입은 다른 번호 (카운터가 증가하지 않으면 전부 같아서 실패)
	CHECK(health != name && health != owned && health != dead);
	CHECK(name != owned && name != dead && owned != dead);
	CHECK(type_id<int>() != type_id<float>());
	CHECK(type_id<Health*>() != health);				// 포인터는 다른 타입

	// const·volatile·참조를 떼고 같은 번호
	CHECK(type_id<const Health>() == health);
	CHECK(type_id<Health&>() == health);
	CHECK(type_id<const Health&>() == health);
	CHECK(type_id<Health&&>() == health);
	CHECK(type_id<volatile Health>() == health);

	// 다른 번역 단위에서도 같은 번호 (카운터가 파일마다 따로면 SecondTU는 Position=0, Dead=1, Health=2부터 세서 어긋남)
	const SecondTuIds other = second_tu_type_ids();
	CHECK(other.health == health);
	CHECK(other.dead == dead);
	CHECK(other.position == type_id<Position>());
	CHECK(other.position != health && other.position != dead);

	// SecondTU에서만 쓰는 타입의 번호가 이 파일에서 받은 어떤 번호와도 겹치지 않는지 (카운터가 하나뿐인지 확인하는 핵심 검사)
	const uint32_t mine[] = { health, name, owned, dead, type_id<int>(), type_id<float>(), type_id<Health*>(), type_id<Position>() };
	for (uint32_t id : mine)
		CHECK(other.onlyHere != id);

	// 번호는 0부터 촘촘한 작은 정수 (저장소 배열 인덱스로 쓰기 때문). 지금까지 받은 타입은 Health, Name, Owned, Dead,
	// int, float, Health*, Position, OnlyInSecondTu의 9개라 모두 9 미만
	CHECK(health < 9 && name < 9 && owned < 9 && dead < 9 && other.position < 9 && other.onlyHere < 9);
}

// M2 이후 추가할 테스트 (구현되면 주석을 풀고 채운다)
// - Registry 컴포넌트: emplace/get/try_get/has/remove, destroy하면 모든 저장소에서 빠지는지
// - each<A, B>: A만 또는 B만 가진 엔티티 제외, 빈 태그 포함
// - ctx, destroy_later/flush

int main()
{
	SetConsoleOutputCP(CP_UTF8);

	test_type_id();		// 번호를 받는 순서가 중요해서 맨 먼저

	test_entity();

	test_pool_basic();
	test_pool_sparse_growth();
	test_pool_remove();
	test_pool_stale();
	test_pool_const();
	test_pool_types();
	test_pool_via_interface();

	test_registry_entity();
	test_registry_free_queue();
	test_registry_many_generations();

	std::printf("[SecondTU]\n");
	CHECK(second_tu_smoke() == 1);

	if (g_failed == 0) std::printf("ALL PASSED\n");
	else std::printf("%d FAILED\n", g_failed);
	return g_failed == 0 ? 0 : 1;
}
