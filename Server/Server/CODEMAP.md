# 서버 코드맵 (Server/Server)

IOCP 기반 권위 서버. 방(Room)마다 Jolt 물리 시스템을 따로 돌린다. 이 문서는 실제 소스(2026-10-04 기준)를 보고 작성했다. 코드를 바꾸면 해당 항목을 같이 고친다.
전체 색인은 저장소 루트 `CODEMAP.md`, 클라이언트는 `Client/Client/CODEMAP.md`, 공용은 `Common/CODEMAP.md`.

---

## 1. 빌드·실행 기본

| 항목 | 내용 |
|---|---|
| 솔루션 | `Server/Server.sln`, 프로젝트 `Server.vcxproj` (출력 `Server/x64/<구성>/STL_Server.exe`) |
| 언어·옵션 | `/utf-8`. 콘솔은 `main.cpp`에서 `SetConsoleOutputCP(CP_UTF8)`, `wcout` 로케일 `.UTF-8` |
| 미리 컴파일 헤더 | `pch.h` (STL, Winsock, Jolt, `Singleton<T>`, `Logger`(`MYLOG/MYERROR`, 에러면 `__debugbreak`), `ObjectSnapshot`, P코어 고정 헬퍼, `Packet.h`) |
| 컴파일 스위치 (`pch.h`) | `DEBUG_VIEWER`(Jolt 물리 기록 `physics_dump.bin`), `ENABLE_DEBUG_LOG`(MYLOG 출력, Release에서는 빠짐), `_DEBUG_PHYSICS_VISUALIZATION` |
| Jolt | `Jolt/lib/Debug`, `Jolt/lib/Release` |
| 외부 | Lua 5.4(`lua-5.4.2_Win64_dll17_lib/`, `lua54.dll`), Recast/Detour(`Recast/`, `Detour/`, 내비메시), nlohmann json |
| 경로 기준 | `common::PathManager`(`Common/PathManager.h`). 기본은 개발 모드: 저장소 `PathManifest.json`의 별칭, App 루트 `Server/Server`. exe 옆에 `Deploy.json`이 있을 때만 배포 모드. 작업 폴더 무관. 서버 경로는 모두 별칭: `Lua:`, `NavMesh:`, 클라 리소스 `LandscapeMeshes:`, `BossMap:`, `MainLandscape:`, `WorldBatch:`(`server.cpp` 시작부, `LuaManager`). `MapDataManager::Load*`는 함수 안에서 `ResolveApp`. 물리 기록은 `Saved/` |
| 배포 | `python Tools/deploy.py`(대화형 메뉴) 또는 `python Tools/deploy.py --app server`: `STL_Server.exe`, `lua54.dll`, `Lua/`, `Resource/NavMesh2.obj`, `External/`(BossMap, LandscapeMeshes 2개, MainLandscape `Landscape*`, WorldBatch `Tile_X-1_Y-1`), `Deploy.json`. 약 2.5GB. `.jbin` 캐시는 복사하지 않으므로 첫 실행 때 원본 메쉬 옆에 새로 만든다(쓰기 권한 필요) |
| 시계 | `common::NetNowMs()`(steady_clock ms). 리와인드 기록·판정, NPC 시각 모두 이 시계 |

---

## 2. 스레드 모델 (`server.h/.cpp`)

| 스레드 | 하는 일 |
|---|---|
| 메인 | `main.cpp`: P코어 수 → IO 2개, 로직 = 나머지. `Server::Initialize`(DB 더미 모드, 맵 데이터 로드, Lua·스테이지 초기화) → `Start` → Enter 대기 → `Stop` |
| IO 워커 (2) | IOCP 완료 처리, 수락, 수신 → 패킷 조립 → `PacketManager::Dispatch`(**IO 스레드에서 실행**). 대부분의 핸들러는 방 잡 큐에 넣고, `TIME_SYNC`처럼 즉시 응답하는 것도 있음 |
| 로직 워커 (N) | 방 100개를 `i % N`으로 나눠 맡음. 루프: 공용 잡 큐 → 담당 방 `ProcessJobs` → 물리 고정 스텝 1/60초(누적, 최대 5회) `Room::UpdatePhysics` → `Room::UpdateLogics(dt)` → 16ms 맞춰 sleep |
| DB 워커 | `DBManager` 작업 큐 (현재 연결 문자열 `DUMMY`) |
| 타이머 | `Server::AddTimerJob(워커, 지연, 작업)` |

