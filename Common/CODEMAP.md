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
| `PathManager.h` | 서버·클라 공용 경로 관리(헤더 전용, Windows). `Init(AppKind)`가 exe 위치로 배포/개발을 판별(배포: 클라 `Resource/`+`Shaders/`, 서버 `Lua/`가 exe 옆에 있음. 개발: 위로 올라가며 `Common/Packet.h`가 있는 저장소 루트 탐색)하고 루트(`App`, `Shader`, `Lua`, `ClientResource`, `CommonData`, `Saved`)를 정한 뒤 cwd를 App으로 맞춤. `Resolve(root, rel)`, `ResolveApp("Resource/...")`, `ToUtf8`. 필수 폴더가 없으면 메시지 박스 |
| `json.hpp` | nlohmann json 3.12.0 |
| `stb_image.h` | 이미지 로드 |

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
