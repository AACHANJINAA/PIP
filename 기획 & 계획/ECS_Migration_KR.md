# 클라이언트·서버 ECS 전환 계획

작성일: 2026-10-13
관련 문서: `NetMotionSync_Design_KR.md`(보간·시계), `HitFeel_Plan_KR.md`(히트스톱·타격 피드백), `MeleeHitPrediction_Spec_KR.md`(예측 판정)

---

## 1. 목표

- 게임 로직을 "컴포넌트 = 데이터, 시스템 = 로직" 구조(ECS)로 옮긴다.
- 실행 순서를 컴포넌트 추가 순서가 아니라 **시스템 호출 순서로 명시**한다.
- 넷코드(보간, 되감기 판정, 이후 롤백)가 다룰 상태를 컴포넌트로 모아, 저장·복원 경계를 분명하게 한다.
- 한 번에 갈아엎지 않는다. **기존 GameObject와 ECS 레지스트리를 나란히 두고 기능 단위로 옮긴다.** 각 단계가 끝날 때마다 빌드·실행되는 상태를 유지한다.

범위 밖(이 문서에서는 다루지 않음): 렌더러 재작성, 결정론적 시뮬레이션, 서버 스레드 모델 변경.

---

## 2. 참고 사례

| 사례 | 내용 | PIP에 주는 시사점 |
|---|---|---|
| 오버워치 (GDC 2017, Tim Ford "Overwatch Gameplay Architecture and Netcode") | 컴포넌트는 데이터만, 시스템은 상태 없는 함수. 싱글톤 컴포넌트, 부작용은 지연 처리. 이 구조 덕분에 예측·롤백이 가능했다고 설명 | 넷코드가 다루는 상태(위치, 시간, 판정)부터 컴포넌트로 옮긴다 |
| EnTT (skypjack/entt) | 헤더 하나짜리 C++ ECS. 희소 집합(sparse set) 저장소, 세대 번호가 붙은 엔티티 ID, `registry.view<A, B>()` 순회 | 라이브러리는 쓰지 않고 같은 구조를 직접 구현한다(4.1). 인터페이스 이름은 비슷하게 맞춰 나중에 교체할 여지를 둔다 |
| 유니티 DOTS 전환 사례 | GameObject와 Entity를 한동안 같이 쓰는 하이브리드 기간을 둠 | 다리(bridge) 구조로 점진 전환 |

---

## 3. 현재 구조 (클라이언트)

| 항목 | 현재 |
|---|---|
| 오브젝트 | `GameObject`(컴포넌트 `vector<shared_ptr<Component>>`), `ObjectManager`가 목록·생성·지연 파괴 |
| 갱신 | `GameFramework::update_game_logic`: 모든 활성 GameObject의 `update` → 모든 `late_update`. 오브젝트 안에서는 **컴포넌트 추가 순서 = 갱신 순서** (`CODEMAP` 3.2) |
| 컴포넌트 조회 | `get_component<T>`가 매번 `dynamic_pointer_cast` 선형 탐색. 호출이 많은 것: `RenderComponent` 44곳, `AnimationComponent` 41곳 |
| 오브젝트 시간 | `GameObject::hit_stop`, `set_time_scale`, `consume_delta`가 GameObject 멤버. update에서 계산해 late_update가 재사용 |
| 보간 | `NPCScript`와 `OtherPlayerScript`가 같은 보간 코드(`SnapshotBuffer::sample(render_time())` → Transform, 대기 상태 적용, 디버그 유령)를 각자 가짐 |
| 판정·피드백 | `PhysicsColliderComponent::run_hit_query` → `GameObject::on_trigger_enter`가 모든 스크립트에 전달 → `MainPlayerScript`가 스파크·사운드·히트스톱·카메라 킥 처리 |

서버(`Server/Server`)는 클라와 별개인 더 단순한 컴포넌트 구조이고, 판정은 `GameObject::ValidateHit` 가상 함수(5개 엔티티가 각자 구현)로 되어 있다. 서버 전환은 7장.

---

## 4. 구조 원칙