`SESSION`: 상태 `ST_FREE/ST_LOBBY/ST_INGAME/ST_CLOSE`, `do_recv/do_send`, 소속 방 id, `_player`.

---

## 3. 방 (`Room.h/.cpp`, 약 3000줄)

| 영역 | 내용 |
|---|---|
| 구성 | 방마다 Jolt `PhysicsSystem`, `GridMap`(셀 AOI), 플레이어·NPC·엘리베이터 목록, 현재 `Stage`, 방 상태(대기 중이면 이동·행동 무시) |
| 잡 | `PushJob`/`ProcessJobs` (네트워크 스레드 → 로직 스레드 전달) |
| 입장·씬 | `EnterPlayer/LeavePlayer`, `SetupPlayerSpawn`, `CheckAndStartGame`, `ChangeScene(스테이지 이름)`, 컷씬 완료 처리 |
| 패킷 실행 | `Execute_C2S_MOVE`(클라 위치 검증), `Execute_C2S_ACTION`(평타: 앞 1m 반지름 2m 구체, 대검 스킬, 상호작용(레버·퀘스트)), `Execute_C2S_NPC_INTERACT`, `Execute_C2S_PLAYER_READY` 등 |
| 공격 판정 | `ExecuteActorAction(공격자, AttackConfig, 리와인드 시각)`: GridMap 후보 → 대상 `ValidateHit`(과거 스냅샷 기준 히트박스 겹침) → 피격 패킷 브로드캐스트. NPC가 맞으면 `BroadcastNpcMotionEvents`로 넉백 `MOTION_START`(그 NPC를 보는 플레이어에게). 물리 갱신 뒤에도 같은 함수로 막혀서 끝난 모션의 `MOTION_END` 전송 |
| 갱신 | `UpdateLogics`: 활성 셀(플레이어 주변) NPC 수집, 플레이어 갱신·스냅샷 기록(`RecordSnapshot`), **바뀐 플레이어는 매 로직 루프마다 `SC_PACKET_MOVE` 브로드캐스트**, NPC AI·갱신, **NPC 이동은 0.05초마다 바뀐 것만 셀 단위로 `NPC_MOVE_BATCH`** |
| 물리 | `UpdatePhysics`: Jolt 스텝, 컨트롤러 갱신(NPC 경량 갱신, 보스·플레이어 전체) |
| 사망·리스폰 | `OnNPCDead`, `OnPlayerDead`, NPC 리스폰 |
| 디버그 | F8 요청 시 `physics_dump.bin`에 한 프레임 기록(0번 방만, 세션 레코더 하나 유지, 캡처마다 flush) |

---

## 4. 스테이지 (맵)

| 파일 | 역할 |
|---|---|
| `Stage.h` | 기반: `on_initialize`(물리 지형·정적 충돌체, 씬 전환 즉시), `on_enter`(플레이어 로딩 후 NPC 스폰), `update`, `on_exit`, `get_spawn_pos` |
| `StageManager.h/.cpp` | 이름 → 스테이지 팩토리 (`MainStage`, `CastleStage`, `BossStage`) |
| `MainStage` | 스폰 (-212.0, 6.43, -355.0) |
| `CastleStage` | 스폰 (-215.27, 6.59, -366.41), NPC·DynamicBox 배치 |
| `BossStage` | 스폰 (15, 0, 0), 보스 |

퀘스트 2 진행 중 부활 위치는 `PlayerData.lua`의 `quest2_spawn_point`.

---

## 5. 엔티티·컴포넌트 시스템

클라이언트와 **별개로 구현된** 컴포넌트 구조다(네임스페이스 `PIP::GAME`). 클라이언트보다 단순하다: 수명 주기 함수가 적고, 갱신은 방이 엔티티 종류별로 직접 부른다.

### 5.1 기반 클래스

