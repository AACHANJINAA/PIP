# 범용 파티클 시스템 계획

작성일: 2026-10-04
관련 문서: `HitFeel_Plan_KR.md` 3단계(연출), `MeleeHitPrediction_Spec_KR.md` (예측 적중 콜백)

---

## 1. 목표

- 타격 스파크, 휘두르기 먼지, 피격 이펙트처럼 **여러 곳에서 짧게 터뜨리는 범용 파티클**을 만들 수 있는 시스템을 만든다.
- 기존 `ParticleSystemComponent`(대검 스킬의 "칼 모양으로 모이는" 연출)는 새 시스템의 **파생 컴포넌트**로 옮기고, 동작은 그대로 유지한다.
- 렌더러의 파티클 처리 경로를 하나로 합쳐, 기존 스킬 파티클과 새 파티클이 같은 경로로 연산·그리기 된다.
- 이후 연출 시스템(히트스톱, 카메라 줌, 칼 잔상)에서 `이펙트 이름 + 위치 + 방향`만으로 파티클을 재생할 수 있게 한다.

### 1.1 설계 원칙: 유니티처럼 쓰기 쉽게 (확정 2026-10-06)

내부는 GPU 컴퓨트로 돌리되, **게임 코드를 쓰는 프로그래머에게는 유니티 `ParticleSystem`처럼 보이게** 한다. 엔진 전체가 유니티식 `GameObject` / 컴포넌트 / `required_components` 구조이므로 같은 철학을 유지한다.

- **컴포넌트로 붙이고 설정값을 채우면 끝.** `add_component<ParticleSystemComponent>()` → 설정 구조체(유니티 인스펙터의 Main·Emission·Shape 모듈에 해당) 채우기 → `play()` / `stop()` / `emit(개수)`. 버퍼, 루트 시그니처, PSO, 디스패치는 게임 코드에 드러나지 않는다.
- **한 줄 재생.** 타격 스파크처럼 오브젝트 없이 터뜨리는 이펙트는 `VfxManager::instance()->play("hit_spark", 위치, 방향)` 한 줄. 유니티에서 프리팹을 `Instantiate`하는 것에 해당하지만 내부는 풀링이라 오브젝트를 만들지 않는다.
- **트랜스폼을 따른다.** 오브젝트에 붙인 방출기는 그 오브젝트의 위치·회전에서 방출한다(유니티 Simulation Space: Local/World 선택에 해당하는 설정 하나).
- **이름과 단위는 유니티와 비슷하게.** 수명(초), 시작 속도(m/s), 시작 크기(m), 중력 배율, 초당 방출 수, 버스트, 방출 모양(Sphere, Cone 등). 설정 항목 이름만 보고 유니티 경험으로 짐작할 수 있게 한다.
- **잘못 써도 조용히 망가지지 않게.** 풀 크기를 넘는 방출, 잘못된 설정은 로그로 알린다.

범위 밖(이후 단계): 칼 잔상(트레일 리본), 타격 충격파 왜곡(4장 P4), 화면 전체 후처리 효과, 연출 프리셋 시스템. 이 문서의 매니저 구조가 그 기반이 된다.

---

## 2. 기존 시스템 분석

### 2.1 구성

