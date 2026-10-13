# 공용 코드맵 (Common)

서버와 클라이언트가 같이 include하는 헤더와 공용 데이터. 이 문서는 실제 소스(2026-10-04 기준)를 보고 작성했다. 여기를 바꾸면 **서버와 클라이언트를 둘 다 다시 빌드**해야 한다(패킷 구조가 어긋나면 런타임에 깨진다).
전체 색인은 저장소 루트 `CODEMAP.md`.

---

## 1. 헤더

| 파일 | 역할 |
|---|---|
| `Packet.h` | 네트워크 프로토콜 전체. 네임스페이스 `common::packet`. 패킷 구조체는 `#pragma pack(push, 1)` 안에 있다(정렬 패딩 없음) |
| `PacketStream.h` | 패킷 직렬화 버퍼 (`<<`, `>>`, 문자열·가변 데이터) |
| `NetClock.h` | 공용 네트워크 시계 `common::NetNowMs()`(uint64 ms), `NetNowMsPrecise()`(double ms). `steady_clock` 기반, PC마다 기준점이 다르다(서버 시각 추정은 클라 `ServerClock`) |
| `MotionCurve.h` | 모션 이벤트 공용 정의 `common::motion`: `MotionType`(Knockback), `MotionCurve`(EaseOutQuad), `Progress(곡선, 버팀, 이동 시간, 경과)` → 0~1. 서버 이동과 클라 재생이 같은 함수를 씀 |
| `Vector3.h` | `common::Vec3`(= `XMFLOAT3`), `Vec4`, `Quat`(= `XMFLOAT4`), 상수(`Vec3Zero` 등), `Dot/Length/Normalize/Distance`, NaN 검사, `common::VectorHelper` 연산자(+, -, *, /) |
| `JoltHelper.h` | `PIP::Utils::ToJolt/FromJolt` (Vec3, Quat, 행렬 ↔ Jolt) |
| `TerrainData.h` | 하이트맵 지형 데이터 로드(metadata.json + raw), 정보·높이 배열 |
| `PathManager.h` | 서버·클라 공용 경로 관리(헤더 전용, Windows). `Init(AppKind)`: exe 옆에 `Deploy.json`이 있으면 배포 모드(별칭 → exe 기준 폴더), 없으면 개발 모드(위로 올라가 저장소 루트를 찾고 `PathManifest.json`의 `dev` 경로 사용). 루트 `App`, `Shader`, `Lua`, `Saved`. `Expand("UI:a.dds")` → `"Resource/UI/a.dds"`(App 밖 별칭은 절대 경로), `ResolveApp`(별칭·App 기준 경로 → 실제 경로), `Resolve(root, rel)`, `GetAlias`, `ToUtf8`. 개발 모드에서 연 파일을 `Saved/used_files.txt`에 누적 기록(종료 시). 매니페스트 오류·필수 폴더 없음은 메시지 박스 |
| `json.hpp` | nlohmann json 3.12.0 |
| `stb_image.h` | 이미지 로드 |

### 1.1 `ECS/` (직접 구현한 ECS, 작업 중)

설계·진행은 `기획 & 계획/ECS_Migration_KR.md` 4장. 네임스페이스 `ECS`, 헤더 전용, STL만 사용.

| 파일 | 역할 |
|---|---|
| `ECS/Entity.h` | `Entity`(= `uint64_t`, 하위 32비트 인덱스 + 상위 32비트 세대), `make`, `index`, `version`, `kNull` |
| `ECS/TypeId.h` | `type_id<T>()`: 컴포넌트 타입마다 0부터 번호(저장소 배열 인덱스용, 처음 불린 순서라 실행마다 다를 수 있음 → 저장·전송 금지). `std::remove_cvref_t`로 const·참조를 떼고 번호를 매김. 카운터 `detail::next_type_id()`는 이름 있는 namespace의 `inline`(익명 namespace·`static` 금지: .cpp마다 카운터가 따로 생김) |
| `ECS/Pool.h` | `IPool`(타입을 모르는 공통 인터페이스: `contains`, `remove`, `size`), `Pool<T>`: 컴포넌트 타입 하나의 저장소(sparse set: `_sparse` 인덱스 → 위치, `_dense` 위치 → 엔티티, `_data` 위치 → 컴포넌트). `emplace(e, 재료...)`는 가변 인자를 완전 전달해 자리에서 생성하고 `T&` 반환, 삭제는 swap and pop, `get`·`try_get`은 const·비 const 두 벌 |
| `ECS/Registry.h` | `Registry`: 엔티티 발급(`create`, 반납 목록 `std::deque` 큐)·삭제(세대 +1)·유효 검사(`valid`), 순회 깊이 `_iterating`·`IterGuard`. 컴포넌트 함수·저장소 목록·`each`·`ctx`는 아직 |
| `ECS/Test/ECSTest.sln` | 단독 테스트 콘솔 프로젝트(pch 없음, v143·x64·stdcpplatest·/utf-8). 게임 프로젝트와 별개. `main.cpp`: `CHECK` 매크로(Release에서도 동작), Entity·Pool·Registry 테스트와 불변식 검사 `pool_consistent`, windows.h를 ECS 헤더보다 먼저 포함, 콘솔 UTF-8. `SecondTU.cpp`: Pool.h를 맨 먼저 포함하는 두 번째 번역 단위(헤더 자립성, `type_id` 교차 확인: 다른 순서로 번호 받기 + 이 파일 전용 타입 `OnlyInSecondTu`). `TestComponents.h`: 두 .cpp가 같이 쓰는 테스트용 컴포넌트 |