| 규칙 | 내용 |
|---|---|
| 라이브러리 | 쓰지 않음. 레지스트리를 직접 구현한다(4.1) |
| 레지스트리 | 클라이언트 전체에 하나. `EcsWorld::instance()->registry()` |
| 다리 | 모든 GameObject는 생성 시 엔티티 하나를 받는다(`GameObject::entity()`). 엔티티에는 `GameObjectRef{GameObject*}`가 붙는다. GameObject가 목록에서 빠질 때(`ObjectManager::remove_game_object_from_list`) 엔티티도 지운다 |
| 컴포넌트 | 로직 없는 struct. 파일 이름은 `*Components.h`. 멤버 함수는 값 계산용 인라인 함수 정도만 |
| 시스템 | 상태 없는 함수 묶음(`namespace XxxSystem { void update(Registry&, ...); }`). 파일 이름은 `*System.h/.cpp`. 기존 `ReplicationSystem` 클래스와 이름이 겹치지 않게 한다 |
| 실행 순서 | 시스템은 `GameFramework::FrameAdvance` / `update_game_logic`에서 정해진 자리에서만 부른다. 자리를 정하거나 바꾸면 `Client/Client/CODEMAP.md` 2장(프레임 흐름)을 고친다 |
| 기존 API | 옮긴 기능의 기존 GameObject 함수(예: `hit_stop`)는 당분간 레지스트리로 전달하는 얇은 함수로 남긴다. 호출처를 다 옮긴 뒤 지운다 |
| 수명 | GameObject가 이미 목록에서 빠져 엔티티가 없을 수 있다. 전달 함수는 `registry.valid(entity)`를 확인하고, 없으면 기본값을 돌려준다 |

### 4.1 레지스트리 설계 (직접 구현)

엔티티와 컴포넌트를 저장하고 찾는 창고. 인터페이스 이름은 EnTT와 비슷하게 맞춘다.

**엔티티 ID**

| 항목 | 내용 |
|---|---|
| 형식 | `using Entity = uint64_t;`. 하위 32비트 인덱스, 상위 32비트 세대(version). 비트 나누기는 `Entity.h`의 상수 하나로만 정의해 나중에 바꾸기 쉽게 한다 |
| 생성 | 반납된 인덱스가 있으면 재사용하고 그 인덱스의 현재 세대를 붙임(`_versions`는 읽기만 함). 없으면 새 인덱스(`_versions`에 0 추가, 세대 0) |
| 반납 목록 | `_free`는 `std::deque<uint32_t>` 큐: 삭제한 인덱스를 뒤에 넣고(`push_back`) 생성할 때 앞에서 꺼냄(`pop_front`). 가장 먼저 비워진 칸부터 재사용해서 세대 증가가 여러 칸에 고르게 퍼진다 |
| 삭제 | 그 인덱스의 세대를 1 올리고 반납 목록에 넣음. 모든 저장소에서 컴포넌트 제거 |
| 유효 검사 | `valid(e)`: 인덱스 범위 안이고 ID의 세대가 현재 세대와 같은지. 지워진 뒤 재사용된 인덱스를 옛 ID로 접근하는 것을 막는다 |
| 없음 값 | `kNull` = 모든 비트 1 |
| 네트워크 ID와 구분 | 엔티티 ID는 레지스트리 안에서만 의미가 있다. 클라와 서버의 레지스트리는 따로라 같은 몬스터라도 엔티티 번호가 다르다. 패킷에는 엔티티 ID를 보내지 않고, 지금의 NPC id(`int64_t`)·플레이어 id를 `NetId{int64_t}` 컴포넌트로 들고 있는다. GameObject `unique_id`와도 다른 값이다 |

**왜 64비트인가 (2026-10-13 결정)**

| 항목 | 32비트 (인덱스 20 + 세대 12, EnTT 기본값) | 64비트 (인덱스 32 + 세대 32, Bevy·Flecs) |
|---|---|---|
| 동시 엔티티 수 | 약 104만 | 약 43억 |
| 같은 칸 재사용 후 세대가 한 바퀴 도는 횟수 | 4096번 | 약 43억 번 (사실상 안 돎) |
| 추가 메모리 | — | dense 배열의 엔티티 칸이 4 → 8바이트 (엔티티 1만 × 타입 10이면 약 400KB). 컴포넌트 `data` 배열은 그대로라 순회 성능 차이는 거의 없음 |