| 파일 | 역할 |
|---|---|
| `ParticleSystemComponent.h/.cpp` | 목표점 버퍼(정답지) 업로드, 컴퓨트 PSO 생성, 매 프레임 상수(무기 행렬, 플레이어 위치, 진행도, 사라짐 진행도) 보관, `dispatch_compute` |
| `Particle_CS.hlsl` | 파티클 하나당 스레드 하나. 플레이어 가슴에서 터졌다가 목표점(칼 모양)으로 빨려 드는 위치를 **진행도로부터 직접 계산**(누적 시뮬레이션 아님). 사라질 때는 우상단으로 날아감 |
| `ParticleRenderComponent.h/.cpp` | `RenderComponent` 파생. 위치 버퍼를 SRV로 묶고 `DrawInstanced(4, 개수)` (버텍스 버퍼 없이 빌보드) |
| `ParticleShader.h/.cpp`, `Particle_Draw.hlsl` | 카메라를 향하는 사각형, 절차적 부드러운 원, 진행도에 따라 흰색 → 고유 색, 알파 블렌딩 |
| `RootSignature.cpp` | `compute_particle`(상수 + SRV + UAV), `particle_draw`(b0, b1 카메라, b2 상수 7개, t0 버퍼, t1 텍스처) |
| `Renderer.cpp` | `particle_draw` PSO 오브젝트마다 컴퓨트 → 그래픽 상태 복구 → 그리기 |

### 2.2 사용처

| 위치 | 용도 |
|---|---|
| `MainPlayerScript` | 대검 스킬 (5만 개, 플레이어 고유 색) |
| `OtherPlayerScript` | 다른 플레이어의 대검 스킬 |
| `Main_Scene` | 컷씬용 플레이어 파티클, 더미 분수 파티클(30만 개) |

### 2.3 범용으로 쓰기 어려운 점

- 파티클마다 **수명·속도 개념이 없다.** 위치가 "스킬 진행도"의 함수라, 한 번 터지고 흩어지는 이펙트를 만들 수 없다.
- 방출(spawn) 개념이 없다. 파티클 수 = 목표점 수로 고정되고, 생성 시 한 번 정해진다.
- 상수 구조가 스킬 전용이다(무기 행렬, 플레이어 위치, 스킬 진행도).
- 오브젝트 하나에 시스템 하나가 붙는 구조라, 타격마다 게임 오브젝트를 만들면 비싸다.
- 셰이더 파일을 런타임에 컴파일하고 컴포넌트마다 컴퓨트 PSO를 새로 만든다.

### 2.4 발견한 문제 (이번 작업에서 같이 고침)

1. **루트 상수 개수 불일치:** `compute_particle` 루트 시그니처는 상수 21개(`InitAsConstants(21, 0)`)인데, `dispatch_compute`는 24개(`SetComputeRoot32BitConstants(0, 24, ...)`)를 넣는다. D3D12 디버그 레이어 오류이고 정의되지 않은 동작이다.
2. **파티클 수 상한 하드코딩:** `Particle_CS.hlsl`이 `if (idx >= 50000) return;`으로 끊는다. 더미 분수는 30만 개를 만들기 때문에 5만 번째 이후 파티클은 위치가 갱신되지 않은 채(초기값) 그려진다. 상수로 실제 개수를 넘겨야 한다.
3. **컴포넌트마다 PSO 생성:** 같은 컴퓨트 셰이더를 컴포넌트마다 컴파일·생성한다. 공용으로 한 번만 만든다.
4. **렌더러 파티클 경로가 두 군데:** `Renderer.cpp`에 `particle_draw` 처리 블록이 두 곳(일반 PSO 순회 안, Step 5) 있다. 실제로 두 번 실행되는지 확인하고 하나로 합친다.

---

## 3. 설계

### 3.1 클래스 구조

```
ParticleSystemComponent (새 기반, 범용)
├─ 방출기 설정(ParticleEmitterDesc), GPU 파티클 풀, 방출 요청 큐
├─ virtual dispatch_compute(cmd)   : 방출 + 갱신 컴퓨트
├─ virtual draw(cmd, frame)        : 빌보드 그리기
└─ GatherParticleComponent (파생, 기존 대검 스킬 연출)
     기존 init_particles / set_compute_data / set_particle_dying 등 API 유지
     기존 Particle_CS.hlsl, Particle_Draw.hlsl 그대로 사용
```