| 클래스 | 파일 | 역할 |
|---|---|---|
| `GameObject` | `GameObject.h/.cpp` | 고유 id(전역 원자 카운터, NPC·플레이어는 `SetId`로 덮어씀), 이름, 컴포넌트 목록(`vector<unique_ptr<Component>>`) + 타입 캐시(`unordered_map<type_index, Component*>`). `Update/PhysicsUpdate`는 모든 컴포넌트에 그대로 전달. **`ValidateHit`(피격 검증, 데미지·넉백 값을 받음)은 순수 가상**이라 판정 대상 엔티티가 각자 구현 |
| `Component` | `Component.h/.cpp` | 소유자를 **원시 포인터**(`GameObject* _owner`)로 보관(소유자가 컴포넌트를 `unique_ptr`로 가지므로 수명이 같다). 가상 함수 `Initialize`, `Update(dt)`, `Update(dt, TempAllocator*)`, `PhysicsUpdate(dt)`, `PhysicsUpdate(dt, TempAllocator*)`. `GetComponent<T>()`로 같은 오브젝트의 다른 컴포넌트 접근 |

### 5.2 컴포넌트 추가·조회 규칙

- `AddComponent<T>(args...)`: `T(owner, args...)` 생성 → 목록 추가 → 타입 캐시 등록 → **즉시 `Initialize()` 호출**. 클라이언트와 달리 **중복 검사가 없고**(같은 타입을 두 번 붙이면 둘 다 들어가고 캐시는 마지막 것을 가리킴), 의존 컴포넌트 자동 추가(`required_components`)도 없다. 필요한 컴포넌트는 엔티티 생성자에서 순서대로 직접 붙인다.
- `GetComponent<T>()`: 정확한 타입이면 캐시에서 바로, 아니면 `dynamic_cast`로 상속 관계를 찾아 캐시에 넣는다. 예: `GetComponent<CharacterControllerComponent>()`가 `NPCControllerComponent`를 찾는다.
- 컴포넌트 제거·파괴·활성 플래그가 없다. 엔티티가 사라질 때 함께 사라진다.
- Jolt `PhysicsSystem`이 필요한 컴포넌트(캐릭터 컨트롤러, 물리 바디)는 생성 시점이 아니라 **방에 들어갈 때 방의 물리 시스템으로 초기화**한다(방마다 물리 세계가 따로 있기 때문).

### 5.3 엔티티 상속 관계와 구성

```
GameObject (ValidateHit 순수 가상)
└─ Actor          활성, HP·상태(가상), 파벌, 잡기 정보, 위치 기록(리와인드)
   ├─ Player      SESSION이 shared_ptr로 소유
   ├─ NPC         Room::_npcs(unique_ptr)가 소유
   │  ├─ MagicGuard, Tainer(보스), QuestNPC
   └─ Elevator    Room::_elevators(unique_ptr)가 소유
```

| 엔티티 | 생성자에서 붙이는 컴포넌트 (순서대로) |
|---|---|
| `Player` | Transform → PlayerController(레이어 MOVING) → Inventory → Hitbox |
| `NPC` (일반) | Transform → NPCController(레이어 NPC) → Hitbox(캡슐 r0.5·키1.8·발 위 0.9m) → AI → `SetupBT()`(행동 트리 구성) |
| `NPC` (DynamicBox) | Transform → Physics(일반 Jolt 바디) → Hitbox(박스 반크기 0.5). AI·컨트롤러 없음 |
| `Tainer` | NPC 구성 후 히트박스를 캡슐 r3·키8·발 위 4m로 교체, 공격 설정 초기화, 보스 전용 BT |
| `Elevator` | Transform → Physics |

### 5.4 `Actor` (플레이어·NPC 공통)

- **위치·속도·회전 조회**: 캐릭터 컨트롤러가 있으면 Jolt 캐릭터 위치(발바닥 보정)가 진짜 위치, 없으면 Transform. 회전은 Transform.
- **위치 기록(리와인드용)**: `RecordSnapshot(시각)`이 `{시각, 위치, 회전}`을 최근 `kMaxHistory`(60)개까지 보관, `GetSnapshotAt(시각)`이 그 시각 이후 첫 기록(없으면 마지막)을 돌려준다. `Room::UpdateLogics`가 매 로직 루프(약 16ms)마다 플레이어·NPC에 기록한다. 60개 ≈ 1초로 되감기 한도(`Player::ComputeRewindTimestamp`의 `MAX_REWIND_MS` = 1000)와 맞춘다(2026-10-04, 30개 ≈ 0.5초에서 늘림). 로직 루프 간격이나 되감기 한도를 바꾸면 같이 맞춘다.
- 파벌(`FACTION_PLAYER/MONSTER/NEUTRAL`), 활성 플래그, 리스폰 대기·사망 연출 시간, 잡기(`grabbed_by_id`, 손 슬롯).

