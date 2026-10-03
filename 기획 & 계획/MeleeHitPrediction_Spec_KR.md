# 평타 예측 판정 구현 명세서

작성일: 2026-10-04
관련 문서: `HitFeel_Plan_KR.md` 4장 2단계, `BoneCollider_Spec_KR.md` (1단계 칼날 캡슐)

---

## 1. 목표

- 클라이언트가 칼날 캡슐로 평타 적중을 **서버 응답 전에** 판정한다.
- 판정 결과는 Unity의 `OnTriggerEnter`처럼 플레이어 스크립트의 콜백(`on_trigger_enter`)으로 돌려준다.
- 이번 단계는 **로그와 디버그 기록으로만** 확인한다. 히트스톱·타격음 등 연출은 3단계에서 같은 콜백에 연결한다.
- 서버 판정(HP, 데미지, 사망)은 그대로 둔다. 클라이언트 판정은 연출용 예측이다.

범위 밖: 연출(3단계), 대검 스킬(4단계), PvP 판정(NPC와 같은 구조라 나중에 다른 플레이어에 피격 히트박스만 붙이면 됨), 서버 판정 방식 변경.

---

## 2. 현재 서버 판정 (참고)

| 항목 | 값 | 위치 |
|---|---|---|
| 평타 판정 모양 | 반지름 2m 구체, 플레이어 앞 1m | `Room.cpp` `Execute_C2S_ACTION` |
| 판정 시점 | 클라이언트가 애니메이션 30%에 보낸 패킷 1회, 리와인드 적용 | `MainPlayerScript::process_attack_and_packet` |
| 일반 NPC 히트박스 | 캡슐 반지름 0.5, 원기둥 반높이 0.4 (키 1.8m), 발바닥 위 0.9m | `NPC.cpp` 생성자 |
| 보스(Tainer) 히트박스 | 캡슐 반지름 3.0, 원기둥 반높이 1.0 (키 8m), 발바닥 위 4m | `Tainer.cpp` 생성자 |
| DynamicBox 히트박스 | 박스 반크기 0.5, 중심 | `NPC.cpp` 생성자 |
| NPC 위치 기준 | 발바닥 (`NPCControllerComponent`가 캐릭터 중심에서 반높이를 뺀 값) | `NPCControllerComponent.cpp` |
| 히트박스 변환 | NPC 위치 + 회전만 사용 (스케일 없음) | `HitboxComponent::CheckCollision` |
| 피격 쿨다운 | 0.5초 (그동안 판정 실패, 패킷에 안 실림) | `NPC::ValidateHit` |

서버 구체(반지름 2m)가 칼날보다 넉넉하므로, 클라이언트가 맞혔다고 본 대상은 대부분 서버에서도 맞는다.

---

## 3. 설계

### 3.1 Unity와 비교

- Unity는 트리거 콜라이더와 Rigidbody가 겹치면 물리 단계에서 그 오브젝트의 모든 스크립트에 `OnTriggerEnter(Collider other)`를 호출한다. 스크립트는 받기만 하면 된다.
- 다만 키네마틱 바디(애니메이션으로 움직이는 칼)는 연속 충돌 감지를 쓰지 않아 빠른 스윙에서 적을 건너뛴다. 그래서 Unity 액션 게임도 칼 판정은 직전·현재 프레임 사이를 직접 쓸어 검사하는 경우가 많다.
- PIP는 **콜백 쪽은 Unity처럼**(`ScriptComponent::on_trigger_enter`), **판정은 스윕으로** 한다. 엔진에는 이미 `on_collision_enter`를 오브젝트의 모든 스크립트에 전달하는 통로가 있어 같은 방식으로 추가했다.

### 3.2 콜라이더 역할과 피격 히트박스

- `PhysicsColliderComponent::Role`: `Hitbox`(공격, 칼날), `Hurtbox`(피격, NPC 몸통), `None`(기존 용도).
  - `Hurtbox`로 지정하면 정적 목록(`hurtboxes()`)에 등록되고, 소멸 시 빠진다.