- 파생 클래스 이름은 `GatherParticleComponent`(목표점으로 모이는 파티클)를 제안한다. 사용처 5곳의 `ParticleSystemComponent`를 이 이름으로 바꾼다.
- 렌더러는 `ParticleSystemComponent*`의 가상 함수만 호출한다. 기존/새 파티클을 구분하지 않는다.
- `ParticleRenderComponent`는 렌더 목록에 들어가기 위한 껍데기로 유지하되, 실제 그리기는 파티클 컴포넌트의 `draw`로 넘긴다. 유니티처럼 파티클 컴포넌트의 `required_components`로 자동으로 붙게 해서, 게임 코드가 렌더 컴포넌트를 따로 붙이지 않게 한다.

게임 코드에서 쓰는 모습 (1.1 원칙):

```cpp
// 오브젝트에 붙이는 방출기 (예: 횃불 불씨)
auto ps = torch->add_component<ParticleSystemComponent>();
auto& s = ps->settings();          // 유니티 Main·Emission·Shape 모듈에 해당
s.start_lifetime = { 0.5f, 1.0f }; // 초 (최소, 최대)
s.start_speed = { 1.0f, 2.0f };    // m/s
s.rate_over_time = 40.0f;          // 초당 방출 수
s.shape = ParticleShape::Cone;
ps->play();

// 오브젝트 없이 한 번 터뜨리는 이펙트 (프리셋 + 풀링)
VfxManager::instance()->play("hit_spark", hit.point, hit.direction);
```

### 3.2 범용 파티클: CPU는 방출 요청만, 초기값 생성부터 GPU (확정 2026-10-06)

언리얼 Niagara·유니티 VFX Graph와 같은 분담이다(3.6). CPU는 "몇 개를, 언제, 어디서"만 정하고, 파티클 하나하나의 초기값 생성·갱신·그리기는 GPU 컴퓨트 셰이더가 한다.

| 항목 | 방식 |
|---|---|
| 파티클 저장 | 방출기(프리셋)마다 고정 크기 풀(RWStructuredBuffer, 기본 2048개). 링 버퍼처럼 앞에서부터 덮어씀 |
| 방출 요청 (CPU) | `play`·연속 방출이 **방출 요청**(위치, 방향, 개수, 난수 시드)을 쌓는다. 프레임마다 요청 목록을 업로드 버퍼에 담는데, CPU가 링 버퍼 쓰기 위치를 관리해 요청마다 풀 시작 위치를 미리 정해 둔다(GPU 원자 연산 불필요). 풀이 차면 새로 만들지 않는다(유니티 VFX Graph Capacity, Niagara FixedCount와 같음). CPU가 묶음별 최대 수명으로 빈자리를 추정하므로 GPU에서 개수를 읽어 오지 않는다 |
| 방출 (GPU) | **방출 컴퓨트**: 이번 프레임 새 파티클 수만큼 스레드. 스레드는 자기 번호로 요청을 찾고(요청 수가 적어 순차·이진 탐색), `시드 + 번호`로 만든 해시 난수와 프리셋 상수(방출 모양, 수명·속도·크기 범위, 퍼짐 각도)로 초기값을 만들어 풀에 쓴다 |
| 갱신 (GPU) | **갱신 컴퓨트**가 풀 전체를 돈다: 나이 증가, 속도 적분(중력, 공기 저항), 수명이 다하면 죽음 표시 |
| 그리기 (GPU) | 풀 크기만큼 `DrawInstanced(4, 풀 크기)`. 죽은 파티클은 정점 셰이더에서 화면 밖으로 보내 버린다(정렬·압축 없음). CPU로 다시 읽어오지 않음 |
| 시간 | 방출기 오브젝트의 시간(`GameObject` 시간 배율). 히트스톱은 맞은 오브젝트에만 걸리므로 파티클은 계속 움직인다 |