세대가 한 바퀴 돌면, 오래 들고 있던 옛 ID(예: AI의 어그로 대상, 락온 대상)가 같은 칸을 재사용한 전혀 다른 엔티티와 같은 값이 되어 `valid`가 true가 된다. 클라는 씬 전환마다 비워지지만 서버는 며칠씩 켜져 있고, 타격마다 생겼다 사라지는 엔티티(`PendingDamage`, `HitEvent` 등)를 쓰면 특정 칸이 몇 시간 만에 4096번 재사용될 수 있다. 드물지만 재현이 거의 불가능한 버그라서, 비용이 작은 64비트로 구조적으로 막는다.

**저장소 (컴포넌트 타입마다 하나, sparse set)**

```cpp
template<class T>
struct Pool : IPool {
    std::vector<uint32_t> sparse;  // 엔티티 인덱스 → dense 위치 (없으면 kInvalid). 위치는 32비트로 충분
    std::vector<Entity>   dense;   // 위치 → 엔티티
    std::vector<T>        data;    // 위치 → 컴포넌트 (연속 메모리)
};
```

| 연산 | 방법 | 비용 |
|---|---|---|
| 추가 | `dense`·`data` 끝에 넣고 `sparse[인덱스]`에 위치 기록 | O(1) |
| 조회 | `sparse[인덱스]`로 위치 → `dense[위치]`의 세대까지 같은지 확인 | O(1) |
| 삭제 | 마지막 원소를 지울 자리로 옮기고(swap and pop) 옮긴 원소의 `sparse` 갱신 | O(1) |
| 순회 | `data`를 처음부터 끝까지 | 연속 메모리 |

`IPool`은 타입을 모르는 공통 인터페이스(`remove(Entity)`, `contains(Entity)`, `size()`). 엔티티 삭제 때 모든 저장소에 `remove`를 부르기 위해 필요하다. 저장소 목록은 타입별 번호(`TypeId.h`의 `type_id<T>()`, 정적 카운터로 타입마다 한 번 발급)를 인덱스로 하는 `vector<unique_ptr<IPool>>`.

**레지스트리 인터페이스**

| 함수 | 하는 일 |
|---|---|
| `create()` / `destroy(e)` / `valid(e)` | 위 엔티티 ID 규칙 |
| `emplace<T>(e, args...)` | 컴포넌트 추가 (이미 있으면 assert) |
| `get<T>(e)` / `try_get<T>(e)` | 조회 (`try_get`은 없으면 nullptr) |
| `has<T>(e)` / `remove<T>(e)` | 소유 확인, 제거 |
| `each<A, B...>(fn)` | A·B…를 모두 가진 엔티티마다 `fn(e, A&, B&...)`. 가장 작은 저장소를 돌며 나머지 소유를 확인 |
| `ctx<T>()` | 레지스트리에 하나만 있는 데이터(싱글톤 컴포넌트, 예: 프레임 시계) |

**순회 중 변경 규칙**

- `each` 도중 지금 순회하는 저장소의 원소를 지우면 swap and pop 때문에 원소를 건너뛸 수 있다. 첫 버전은 **순회 중 삭제·추가를 금지**(디버그에서 assert)하고, 지울 것은 목록에 모았다가 순회 뒤에 지운다. 오버워치의 지연 처리와 같은 방식이다.
- 필요해지면 뒤에서부터 순회하는 방식(뒤에서 지우면 앞쪽 원소 위치가 안 바뀜)으로 허용을 검토한다.

**확인할 것 (단위 테스트 또는 디버그 검사)**

- 삭제 후 같은 인덱스를 재사용했을 때 옛 ID의 `valid`가 false인지.
- 중간 원소 삭제 뒤 옮겨진 원소를 `get`으로 찾을 수 있는지.
- `each<A, B>`가 A만 있거나 B만 있는 엔티티를 빼는지.

### 4.2 결정 사항 (2026-10-13)