- NPC 피격 히트박스: `NetworkManager::HANDLE_S2C_SPAWN_NPC`에서 종류별로 `QueryOnly` 콜라이더를 붙인다(`attach_npc_hurtbox`). 값은 2장 서버 값과 같다. QuestNPC, Elevator는 붙이지 않는다.
- **스케일 무시**(`set_ignore_owner_scale(true)`): 클라이언트 NPC는 연출용으로 키워서 그린다(매직 컨스트럭트 1.5배, 보스 5배). 그대로 계산하면 오프셋에도 배율이 곱해져 히트박스가 뜬다(매직 컨스트럭트 0.9m → 1.35m, 보스 4m → 20m). 서버처럼 오브젝트의 위치와 회전만 쓴다.

### 3.3 판정 구간과 흐름

- 판정 구간: 평타 애니메이션 진행도 **5%~80%**.
- 흐름 (한 프레임):
  1. `MainPlayerScript::update` → `process_attack_and_packet`: 진행도가 구간에 들어오면 `begin_hit_query()`, 벗어나면 `end_hit_query()`. 켤 때 맞힌 대상 목록을 비운다.
  2. 칼날 `PhysicsColliderComponent::late_update`: 월드 변환을 갱신한 뒤 판정이 켜져 있으면 스윕 판정(3.4).
  3. 겹친 NPC마다 한 번 `game_object()->on_trigger_enter(npc, hit)` → `MainPlayerScript::on_trigger_enter`.
- 안전장치: 평타가 끝났는데 판정이 켜져 있으면 `update` 시작에서 끈다. 스킬 중에는 끈다(스킬은 4단계).
- NPC 히트박스는 각 NPC의 `late_update`에서 갱신된다. NPC가 플레이어보다 나중에 갱신되면 한 프레임 전 위치를 읽는데, NPC는 칼보다 훨씬 느리므로 허용한다.

### 3.4 프레임 사이 보간 (스윕)

- 한 프레임 사이 칼 회전이 매우 크다(평타 30% 시점 기록에서 약 90도). 현재 자세만 검사하면 그 사이의 적을 놓친다.
- 직전 변환과 현재 변환 사이를 N단계로 나눠 각 자세에서 겹침을 검사한다(회전 slerp, 위치 lerp).
  - N = ceil(max(회전 각도 / 15도, 이동 거리 / 0.2m)), 1~12.
  - 직전 자세(t = 0)는 이전 프레임에 검사했으므로 t > 0만 검사한다.
- 겹침 검사는 서버와 같은 `CollisionDispatch::sCollideShapeVsShape`(모양 대 모양). 히트박스를 Jolt 물리 시스템에 등록할 필요가 없다.
- 후보 거르기: NPC 히트박스 중심이 칼날 직전·현재 위치에서 (칼날 크기 × 2 + 히트박스 크기 + 이동량)보다 멀면 건너뛴다.
- 적중 정보(`TriggerHit`): 상대 콜라이더, 적중 지점(접촉점), 그 지점에서 칼이 움직인 방향(직전 → 현재, 정규화).

### 3.5 중복 적중 방지

- 판정을 켤 때(평타 한 번) 맞힌 대상 목록을 비우므로, 한 번의 평타에 같은 대상은 한 번만 들어온다.
- 서버 피격 쿨다운(0.5초) 흉내: `MainPlayerScript::on_trigger_enter`에서 NPC id별 마지막 예측 적중 시각을 보고 0.5초 안이면 무시한다. 게임 규칙이라 콜라이더가 아니라 스크립트에 둔다.
- 죽은 NPC(HP 0 이하)는 무시한다.

### 3.6 결과 처리 (이번 단계)