- CPU가 파티클마다 초기값을 만들어 올리는 방식(처음 계획) 대신 이 방식을 고른 이유: 업로드가 이펙트 하나당 요청 1개(수십 바이트)라 먼지·불씨처럼 한 번에 수천 개를 뿜는 이펙트도 같은 구조로 감당한다. 두 엔진도 파티클별 초기화(Niagara Particle Spawn, VFX Graph Initialize)를 GPU에서 한다.
- 방출 모양·난수 로직이 셰이더에 있어 고치기 번거로운 점은, 모양 종류(점, 구, 원뿔, 방향 원뿔, 선분)를 셰이더에 만들어 두고 프리셋의 숫자로 고르게 해서 줄인다. 같은 시드면 같은 결과라 디버깅 때 재현할 수 있다.
- 넣지 않는 것: GPU 이벤트(파티클이 죽을 때 다른 파티클 생성), CPU 시뮬레이션 모드, Append/Consume 버퍼, 간접 그리기. 필요해지면 나중에 추가한다.

방출 요청 (요청 하나당, 48바이트):

| 필드 | 크기 |
|---|---|
| 위치 float3, 풀 시작 위치 uint | 16 |
| 방향 float3, 개수 uint | 16 |
| 난수 시드 uint, 이번 요청의 첫 스레드 번호 uint, 예비 2개 | 16 |

GPU 파티클 데이터(파티클 하나당, 48바이트):

| 필드 | 크기 |
|---|---|
| 위치 float3, 나이 float | 16 |
| 속도 float3, 수명 float | 16 |
| 시작 크기, 끝 크기, 회전, 난수 시드 | 16 |

색과 크기의 수명에 따른 변화는 방출기 상수(시작/끝 값, 이징)로 계산한다.

### 3.3 방출기 설정 (ParticleEmitterDesc)

| 분류 | 항목 |
|---|---|
| 방출 | 한 번에 터뜨릴 개수(버스트), 초당 방출량(연속), 방출 모양(점, 구, 원뿔, 방향 원뿔) |
| 초기값 | 수명 범위, 속도 범위, 방향 퍼짐 각도, 크기 범위, 회전 범위 |
| 운동 | 중력, 공기 저항 |
| 수명에 따른 변화 | 색(시작 → 끝, 알파 포함), 크기(시작 → 끝 배율) |
| 그리기 | 빌보드 방식(카메라 정면 / **속도 방향으로 늘림**: 스파크·불똥용), 블렌딩(가산 / 알파), 모양(절차적 부드러운 원 / 텍스처) |

- 처음에는 C++ 구조체로 프리셋을 정의한다(예: `hit_spark`, `slash_dust`). 데이터 파일(JSON/Lua) 로드는 프리셋이 안정된 뒤에 한다.
- 방출·초기값 항목은 GPU 방출 컴퓨트가 읽는 상수(방출기마다 하나)로 올린다. 방출 모양은 셰이더에 있는 종류 중 번호로 고른다.

### 3.4 VfxManager (이펙트 재생 창구)

```cpp
VfxManager::instance()->play("hit_spark", hit.point, hit.direction);
```

- 이펙트 이름별로 방출기 오브젝트를 하나씩만 만들어 두고(풀링), `play`는 그 방출기에 방출 요청만 쌓는다. 타격마다 게임 오브젝트를 만들지 않는다.
- 같은 이펙트를 여러 곳에서 동시에 재생해도 한 풀을 나눠 쓴다(위치·방향은 방출 요청마다 다름).
- 이후 연출 시스템(히트스톱, 카메라 줌, 사운드를 묶은 타격 프리셋)이 이 창구를 쓴다.

### 3.5 렌더링 경로

- `Renderer`의 파티클 단계를 하나로 합친다. 순서: 모든 파티클의 컴퓨트(방출 → 갱신)를 먼저 몰아서 실행 → 그래픽 상태 한 번 복구 → 블렌딩별로 그리기.
  - 지금은 파티클 오브젝트마다 컴퓨트 ↔ 그래픽 전환을 반복한다.