| 항목 | 결정 | 이유 |
|---|---|---|
| 파일 위치 | `Common/ECS/` (폴더 이름은 namespace `ECS`와 같게) | 서버(S0)도 같은 코드를 쓴다 |
| 파일 구성 | `Entity.h`, `TypeId.h`, `Pool.h`, `Registry.h`. **헤더 전용**(.cpp 없음, 템플릿이 아닌 함수는 `inline`) | Common의 .cpp는 클라(`stdafx.h`)·서버(`pch.h`) 중 어느 pch를 첫 줄에 둘지 정할 수 없다. 기존 Common 파일도 모두 헤더 전용이다 |
| 테스트 | 별도 솔루션 `Common/ECS/Test/ECSTest.sln` (콘솔, pch 없음, v143·x64·stdcpplatest·/utf-8로 클라·서버와 같은 설정). 검사는 Release에서도 동작하는 `CHECK` 매크로 | 게임을 띄우지 않고 빠르게 확인. pch가 없어서 헤더가 스스로 필요한 include를 갖는지도 드러난다. 클라·서버 프로젝트에는 테스트 파일을 넣지 않는다 |
| 오류 처리 | assert (예외 안 씀) | 클라 `CERROR`·서버 `MYERROR`가 디버그 브레이크를 거는 방식과 같게 |
| 스레드 | 단일 스레드 전제 | 클라는 메인 스레드, 서버는 방마다 로직 스레드 하나 |
| 게임 코드 의존 | 없음 (STL만 사용) | `Common`에 두고 클라·서버·테스트 프로젝트가 같이 포함 |

헤더 전용에서 주의할 점:

- `type_id<T>()`의 카운터 함수는 반드시 **이름 있는 namespace(`detail`) 안의 `inline`**으로 둔다. `static` 함수나 익명 namespace(`namespace { }`)는 내부 연결이라 .cpp마다 카운터가 따로 생기고, 서로 다른 타입이 같은 번호를 받아 다른 타입의 저장소를 꺼내게 된다. `inline` 함수 안의 static 변수는 프로그램 전체에서 하나다.
- 이 버그는 테스트로 잡기 까다롭다. 두 .cpp가 같은 타입(예: `Health`)의 번호만 비교하면, 링커가 같은 이름의 `type_id<Health>`를 하나로 합쳐(COMDAT) 우연히 맞는다(2026-10-14 MSVC에서 확인, ODR 위반이라 결과는 링커에 따라 다름). 그래서 테스트는 **한 .cpp에서만 쓰는 타입**(그 파일의 익명 namespace 안 구조체)의 번호가 다른 .cpp에서 받은 번호와 겹치지 않는지로 확인한다(`SecondTU.cpp`의 `OnlyInSecondTu`).
- `type_id<T>()`는 번호를 매기기 전에 `std::remove_cvref_t<T>`로 const·volatile·참조를 뗀다. `get<const Health>`처럼 불러도 `Health`와 같은 저장소를 쓰게 하기 위함. 포인터(`Health*`)는 다른 타입으로 남는다.
- 템플릿 멤버 함수는 실제로 호출된 것만 컴파일된다. 테스트에서 모든 함수(const·비 const 오버로드 포함)를 한 번씩 부르고, 생성자 없는 struct·빈 태그 struct·이동만 되는 타입으로 각각 확인한다.
- 붙일 때 마지막 확인: 클라 .cpp 하나와 서버 .cpp 하나에서 `#include "ECS/Registry.h"`를 하고 둘 다 빌드한다(windows.h·매크로 환경이 테스트 프로젝트와 다름. 클라·서버 둘 다 `NOMINMAX` 정의는 확인함).

### 4.3 구현 순서

부품 (아래에서 위로 쌓는다):