- `CLOG("[MeleeHit] 예측 적중: NPC <id> (<이름>), 진행도 <n>%, 지점 (...), 방향 (...)")`.
- 쿨다운으로 무시한 경우 `[MeleeHit] 쿨다운 중이라 무시` 로그.
- F7 자동 기록: 켜져 있으면 판정 구간(5~80%) 동안 **매 프레임** 기록한다(라벨 `attack <n>%`). 적중 프레임은 라벨이 `hit NPC <id>`로 바뀐다. 1단계의 30/45/60% 세 프레임 기록을 대체한다.

### 3.7 디버그 기록

| 내용 | 색 |
|---|---|
| 칼날(Hitbox) 현재 / 직전 프레임 | 초록 / 어두운 초록 |
| NPC 히트박스(Hurtbox) | 하늘색 |
| 이번 프레임에 맞은 히트박스 | 빨강 |
| 적중 지점 | 빨간 십자 |
| 적중 지점의 칼 진행 방향 | 노란 화살표 |
| 피격 히트박스를 가진 오브젝트(NPC)의 실제 메쉬, 애니메이션 자세로 CPU 스키닝 | 회색 와이어 (플레이어 반경 15m 안만) |

스윕 중간 자세는 그리지 않는다. 필요하면 추가한다.

---

## 4. 변경 파일

| 파일 | 변경 |
|---|---|
| `ScriptComponent.h` | `TriggerHit` 구조체, `on_trigger_enter(other, hit)` 가상 함수 |
| `GameObject.h/.cpp` | `on_trigger_enter`: 모든 스크립트에 전달 |
| `PhysicsColliderComponent.h/.cpp` | `Role`, `hurtboxes()` 목록, `set_ignore_owner_scale`, `begin/end_hit_query`, 스윕 판정(`run_hit_query`), `last_hits()` |
| `NetworkManager.cpp` | `attach_npc_hurtbox`: NPC 생성 시 종류별 피격 히트박스 |
| `MainPlayerScript.h/.cpp` | 판정 구간 5~80% 제어, `on_trigger_enter`(쿨다운 흉내, 로그, 적중 프레임 기록) |
| `PhysicsDebugCapture.cpp` | 역할별 색, 적중 히트박스 빨강, 적중 지점과 방향, NPC 메쉬 |
| `AnimationComponent.h`, `ReadGLTFMesh.h/.cpp` | 스키닝 팔레트 조회, CPU 스키닝 형상(`get_skinned_geometry`) |

---

## 5. 검증 체크리스트

- [x] Debug/Release 빌드 성공 (2026-10-04).
- [x] 첫 테스트에서 예측 적중이 한 번도 나지 않음 → 칼날 콜라이더에 `Role::Hitbox` 지정이 빠져 판정이 돌지 않았음. 수정함 (2026-10-04).
- [x] NPC 히트박스가 기록에서 서버 값과 같은 위치·크기로 그려진다: 드래곤 브루트(몸통을 감쌈, 팔·꼬리는 서버와 같이 밖), DynamicBox (2026-10-04). 매직 컨스트럭트, 보스는 미확인.
- [ ] 스케일 1.5/5인 NPC도 히트박스 중심이 발바닥 위 0.9m(보스 4m)에 있다.
- [x] 평타로 NPC를 칠 때 `[MeleeHit] 예측 적중` 로그가 한 번만 찍힌다 (평타 2회 모두 진행도 22~23%에 1회, 2026-10-04).
- [ ] 칼이 NPC에 닿지 않는 거리·각도에서는 예측 적중이 나지 않는다.
- [x] 한 프레임에 크게 지나간 경우에도 맞는다 (F7 기록 적중 프레임에서 직전·현재 칼 사이 각도가 큰데 적중, 히트박스 빨강과 적중 지점 표시 확인).
- [ ] 같은 NPC를 0.5초 안에 다시 치면 `쿨다운 중이라 무시` 로그가 찍힌다.
- [ ] 서버 응답(데미지 숫자)과 예측 적중이 대부분 일치한다. 불일치 사례를 기록한다.