- PSO: `particle_draw`(기존, 알파), `particle_additive`(새, 가산), `particle_alpha`(새, 알파). 깊이 테스트는 하고 깊이 쓰기는 끈다.
- 루트 시그니처: 기존 두 개 유지 + 새 `particle_update`(컴퓨트), `particle_billboard`(그래픽).
- 컴퓨트 PSO와 셰이더는 매니저가 한 번만 만든다.

### 3.6 참고: 언리얼·유니티의 분담 (2026-10-06 조사)

| 엔진 | CPU | GPU |
|---|---|---|
| 언리얼 Niagara (GPU 이미터, `GPUComputeSim`) | Emitter Update(GPU 이미터여도 CPU에서 매 틱): 스폰 속도·버스트 개수 | Particle Spawn 스크립트(파티클별 초기값), Particle Update 스크립트를 컴퓨트 셰이더로. 한 프레임 최대 GPU 생성 수 설정 있음 |
| 유니티 VFX Graph | Spawn 컨텍스트(이번 프레임 생성 수, 게임 코드 이벤트) | Initialize(새 파티클마다 1회), Update, Output. GPU 이벤트로 파티클이 파티클 생성 가능 |
| 유니티 Shuriken (기존 Particle System) | 시뮬레이션 전체 | 그리기만 |

- 출처: Epic "How to create a GPU sprite effect in Niagara", UE 4.27 Niagara GPU Particles·Emitter Update 문서, Unity VFX Graph Contexts·Context Initialize 문서.

---

## 4. 작업 순서

### P1. 기존 시스템 분리 (동작 변화 없음)

1. 기존 `ParticleSystemComponent`를 `GatherParticleComponent`로 이름을 바꾸고, 새 기반 클래스 `ParticleSystemComponent`(가상 `dispatch_compute`, `draw`, `particle_count`)를 만든다.
2. 사용처 5곳(`MainPlayerScript`, `OtherPlayerScript`, `Main_Scene` 3곳)과 `ParticleShader`, `ParticleRenderComponent`, `Renderer`를 새 구조에 맞춘다.
3. 2.4의 문제 1~4를 고친다(루트 상수 개수, 개수 상한, 공용 PSO, 중복 경로).
4. 검증: 대검 스킬, 다른 플레이어 스킬, 컷씬 파티클, 분수 파티클이 전과 같이 보인다. D3D12 디버그 레이어 오류가 없다.

### P2. 범용 파티클 기본 (방출 + 갱신 + 빌보드)

1. `Particle_Emit_CS`(요청 찾기, 해시 난수, 방출 모양별 초기값), `Particle_Update_CS`, `Particle_Billboard.hlsl` 작성.
2. 새 루트 시그니처와 PSO(가산/알파).
3. `ParticleSystemComponent`의 범용 구현: 풀 버퍼, 방출 요청 큐와 링 버퍼 쓰기 위치 관리, 요청 업로드, `emit(개수, 위치, 방향)`, 연속 방출.
4. 검증: 디버그 키로 플레이어 앞에 버스트를 터뜨려 수명, 중력, 색·크기 변화, 가산 블렌딩을 확인한다.

### P3. VfxManager와 타격 스파크

1. `VfxManager`: 프리셋 등록, 이름별 방출기 풀링, `play(이름, 위치, 방향)`.
2. 프리셋 `hit_spark`: 속도 방향으로 늘린 흰색·주황 불똥, 칼 진행 방향(`hit.direction`)을 축으로 한 원뿔 방출, 수명 0.15~0.3초.
3. `MainPlayerScript::on_trigger_enter`에서 예측 적중 시 `hit.point`에 재생.
4. 검증: 평타로 NPC를 칠 때 칼이 닿은 자리에서 칼 방향으로 불똥이 튄다.

### P4. 이후 (별도 계획)