알려진 문제: `Vector3.h`의 한글 주석이 CP949로 저장되어 있다(UTF-8로 읽으면 깨짐). 주석뿐이라 빌드에는 영향이 없다. 고치려면 UTF-8 BOM으로 다시 저장한다.

---

## 2. `Packet.h` 구성

### 2.1 패킷 번호 (`PacketType`, uint16)

| 범위 | 분류 | 예 |
|---|---|---|
| 11~14 | 로그인 | `C2S_P_LOGIN`, `S2C_P_LOGIN_ACK`, `S2C_P_LEAVE`, `S2C_P_SPAWN_PLAYER` |
| 91~92 | 이동 | `S2C_P_MOVE`, `C2S_P_MOVE` |
| 103~107 | 행동·피격 | `C2S_P_ACTION`(범용 행동), `S2C_P_PLAYER_ATTACK`, `S2C_P_NPC_ATTACK`, `S2C_P_PLAYER_RESURRECT`, `S2C_P_SKILL_UNLOCKED` |
| 201~209 | 방·씬 | 방 목록·입장, 준비 완료, `S2C_P_CHANGE_SCENE`, 컷씬 |
| 301~302 | 채팅 | |
| 501~506 | NPC | 스폰, 이동, 디스폰, HP, `S2C_P_NPC_MOVE_BATCH`, `S2C_P_NPC_COUNT`(씬 시작 시 NPC·보스 풀 개수) |
| 600~603 | 디버그 | `C2S_P_DEBUG_COMMAND`(F8 물리 스냅샷 등), 디버그 그리기·BT 정보·도형 |
| 701~703 | 인벤토리 | |
| 801~804 | 퀘스트·상호작용 | |
| 901 | 스탯 동기화 | |
| 1001 | 보스전 카운트다운 | |
| 1101~1102 | 시간 동기화 | `C2S_P_TIME_SYNC { double _client_time }`, `S2C_P_TIME_SYNC { double _client_time, double _server_time }` |
| 1201~1202 | 모션 이벤트 | `S2C_P_MOTION_START`(엔티티, 모션 번호, 종류, 곡선, 서버 시작 시각, 시작·끝 위치, 버팀, 이동 시간), `S2C_P_MOTION_END`(엔티티, 모션 번호, 서버 종료 시각, 위치) |

새 패킷을 추가할 때: 번호 → 구조체 → 서버 `PacketManager`(핸들러 등록 + **세션 상태별 허용 목록**) → 클라 `NetworkManager::RegisterHandler`.

### 2.2 주요 구조체

| 구조체 | 내용 |
|---|---|
| `CS_PACKET_MOVE` | 위치, 이동 입력 방향, 회전, 상태, 액션, 클라 틱 |
| `CS_PACKET_ACTION` | 액션 id, 대상 id, 방향, 위치, 클라 시각(`NetNowMs`, 리와인드 판정용) |
| `SC_PACKET_MOVE` | 플레이어 위치·속도·회전·상태·액션, 클라 틱 에코, 잡기, HP·MP, 서버 시각(`_server_time`, double, `NetNowMsPrecise`. 다른 플레이어 보간 기준) |
| `NPCMoveData` / `SC_PACKET_NPC_MOVE_BATCH` | NPC id, 회전, 위치, 속도, 서버 시각(uint32, `NetNowMs`), 상태, 액션, 잡기, HP |
| `SC_PACKET_NPC_SPAWN` | NPC 종류(`NPCType`), 위치, 회전, HP, 상태, 액션 + 이름 |
| `NPCHitInfo` / `SC_PACKET_NPC_ATTACK` | 피격 대상, 데미지, 남은 HP |

### 2.3 열거형·상수

- `EntityState`(IDLE, MOVE, RUN, JUMP, HOVER, LANDING, ACTION, SKILL_ONE, HITTED, DEAD, GRABBED), `NPCType`(Basic, Tainer, Elevator, MagicGuard, QuestNPC, Lever, DynamicBox), `ActionID::Common/Tainer/...`, `DebugCommandType`(물리 기록, 보스 씬 전환, 주변 몬스터 즉사, 스킬 즉시 해금), `DebugShapeType`
- `common::anim_speed`(수정 금지), `common::move_speed`(걷기 8, 달리기 50, 한 프레임 최대 50)

---

## 3. 데이터 폴더

| 폴더 | 내용 |
|---|---|
| `MapData/` | 하이트맵(raw, r16, png, json), 익스포트된 클라·서버 씬 데이터, 보스 스테이지 데이터 |
| `World_Batch_glTF/` | 마을 충돌용 glTF 타일(서버 `VillageCollisions`가 `Tile_X-1_Y-1.json` 사용, `.jbin` 캐시가 옆에 생김) |
