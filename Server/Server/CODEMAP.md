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
| **작업 폴더** | **`Server/Server` (프로젝트 폴더)여야 한다.** 지형·씬 JSON(`../../Client/Client/Resource/...`, `../../Common/...`), Lua 데이터(`*.lua`), 내비메시(`Resource/NavMesh2.obj`) 경로가 모두 이 기준. `Server.vcxproj`에 `LocalDebuggerWorkingDirectory=$(ProjectDir)`. 개인 `.vcxproj.user`에 다른 값이 있으면 그쪽이 우선하므로 "상속"으로 되돌릴 것 |
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
| 공격 판정 | `ExecuteActorAction(공격자, AttackConfig, 리와인드 시각)`: GridMap 후보 → 대상 `ValidateHit`(과거 스냅샷 기준 히트박스 겹침) → 피격 패킷 브로드캐스트 |
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

## 5. 엔티티

| 파일 | 역할 |
|---|---|
| `GameObject.h/.cpp`, `Component.h/.cpp` | 서버 쪽 컴포넌트 구조 (`AddComponent/GetComponent`, `Update/PhysicsUpdate`) |
| `Actor.h` | 플레이어·NPC 공통: 활성, HP, 상태(`EntityState`), 파벌, 잡기 정보, **위치 기록**(`RecordSnapshot`/`GetSnapshotAt`, 최근 30개 ≈ 1초, 리와인드 판정용) |
| `Player.h/.cpp` | 플레이어. HP/MP/공격력, 쿨다운(피격 0.5초, 대시), 퀘스트, 인벤토리, `IsDirty`, `CreateMovePacket`(정지·액션 중 속도 0), `ComputeRewindTimestamp`(클라 시각 차이의 관측 최솟값을 기준선으로 늦게 온 만큼 되감기, 최대 1초), `ValidateHit` |
| `NPC.h/.cpp` | NPC. 종류별 구성(DynamicBox는 물리만), 히트박스(캡슐 r0.5·키1.8·발 위 0.9m), 행동 트리, `ValidateHit`(피격 쿨다운 0.5초, HP 감소, `HITTED`, **넉백 초속 15 고정: 공격 설정의 넉백 값이 전달되지 않는 버그**), `IsDirty`, 리스폰 |
| `MagicGuard.h/.cpp` | 길찾기 경비병 NPC (내비메시 순찰·추적 BT) |
| `Tainer.h/.cpp` | 보스. 히트박스 캡슐 r3·키8·발 위 4m, 페이즈, 공격 설정(내려찍기, 돌진, 잡기 돌진 등) |
| `QuestNPC.h/.cpp` | 퀘스트 NPC (판정 안 받음) |
| `Elevator.h/.cpp` | 엘리베이터 |

---

## 6. 컴포넌트

| 파일 | 역할 |
|---|---|
| `TransformComponent` | 위치·회전, 부드러운 회전 |
| `CharacterControllerComponent` | Jolt `CharacterVirtual` 래퍼. 충격 속도(`AddImpact`, 마찰 `ImpactFriction=35`), 지면 판정 |
| `NPCControllerComponent` | NPC 이동, `AddKnockback`(넉백 방향으로 스윕해서 벽 앞에서 멈추게 초기 속도 계산), 경량 물리 갱신, 위치는 발바닥 기준 |
| `PlayerControllerComponent` | 플레이어 이동 속도 적용 |
| `PhysicsComponent` | 일반 Jolt 바디 (DynamicBox) |
| `HitboxComponent` | 이름 붙은 히트박스 목록, `CheckCollision`(과거 스냅샷의 위치·회전으로 모양 대 모양 검사, 스케일 없음) |
| `AIComponent` | Lua 스크립트 또는 행동 트리 실행, 블랙보드 |
| `InventoryComponent` | 재료·장비 |

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
| `LuaManager.h/.cpp` + `*.lua` | Lua 데이터: `NPC_Data.lua`(스폰), `QuestData.lua`, `LeverData.lua`, `PlayerData.lua`(초기 스탯, 퀘스트2 부활 위치). AI용 Lua 함수 등록. `Monster.lua`는 C++에서 로드하지 않음(`AIComponent::SetLuaScript` 호출처 없음, AI는 행동 트리 사용) |
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

- 넉백 값 버그 수정과 넉백 모션 이벤트: `기획 & 계획/NetMotionSync_Design_KR.md` S4
- 서버 리소스 경로를 exe 기준 PathManager로 바꾸는 건은 배포 폴더 구성부터 정하기로 함(미정)