- **타격 충격파 왜곡 (shockwave distortion)**: 맞은 지점에서 투명한 고리가 퍼지며 뒤 화면이 물결처럼 굴절되는 효과(0.2~0.4초). 애니메식 타격 연출의 "화면 일렁임".
  - 방식: 불투명 물체와 하늘을 그린 뒤 씬 색을 텍스처로 복사해 두고, 충격파를 그 텍스처를 굴절(UV 오프셋)시켜 그리는 **왜곡 파티클**로 그린다. 파티클 시스템의 블렌딩/그리기 종류 하나로 넣어 `VfxManager::play("hit_shockwave", 위치)`로 스파크와 같이 재생한다.
  - 필요한 기반: 지금 렌더러는 씬을 백버퍼에 바로 그리므로, 파티클 단계 전에 백버퍼(또는 오프스크린 씬 타깃)를 텍스처로 복사하는 단계가 필요하다.
  - 화면 전체에 거는 방사형 줌 블러(강공격·필살기용)는 별도. 필요해지면 씬을 오프스크린 타깃에 그리고 후처리 패스로 옮기는 구조를 만든다.
- 텍스처·플립북 애니메이션, 알파 파티클 정렬.
- 칼 잔상(트레일 리본) — 같은 매니저와 렌더 단계에 붙인다.
- 연출 프리셋 시스템(히트스톱, 카메라 줌, 사운드, 파티클을 묶음).
- 프리셋을 데이터 파일로.

---

## 5. 정해야 할 것

모두 확정(2026-10-06).

1. **클래스 구조:** 범용 `ParticleSystemComponent`가 기반, 기존 대검 연출은 파생 `GatherParticleComponent`.
2. **시뮬레이션 방식:** CPU는 방출 요청만, 방출(초기값 생성)·갱신·그리기는 GPU 컴퓨트. 3.2, 3.6.
3. **최대 개수:** 이펙트마다 고정 `max_particles`(기본 2048), 꽉 차면 새로 만들지 않음. 유니티 `Max Particles`·VFX Graph Capacity, Niagara FixedCount와 같은 방식.
4. **프리셋 형식:** Lua 파일(서버 데이터 `NPC_Data.lua` 등과 같은 방식, 주석·변형 가능, 실행 중 다시 읽기). 클라에 Lua 5.4.2 라이브러리를 복사해 연결.

---

## 6. 구현 체크리스트

항목을 끝낼 때마다 `[x]`로 바꾸고, 필요하면 결과나 바뀐 결정을 한 줄 붙인다. 단계 마지막 검증을 통과해야 다음 단계로 간다. 작업 브랜치: `worktree-particle-system`(워크트리 `.claude/worktrees/particle-system`).

### P0. 클라이언트 Lua 연결

- [x] `Server/Server/lua-5.4.2_Win64_dll17_lib/`(include, `lua54.lib`, `lua54.dll`)를 `Client/Client/ThirdParty/lua-5.4.2/`로 복사
- [x] `Client.vcxproj` Debug·Release: include·lib 경로, `lua54.lib` 링크, 빌드 후 `lua54.dll`을 `$(OutDir)`로 복사 — PostBuildEvent `xcopy /Y /D`
- [x] `LuaUtil.h/.cpp`: `luaL_dofile`, 전역 테이블 순회, number/bool/string/vec3/color/range 읽기(없으면 기본값, 모르는 키 로그). 서버 `LuaManager`의 C API 사용 방식 따름 — `LuaState`, `LuaTable`(읽은 키 기록 → `warn_unknown_keys`), `lua_for_each_global_table`, `for_each_array`
- [x] 검증: 빌드, 테스트 Lua 파일 하나를 읽어 로그로 값 확인 (확인 후 테스트 코드 제거) — 게임 밖 별도 테스트 프로그램으로 확인(저장소에 안 넣음): 범위·색·버스트 배열·`extend` 변형, 타입 오류·오타 키·없는 파일 로그. 클라 빌드 성공, `lua54.dll` 복사 확인