### 5.5 갱신 흐름 (누가 언제 부르나)

엔티티 갱신은 GameObject 목록을 일괄 순회하지 않고 **방이 종류별로 직접** 부른다.

| 단계 | 호출 | 대상 |
|---|---|---|
| 물리 (`Room::UpdatePhysics`, 1/60초 고정 스텝) | 방 Jolt 월드 스텝 | 정적 지형, 일반 물리 바디 |
| | `PhysicsComponent::PhysicsUpdate` | 시야 안 DynamicBox |
| | `NPCControllerComponent::LightPhysicsUpdate` | 시야 안 일반 NPC (경량 갱신) |
| | `NPC::PhysicsUpdate` → 모든 컴포넌트 | 보스 (거리와 무관하게 정밀 갱신) |
| | `Player::PhysicsUpdate` → 모든 컴포넌트 | 플레이어 (잡힌 상태면 건너뜀) |
| | `GridMap::UpdatePosition` | 위 대상 전부 (셀 위치 갱신) |
| 로직 (`Room::UpdateLogics`) | `NPC::Update` | 활성 셀 NPC + 보스: 피격 쿨다운·`HITTED` 해제, 비전투 회복(5초마다 10%), 공격 쿨다운, `AIComponent::Update`(행동 트리 tick) |
| | 방에서 직접 | NPC 속도 방향으로 회전 설정, `RecordSnapshot` |
| | `Player::Update` | 쿨다운, MP 회복(초당 8) |
| | 방에서 직접 | 플레이어 `RecordSnapshot`, 바뀌었으면 이동 패킷 브로드캐스트 |
| 판정 (`ExecuteActorAction`) | 대상의 `ValidateHit` → `HitboxComponent::CheckCollision` | 공격 시점 |



### 5.6 엔티티별 역할

| 엔티티 | 파일 | 역할 |
|---|---|---|
| `Player` | `Player.h/.cpp` | HP/MP/공격력, 쿨다운(피격 0.5초, 대시), 퀘스트, 인벤토리. `IsDirty`(보낸 값과 비교), `CreateMovePacket`(정지·액션 중이면 속도 0, 서버 시각 `_server_time` 기록), `ComputeRewindTimestamp`(클라 시각 차이의 관측 최솟값을 기준선으로, 늦게 온 만큼 되감기, 최대 1초), `ValidateHit` |
| `NPC` | `NPC.h/.cpp` | 종류·방 id·스폰 위치·순찰 지점, 행동 트리 구성(`SetupBT`), `ValidateHit`(피격 쿨다운 0.5초, HP 감소, `HITTED`, 공격 설정의 넉백 값으로 넉백), `IsDirty`(상태·액션·잡기·위치 변화), 리스폰(`ResetForRespawn`) |
| `MagicGuard` | `MagicGuard.h/.cpp` | 내비메시 경비병. BT: 피격 → 감지·추적(`FindPath/FollowPath`) → 공격, 없으면 순찰 |
| `Tainer` | `Tainer.h/.cpp` | 보스. 페이즈 전환, 공격 설정(내려찍기, 돌진, 잡기 돌진, 포효 등), 보스 BT |
| `QuestNPC` | `QuestNPC.h/.cpp` | 퀘스트 NPC. 판정을 받지 않음(`ValidateHit`가 항상 실패) |
| `Elevator` | `Elevator.h/.cpp` | 엘리베이터 (물리 바디, 위·아래 이동) |

---

## 6. 컴포넌트 목록