| # | 부품 | 파일 | 하는 일 | 핵심 함수 |
|---|---|---|---|---|
| 1 | Entity | `Entity.h` | 64비트 ID = 인덱스 32비트 + 세대 32비트 | `make(index, version)`, `index(e)`, `version(e)`, `kNull` |
| 2 | 타입 번호 | `TypeId.h` | 컴포넌트 타입마다 0, 1, 2… 번호 발급 (저장소 배열의 인덱스). const·참조는 떼고 번호를 매김 | `type_id<T>()` (`detail::next_type_id` 정적 카운터) |
| 3 | IPool / Pool\<T\> | `Pool.h` | sparse set 본체 | `emplace`, `get`, `try_get`, `contains`, `remove`(swap and pop), `size`, 순회용 `entities()`·`data()` |
| 4 | Registry: 엔티티 | `Registry.h` | 발급·반납·검사 | `create`, `destroy`(모든 저장소에서 제거 + 세대 증가), `valid` |
| 5 | Registry: 컴포넌트 | `Registry.h` | 타입에 맞는 저장소를 찾아 위임 | `emplace<T>`, `get<T>`, `try_get<T>`, `has<T>`, `remove<T>` |
| 6 | each | `Registry.h` | 조합 순회. 가장 작은 저장소를 돌며 나머지 소유 확인 | `each<A, B...>(fn)` |
| 7 | ctx | `Registry.h` | 레지스트리에 하나뿐인 데이터 | `ctx<T>()`, `ctx_emplace<T>(...)` |
| 8 | 안전장치 | `Registry.h` | 순회 중 추가·삭제 감지(디버그 assert), 지연 삭제 | `destroy_later(e)`, `flush()` |

전체 300줄 안팎을 예상한다.

단계 (각 단계 끝에 `Common/ECS/Test/ECSTest.sln`으로 확인):

| 단계 | 만들 것 | 끝 기준 | 상태 |
|---|---|---|---|
| M1 | 1 Entity, 3 Pool\<T\> | 추가 후 조회 / 가운데 원소 삭제 뒤 옮겨진 원소를 조회로 찾음 / 없는 엔티티의 `try_get`이 nullptr | **완료** (2026-10-13) |
| M2 | 2 타입 번호, 4·5 Registry | 지운 엔티티의 인덱스가 재사용될 때 옛 ID의 `valid`가 false / `destroy`하면 모든 저장소에서 빠짐 | 진행 중: 엔티티 발급·삭제·유효 검사, 타입 번호(2026-10-14) 완료. 저장소 목록·`pool<T>()`, 컴포넌트 함수, `destroy`의 저장소 제거 남음 |
| M3 | 6 each | `each<A, B>`가 A만 또는 B만 가진 엔티티를 제외 / 빈 구조체(태그) 동작 | 예정 |
| M4 | 7 ctx, 8 안전장치 | 순회 중 `emplace`·`destroy` 시 디버그 assert / `destroy_later` 후 `flush`하면 실제로 삭제 | 예정 |
| M5 (선택) | 성능 비교 | 엔티티 1만 개 기준 `each` 순회와 기존 `get_component` 순회 시간 비교 (포트폴리오 수치) | 예정 |

M1~M4가 끝나면 E0(GameObject 다리)로 넘어간다.

처음에는 만들지 않는 것 (필요해지면 추가): 생성·삭제 콜백(시그널), 저장소 정렬, 그룹, 상태 저장·복원(롤백용 스냅샷), sparse 배열 페이지 분할, 멀티스레드 순회.

---

## 5. 단계