### P1. 기존 시스템 분리 (화면 변화 없음)

- [x] 기존 `ParticleSystemComponent` → `GatherParticleComponent`(새 파일). API 그대로
- [x] 새 기반 `ParticleSystemComponent`: 가상 `dispatch_compute(cmd)`, `draw(cmd, frame)`, `particle_count()` (범용 구현은 P2). Gather가 셋을 오버라이드 — P1에서는 가상 함수만, 기본 구현은 아무것도 안 함
- [x] `ParticleRenderComponent`: 같은 오브젝트의 `ParticleSystemComponent`를 찾아 `draw` 호출, 파티클 컴포넌트 `required_components`로 자동 부착 — `set_particle_system` 제거(사용처 4곳 호출도 삭제)
- [x] 사용처 교체: `MainPlayerScript`, `OtherPlayerScript`, `Main_Scene`(컷씬·분수), `ParticleShader::update_per_object`, `Renderer` — ParticleShader는 Gather 전용으로, 없으면 바로 반환(기존 널 검사 누락 수정)
- [x] 버그 1: `compute_particle` 루트 상수 21 → 24 (`RootSignature.cpp`)
- [x] 버그 2: `Particle_CS.hlsl`의 `idx >= 50000` → 실제 개수를 상수로 전달 — 패딩 자리에 `g_ParticleCount`
- [x] 버그 3: 컴퓨트 PSO를 컴포넌트마다 만들지 않고 최초 1회 공유 — `Renderer::get_or_create_compute_pso`(이름으로 공유, 실패도 기록해 매 프레임 재컴파일 안 함)
- [x] 버그 4: 렌더러 파티클 경로 2곳(`draw_render_list` 분기, 미사용 `draw_render_occlusion_culling_list` Step 5)을 `Renderer::render_particles(...)`로 합침 — `Renderer::render_particle_group`. 실제로 도는 건 `draw_render_list` 쪽 하나였음(오클루전 경로는 호출 안 됨)
- [x] 검증: 대검 스킬(내 것·다른 플레이어), 컷씬 파티클, 분수 파티클이 전과 같음. 분수 30만 개 전부 움직임. D3D12 디버그 레이어 오류 없음 — 2026-10-06 Release 서버 + 클라 2개로 확인. 상한 제거 후 분수가 30만 개 전부 보여 많아짐 → 예전에 실제로 보이던 5만 개로 생성 수 변경. 디버그 레이어는 VS 밖 실행이라 미확인

### P2. 범용 파티클 (GPU 방출 + 갱신 + 빌보드)