| 컴포넌트 | 붙는 곳 | 역할 |
|---|---|---|
| `TransformComponent` | 모든 엔티티 | 위치·회전, 부드러운 회전(`SmoothRotateTo`), 앞·오른쪽 방향, Jolt 변환 |
| `CharacterControllerComponent` | (기반) | Jolt `CharacterVirtual` 래퍼. 캡슐 크기, 충격 속도(`AddImpact/AddImpulse`, 마찰 `ImpactFriction=35`로 감속), 지면 판정, 위치(발바닥 = 캐릭터 중심 − 반높이) |
| `NPCControllerComponent` | NPC | 이동 속도, **넉백 모션** `StartKnockback`(넉백 방향으로 구체 스윕해 벽 앞 끝 위치 확정, 버팀 0.06초 + 일정 감속 곡선, 동작 중엔 AI 이동·충격량 무시하고 서버 시각 기준 곡선 위치로 이동, 0.3m 넘게 벗어나면 중단 기록, 사망으로 물리가 꺼져도 진행 중인 모션은 끝까지 진행), `TakeStartedMotion`/`TakeInterruptedMotion`(Room이 꺼내 전송), 경량 물리 갱신(`LightPhysicsUpdate`), 수직 속도 초기화 |
| `PlayerControllerComponent` | 플레이어 | 클라 입력 방향으로 이동 속도 적용 |
| `PhysicsComponent` | DynamicBox, 엘리베이터 | 일반 Jolt 바디 생성·속도 |
| `HitboxComponent` | 플레이어, NPC | 이름 붙은 히트박스(모양, 오프셋, 회전) 목록. `CheckCollision`: 과거 스냅샷의 위치·회전(스케일 없음)으로 공격 모양과 모양 대 모양 겹침 검사 |
| `AIComponent` | NPC | 모드 `None/Lua/BT`. BT 모드면 블랙보드와 루트 노드를 들고 매 갱신 tick. Lua 모드는 스크립트의 `Update(dt)` 호출(현재 호출처 없음) |
| `InventoryComponent` | 플레이어 | 재료·장비, 저장 필요 여부 |


---

## 7. AI

| 파일 | 역할 |
|---|---|
| `BehaviorTree.h` | 블랙보드(`std::any`), 노드(Selector, Sequence, Inverter, Succeeder, Action, Condition), `BTBuilder` 체인 빌더 |
| `BT_Nodes.h/.cpp` | 조건·행동 노드 모음: 피격·생존·대상 확인, 스폰 주변 배회, 추적·공격, 페이즈·HP 조건, 보스 애니메이션·회전·포효·돌진(`Action_ChargeAttack`, `Action_GrabCharge`), 순찰·감지·길찾기(`Action_FindPath/FollowPath`) |

---

## 8. 데이터·맵·스크립트

| 파일 | 역할 |
|---|---|
| `MapDataManager.h/.cpp` | 하이트맵 지형(`LoadMainLandscapeData`), 정적 메쉬 충돌체(`LoadStaticMeshShapes`, JSON 옆에 `.jbin` 캐시 저장), 서버 익스포트 데이터, 지형 그룹(스테이지별), 내비메시(Detour, `dtNavMeshQuery`는 스레드별), 지면 높이 |
| `glTFMeshLoader` | 충돌용 glTF 메쉬 로드 |
| `LuaManager.h/.cpp` + `Lua/*.lua` | Lua 데이터: `NPC_Data.lua`(스폰), `QuestData.lua`, `LeverData.lua`, `PlayerData.lua`(초기 스탯, 퀘스트2 부활 위치). AI용 Lua 함수 등록. `Monster.lua`는 C++에서 로드하지 않음(`AIComponent::SetLuaScript` 호출처 없음, AI는 행동 트리 사용) |
| `DBManager` | DB 작업 스레드, 로그인·인벤토리 저장(현재 더미 모드) |
| `CombatDef.h` | `AttackConfig`(모양, 위치 오프셋, 데미지, 넉백 값, 쿨다운, 상태·액션, 잡기 여부) |
| `GridMap` | 셀 공간 분할, 주변 객체·셀 조회 |

---

## 9. 물리·네트워크 기반

| 파일 | 역할 |
|---|---|
| `PhysicsManager` | Jolt 팩토리·타입 등록 (프로세스 1회) |
| `JoltSetup.h` | 레이어, 브로드페이즈 필터, 활성·접촉 리스너 |
| `PacketManager.h/.cpp` | 핸들러 등록, 세션 상태별 허용 패킷 검사 후 디스패치 |
| `PacketHandlers.h/.cpp` | `Handle_C2S_*`: LOGIN, MOVE, ACTION, ENTER_ROOM, ROOM_LIST, CHAT_IN_ROOM, PLAYER_READY, DEBUG_COMMAND, NPC_INTERACT, CUTSCENE_DONE, TIME_SYNC(즉시 응답) |
| `Profiling.h/.cpp` | 구간 시간 통계 |

---

## 10. 진행 중 작업 (관련 문서)

- 넉백 모션 이벤트(넉백 값 버그는 수정됨): `기획 & 계획/NetMotionSync_Design_KR.md` S4