| 단계 | 내용 | 바뀌는 파일 | 상태 |
|---|---|---|---|
| **E0** | 레지스트리 직접 구현(4.1~4.3, M1~M4), `EcsWorld`, GameObject ↔ 엔티티 다리 | `Common/ECS/*`(새), `Common/ECS/Test`(새), `EcsWorld.h`(새), `GameObject`, `ObjectManager`, `Client.vcxproj(.filters)` | 진행 중 (레지스트리 M1 완료, M2 진행 중) |
| **E1** | 오브젝트 시간(히트스톱·시간 배율)을 `ObjectTime` 컴포넌트 + `TimeSystem`으로 | `TimeComponents.h`, `TimeSystem.h/.cpp`, `GameObject`, `GameFramework::update_game_logic` | 예정 |
| E2 | 네트워크 보간: `NetInterp`(버퍼, 보간 지연) + `PendingNetStates` + `NetInterpolationSystem`. `NPCScript`·`OtherPlayerScript`의 중복 제거. 액션 패킷의 되감기 시각을 이 시스템의 렌더 시각에서 가져옴 | `NPCScript`, `OtherPlayerScript`, `ReplicationSystem`, `NetworkManager` | 예정 |
| E3 | 판정 이벤트: `run_hit_query`가 `HitEvent`를 만들고, `HitFeedbackSystem`이 스파크·사운드·히트스톱·카메라 킥을 한곳에서 처리 | `PhysicsColliderComponent`, `MainPlayerScript`, `GameObject::on_trigger_enter` | 예정 |
| E4 | 렌더 목록: `build_render_list`를 `view<RenderMesh, WorldTransform>` 순회로 | `Renderer`, `RenderComponent` 계열 | 예정 |
| E5 | 애니메이션·소켓·피격 리액션 | `AnimationComponent`, `SocketComponent`, `HitReactionComponent` | 예정 |
| E6 | Transform·계층 | `TransformComponent` 및 모든 사용처 | 예정 |
| E7 | 스크립트 정리: 남은 `MainPlayerScript` 등을 입력·예측·스킬 시스템으로 분리 | 스크립트 전반 | 예정 |

순서 근거: 의존하는 곳이 적고 넷코드와 가까운 것부터. Transform·애니메이션·렌더는 호출처가 많아 뒤로 미룬다.

---

## 6. E0·E1 상세

### 6.1 E0: 다리

- `Common/ECS/`: 4.1~4.3의 엔티티 ID·저장소·레지스트리. 게임 코드에 의존하지 않는다(4.2). 클라 프로젝트 포함 경로에 `Common`이 이미 있어서(`Client.vcxproj`의 `IncludePath`) `#include "ECS/Registry.h"`로 쓴다. 클라 `Client.vcxproj(.filters)`에는 보기용으로 `..\..\Common\ECS\*.h`가 필터 `0. Engine\0. Core\3. ECS`에 등록돼 있다(2026-10-13).
- `EcsWorld`(`Singleton<EcsWorld>`)가 `Registry`를 가진다.
- `GameObject.h`에는 레지스트리 헤더를 넣지 않고 엔티티를 `uint64_t`로 들고 있는다(GameObject.h를 포함하는 파일이 많아서 컴파일 의존을 줄이기 위함).
- `GameObject::init()`: Transform 추가와 함께 엔티티를 만들고 `GameObjectRef{this}`를 붙인다.
- `ObjectManager::remove_game_object_from_list`: 컴포넌트 `on_destroy`, 계층 해제 뒤 엔티티를 지운다. 씬 전환(`clear_non_persistent_objects`)도 같은 경로로 지워진다. persistent 오브젝트는 지워지지 않으므로 엔티티도 남는다.
- GameObject는 `ObjectManager::create_game_object`에서만 만든다(2026-10-13 확인). 다른 곳에서 만들면 엔티티가 없다.

### 6.2 E1: 오브젝트 시간

컴포넌트:

```cpp
struct ObjectTime {
    float scale = 1.0f;          // 시간 배율
    float hitStopRemain = 0.0f;  // 남은 히트스톱 (실제 시간으로 줄어듦)
    float delta = 0.0f;          // 이번 프레임 오브젝트 시간 (update·late_update 공용)
    uint64_t frame = 0;          // delta를 계산한 프레임 번호
};
```

시스템 `TimeSystem`:

| 함수 | 하는 일 |
|---|---|
| `begin_frame(registry, frameDelta)` | 프레임 번호를 올리고, 활성이고 파괴되지 않은 GameObject의 `ObjectTime.delta`를 계산 (히트스톱 중 0, 풀리는 프레임은 남은 시간만큼, 그다음 배율) |
| `delta_of(registry, entity)` | 이번 프레임 delta. 이번 프레임에 아직 계산되지 않았으면(프레임 중간에 생성·활성화) 그 자리에서 계산 |

호출 위치: `update_game_logic`의 `process_new_game_objects` 직후, update 순회 전.

기존 동작과의 비교:

| 항목 | 기존 | E1 이후 |
|---|---|---|
| delta 계산 시점 | 각 오브젝트 update 안 (오브젝트마다 순회 중 다른 시점) | 프레임 시작에 전부 한 번 |
| 같은 프레임 late_update에서 건 히트스톱 | 아직 update 전인 오브젝트는 이번 프레임부터, 이미 지난 오브젝트는 다음 프레임부터 멈춤(순회 순서에 따라 다름) | 모두 다음 프레임부터 멈춤 |
| 비활성 오브젝트 | 히트스톱 시간이 줄지 않음 | 같음 (`begin_frame`이 비활성은 건너뜀) |
| fixed_update | 배율 미적용 | 같음 |

히트스톱 호출처는 `MainPlayerScript`의 적중 처리 한 곳이며(공격자·피격자 둘 다), late_update 안의 `on_trigger_enter`에서 불린다. 따라서 실제로는 기존에도 대부분 다음 프레임부터 멈췄다.

### 6.3 확인 방법

- Debug·Release 빌드.
- 평타·대검 스킬 적중 시 공격자와 맞은 NPC가 0.05초(스킬은 설정값) 멈췄다가 이어서 움직이는지.
- 씬 전환(메인 → 보스) 후 오류 없이 히트스톱이 동작하는지(엔티티 정리 확인).

---

## 7. 서버 전환 (클라 E2 이후)

| 단계 | 내용 |
|---|---|
| S0 | 클라와 같은 `Common/ECS`를 서버에서도 사용, 방(`Room`)마다 레지스트리 하나 (방마다 물리 세계가 따로인 것과 같은 구조) |
| S1 | 스냅샷: `History` 컴포넌트 + `SnapshotSystem`. 플레이어·NPC를 같은 틱 시각으로 기록 (지금은 둘이 다른 `currentTick`) |
| S2 | 판정·적용 분리: `ValidateHit` 5개 구현을 `CombatSystem`(판정만, `PendingDamage` 기록) + `DamageSystem`(HP·넉백·AI 타겟·잡기·페이즈 전환)으로. QuestNPC·Elevator는 `Invulnerable` 태그 |
| S3 | 되감기 기준: 클라가 보던 서버 시각(E2의 렌더 시각)으로 되감고 상한을 250ms 전후로 |

---

## 8. 열린 문제

- 같은 오브젝트에 같은 타입 컴포넌트가 하나뿐이라는 기존 규칙과 ECS(엔티티당 타입별 하나)는 같다. 자식 오브젝트로 콜라이더를 늘리는 기존 방식은 그대로 쓴다.
- 서버와 클라가 같은 컴포넌트 정의(예: `History`, 판정 모양)를 쓸지, `Common`에 둘지는 S1에서 정한다.

---

## 9. 진행 기록

### 2026-10-13

| 항목 | 내용 |
|---|---|
| 방향 결정 | EnTT 대신 레지스트리 직접 구현(4.1). 엔티티 ID 64비트(인덱스 32 + 세대 32). 반납 목록은 `std::deque` 큐. 위치 `Common/ECS/`, 헤더 전용, 네임스페이스 `ECS`(4.2) |
| 구현 | `Common/ECS/Entity.h`(ID 만들기·나누기, `kNull`), `Pool.h`(`IPool`, `Pool<T>` sparse set: `contains`, `emplace`(가변 인자 완전 전달, `T&` 반환), `remove`(swap and pop), `get`·`try_get` const·비 const, `size`, `entities`, `data`), `Registry.h`(`create`, `destroy`, `valid`, `_iterating`·`IterGuard`) |
| 정리 | 클라에 있던 파일을 `Common/ECS`로 옮기고 `Registry.cpp`를 헤더로 합침. `Client.vcxproj(.filters)`는 `..\..\Common\ECS\*.h`를 필터 `0. Engine\0. Core\3. ECS`에 보기용으로 등록 |
| 테스트 | `Common/ECS/Test/ECSTest.sln`: `main.cpp`(테스트 11개 + 불변식 검사 `pool_consistent`), `SecondTU.cpp`(Pool.h를 맨 먼저 포함하는 두 번째 번역 단위). windows.h를 ECS 헤더보다 먼저 포함해 클라·서버와 같은 매크로 환경에서도 확인, 콘솔은 `SetConsoleOutputCP(CP_UTF8)`. Debug·Release 모두 `ALL PASSED` |
| 테스트로 잡은 것 | `_sparse`를 한 칸씩만 늘림(순서 없는 인덱스에서 범위 밖), `get`이 복사본 반환, const `try_get`의 반환형, `emplace`의 `void` 반환, `Pool.h`의 include 누락(두 번째 번역 단위에서 발견), `size_t` → `uint32_t` 축소 경고 |