- [x] `ParticleSystemSettings` (유니티 모듈 이름): Main(`duration`, `looping`, `play_on_awake`, `start_lifetime/speed/size/rotation` 범위, `start_color` 두 색, `gravity_modifier`, `drag`, `max_particles`=2048), Emission(`rate_over_time`, `bursts`), Shape(Point/Sphere/Hemisphere/Cone/Edge, `radius`, `angle`, `length`), Over lifetime(`end_color`, `end_size_multiplier`), Renderer(`blend` Additive/Alpha, `render_mode` Billboard/StretchedBillboard, `length_scale`). 공간은 World만 — `ParticleSystemSettings.h`, 두 번째 시작 색은 `start_color_2`
- [x] API: `play()`, `stop()`, `clear()`, `emit(count)`, `emit(count, pos, dir)`, `is_playing()`, `settings()`
- [x] CPU 방출: 연속 방출 누적 + 버스트 → 방출 요청, 오브젝트 시간 기준 — 주기 첫 프레임에만 0초 버스트(히트스톱으로 시간이 0이어도 중복 안 함)
- [x] 링 버퍼 관리: 묶음별 (시작, 개수, 최대 수명 만료 시각), 만료된 꼬리 전진, `max_particles` 초과분은 잘라내고 로그 1회 (GPU 읽기 없음)
- [x] 업로드: 요청·설정·dt를 `GameFramework::linear_allocator()`로 — 요청이 없어도 t0용 빈 자리 1개 확보
- [x] 셰이더 `Particle_Emit_CS.hlsl`: 요청 찾기, 해시 난수, 모양별 초기값 — 공용 정의는 `Particle_Common.hlsli`, 해시는 PCG
- [x] 셰이더 `Particle_Update_CS.hlsl`: 나이, 중력·저항, 죽음 표시 — `clear()`는 상수 플래그로 갱신 컴퓨트가 전부 지움
- [x] 셰이더 `Particle_Billboard.hlsl`: 빌보드·속도 늘림, 수명별 색·크기, 부드러운 원, 죽은 것은 화면 밖 — 셰이더 파일은 BOM 없이 저장(컴파일러가 BOM을 못 읽음), fxc로 전부 컴파일 확인
- [x] 루트 시그니처 `particle_compute`, `particle_billboard` (`RootSignature.cpp`)
- [x] `ParticleBillboardShader` 프로토타입 2개 → PSO `particle_additive`, `particle_alpha` (깊이 테스트 O, 쓰기 X)
- [x] 렌더러: `draw_render_list` 시작 시 모든 파티클 그룹 컴퓨트 선행 패스 → 힙 1회 복구 → 그룹별 `draw` — PSO 그룹 단위로 컴퓨트를 먼저 몰아서 실행(그룹 3개라 그룹마다 상태 복구 1회), `render_order`에 `particle_alpha`·`particle_additive` 추가
- [x] 검증: 플레이어 앞 임시 테스트 방출기로 수명·중력·색/크기 변화·가산/알파·속도 늘림 확인, `max_particles` 초과 로그, 디버그 레이어 오류 없음 (확인 후 테스트 방출기 제거) — 2026-10-06 Release로 모드 4가지(분수·연기·폭발·비) 확인, 최대 개수 초과 로그 확인. 테스트 방출기는 지우지 않고 디버그 패널(`\` 키, `DebugPanel`)의 Particle Test로 옮김(사용자 요청). 디버그 레이어는 VS 밖 실행이라 미확인

### P3. Lua 프리셋 + VfxManager + 타격 스파크

- [ ] 프리셋 파일 `Resource/Vfx/VfxPresets.lua` (`vfx.이름 = {...}`, 변형은 `extend(base, overrides)`)
- [ ] `VfxPresetLibrary`: Lua 테이블 → `ParticleSystemSettings`, 모르는 키·잘못된 값 로그
- [ ] `VfxManager`: `play(name, pos, dir, scale = 1)`, 이름별 방출기 오브젝트 지연 생성(풀링), 씬 전환으로 파괴되면 재생성, `reload()`
- [ ] 디버그 키 F5: 프리셋 다시 읽기 (`GameFramework::ProcessInput`)
- [ ] 프리셋 `hit_spark`(가산, 속도 늘림, 흰색→주황, 칼 방향 원뿔, 수명 0.15~0.3초, 30~50개), `skill_hit_spark`(더 크고 많게)
- [ ] 연결: `MainPlayerScript::on_trigger_enter`에서 `VfxManager::play(skill ? "skill_hit_spark" : "hit_spark", hit.point, hit.direction)`
- [ ] 검증: 평타·대검 적중 지점에서 칼 방향으로 스파크, F5로 Lua 값 수정이 바로 반영, 씬 전환 후에도 재생

### 마무리

- [ ] 문서: `ParticleSystem_Plan_KR.md`(결정·체크리스트 결과), `Client/Client/CODEMAP.md`(새 파일·렌더 경로·디버그 키 F5), 루트 `CODEMAP.md` 상태
- [ ] PathManager 합칠 때: 새 셰이더를 `Shaders/`로, 경로를 PathManager로 (합치는 시점에 별도 진행)