### 2026-10-14

| 항목 | 내용 |
|---|---|
| 구현 | `Common/ECS/TypeId.h`: `detail::next_type_id()`(`inline`, `cnt++`), `detail::type_id_impl<T>()`(T마다 static 번호), `type_id<T>()`(`std::remove_cvref_t`로 const·참조 제거). Registry 밖의 독립 함수(번호는 레지스트리가 아니라 프로그램 전체 기준) |
| 처음 버전의 문제 (고침) | 카운터가 증가하지 않음(`return cnt;`), 익명 namespace 사용(.cpp마다 카운터가 따로 생김), 파일을 테스트 폴더(`Test/TypeId.h`)에 만듦 → `Common/ECS/TypeId.h`로 이동 |
| 테스트 | `test_type_id`(검사 17개, `main()`에서 맨 처음 실행): 같은 타입 같은 번호, 다른 타입 다른 번호, cvref 제거, 포인터는 다른 타입, `SecondTU.cpp`에서 다른 순서로 받은 번호 일치, SecondTU 전용 타입 번호가 겹치지 않음, 번호가 0부터 촘촘함. `static_assert` 4개로 `remove_cvref_t` 확인. 테스트용 컴포넌트는 `Test/TestComponents.h`로 분리(두 .cpp가 같은 타입을 써야 해서). Debug·Release `ALL PASSED` |
| 테스트 검증 (뮤테이션) | 임시 복사본에 버그를 하나씩 넣어 테스트가 실패하는지 확인: `cnt++` → `cnt` 13개 실패, `detail` → 익명 namespace 1개 실패, `inline` → `static` 1개 실패. 처음 테스트(공용 타입 번호 비교만)는 익명 namespace 버그를 놓쳤음 → 전용 타입 검사 추가로 해결 |
| 남은 한계 | 확인은 MSVC에서만. "번호 9 미만" 검사는 `test_type_id`가 맨 처음 실행된다는 전제에 의존 |

### 다음 할 일

1. **M2 나머지**
    1. 저장소 목록 `std::vector<std::unique_ptr<IPool>>`과 `pool<T>()`(`type_id<T>()` 칸이 없으면 늘리고, 비어 있으면 만들어 반환). 다른 레지스트리가 안 쓰는 번호는 `nullptr` 칸으로 남는다.
    2. 컴포넌트 함수 `emplace<T>`, `get<T>`, `try_get<T>`(무효 엔티티면 nullptr), `has<T>`, `remove<T>`. 모두 `valid` 확인 후 저장소에 위임.
    3. `destroy`의 TODO: 모든 저장소에서(`nullptr` 칸은 건너뜀) `contains`면 `remove`. 테스트: 컴포넌트 두 종류를 붙인 엔티티를 지우면 두 저장소에서 다 빠지는지.
2. **M3** `each<A>`, `each<A, B>`(작은 저장소를 돌며 나머지 확인, `IterGuard`로 `_iterating` 관리). 가변 인자 일반화는 나중.
3. **M4** `ctx<T>`·`ctx_emplace<T>`, `destroy_later`·`flush`, `emplace`·`remove`에도 `assert(_iterating == 0)`.
4. (선택) **M5** 엔티티 1만 개 순회 성능 비교.
5. M1~M4가 끝나면 **E0**(6.1): `EcsWorld`, GameObject ↔ 엔티티 다리. 붙이기 전에 클라 .cpp 하나·서버 .cpp 하나에서 `#include "ECS/Registry.h"` 빌드 확인.
