# Slay The Lord (S.T.L) 기술 및 구조 계획서 (Technical & Architectural Plan)

본 문서는 S.T.L 프로젝트의 클라이언트(자체 DX12 엔진) 및 서버(IOCP 기반) 개발을 위한 절대적인 기술 지침 및 작업 기억(Working Memory) 문서입니다.

## 1. System Overview (시스템 개요)

### 1.2. 기술 스택 및 개발 환경

* **언어 & 표준**: C++ 17/20/23 (클라이언트/서버 공통, Modern C++ 지향)
* **그래픽스 API**: DirectX 12 (Windows Native)
* **물리 엔진**: Jolt Physics (서버 권위적 물리 및 충돌 처리)
* **네트워크**: Windows Socket (Winsock2), IOCP (I/O Completion Port) 비동기 모델
* **데이터베이스**: MSSQL (C++ Connector 이용)
* **데이터 포맷**: 세팅 및 테이블(JSON/CSV), 모델(GLTF/GLB), 통신(Custom Binary Packet)
* **형상관리 및 환경**: Git, Visual Studio 2022, Windows 10/11

---

## 2. Client Architecture (클라이언트 - 자체 엔진 설계)

### 2.1. DX12 렌더링 파이프라인 및 PBR 구현

* **렌더 패스(Render Pass) 구조**:

  1. **Shadow Pass**: Directional Light 기반의 Cascade Shadow Map(CSM) 생성.
  2. **Opaque Pass (PBR)**: GLTF 기반 메쉬 렌더링. Albedo, Normal, Metallic/Roughness(ORM) 맵을 활용한 Cook-Torrance BRDF 적용.
  3. **Alpha-Blend / Transparent Pass**: 파티클 및 반투명 객체 렌더링 (Z-Sorting 처리).
  4. **Post-Processing Pass**: Tone Mapping, Bloom 등 적용 후 최종 Backbuffer 출력.

* **Root Signature 및 Descriptor Heap**: 바인딩 병목을 최소화하기 위해 프레임별 Constant Buffer 링 버퍼 도입 및 효율적인 디스크립터 관리.

### 2.2. 리소스 매니저 (Resource Manager)

* **비동기 로딩**: `std::async` 또는 전용 `IO Thread`를 구성하여 씬 전환 및 동적 스폰 시 메인 스레드 블로킹(프레임 드랍) 방지. -> 계획
* **GLTF/GLB 파싱**: 언리얼 및 믹사모 등에서 뽑은 GLTF 메쉬와 애니메이션 데이터를 우리 클라이언트에 맞게 사용 할 수 있도록 파싱
* **텍스처 관리**: DDS 포맷을 선호하되, 파싱된 텍스처는 런타임에 WIC(Windows Imaging Component)를 통해 업로드.

### 2.3. 게임 루프 및 업데이트 모델 (ECS-like / Component 구조)

* **구조**: `Scene` 내에 `GameObject`들이 존재하며, 각 객체는 `Component`(Transform, MeshRenderer, Animator, 등)를 가짐.
* **업데이트 순서**:

  1. **Input**: Win32 Message 혹은 Raw Input 수집.
  2. **Network**: 패킷 수신 및 Dead Reckoning(추측 항법) 기반 보간 타겟 업데이트.
  3. **Logic (Update)**: 플레이어 상태 머신, 카메라 타겟 갱신.
  4. **Animation**: 애니메이션 블렌딩 및 본(Bone) 행렬 계산.
  5. **Render (Draw)**: 계산된 데이터를 바탕으로 Command List 작성 및 Execute.

---

## 3. Server Architecture (서버 설계)

### 3.1. IOCP 기반 스레드 모델

 1. 예약 (do_accept): 서버 시작 시 AcceptEx를 호출하여 비동기 접속 대기를 운영체제에 예약합니다.
 2. 완료 통지: 클라이언트가 접속하면, 기존의 IO_worker 스레드 중 하나가 GetQueuedCompletionStatus를 통해 접속 이벤트를 가로챕니다 (IO_ACCEPT 케이스).
 3. 재호출: 접속 처리가 끝나면 루프 내에서 다시 do_accept()를 호출하여 다음 접속을 예약합니다.
* **I/O Worker Threads**: IOCP 큐에서 완료된 I/O(Recv/Send) 이벤트를 처리하고 패킷을 조립하여 Logic Queue로 전달.
* **Logic / Room Threads (핵심)**:
  * 각 스테이지/방(Room) 별로 멀티스레딩 로드 밸런싱 적용.
  * 해당 스레드 내에서만 `Room` 객체의 데이터(플레이어, 몬스터, Jolt Physics Step)를 업데이트하여 **락(Lock) 없는 동기화(Lock-free like)** 지향. Task/Job Queue 패턴 사용.

* **DB Thread**: 비동기 DB 작업(저장/불러오기)을 전담하는 Task Queue 형태의 워커 스레드. -> 계획

### 3.2. 세션 및 패킷 처리

* **세션(Session)**: `Socket`, `RecvBuffer`(Ring Buffer), `SendQueue`로 구성.
* 조립된 완전한 패킷은 해당 유저가 속한 `Room`의 Thread-safe Job Queue에 던져져 로직 스레드에서 안전하게 처리됨.

### 3.3. 동기화 모델

* **위치 및 이동 (Dead Reckoning)**:

  * 클라이언트는 이동 시작/방향 변경/정지 시에만 패킷 전송.
  * 서버는 속도/방향으로 시뮬레이션 후 보정 위치를 주기적 브로드캐스트. 클라이언트는 Lerp/Slerp 보간.

* **액션 및 충돌 (서버 권위 - Server Authoritative)**:

  * Jolt Physics 기반. 클라이언트 공격 모션 시작 시 서버가 Hitbox 판정.
  * 타격 판정, 데미지 계산, 사망 처리는 100% 서버에서 결정.

* **보스 AI 상태 동기화**:

  * Behavior Tree/Lua 연동 AI는 서버에서 연산. 상태 전환(Phase, 특정 패턴 시작)만 클라이언트에 동기화.

---

## 4. Data Protocol \& Management (데이터 구조)

### 4.1. 주요 패킷 구조 정의 (Binary)

```cpp
#pragma pack(push, 1)
struct PacketHeader {
    uint16\_t size;
    uint16\_t id;
};

// 이동 동기화 패킷 (C->S / S->C)
struct Pkt_Move {
    Vector3 position;
    Vector3 direction;
    float speed;
};

// 스킬/공격 사용 패킷 (C->S)
struct Pkt_SkillAction {
    uint32_t skillId;
    Vector3 targetPosition_or_Direction;
};

// 동기화 상태 브로드캐스트 패킷 (S->C)
struct Pkt_SyncState {
    uint32_t entityId;
    Vector3 position;
    uint16_t animation_state_id;
};
#pragma pack(pop)
```

### 4.2. 데이터 테이블 적재 방식

* **포맷**: JSON (nlohmann/json 활용)
* **로딩 및 관리**: 서버 부팅 시 몬스터 스탯, 아이템 템플릿, 무기/스킬 계수 데이터를 메모리에 로드.
* 런타임 중에는 데이터 컨테이너를 **Read-Only**로 접근하여 별도의 Mutex Lock 없이 매우 빠르게 조회할 수 있도록 구성. -> 아직 구현안됨

---

## 5. Risk & R&D (기술적 위험 요소 및 해결 방안)

### 5.1. 자체 엔진 메모리 누수 및 리소스 관리

* **위험**: C++ 자체 엔진 특성상 객체 소멸 및 DX 자원 해제 누락 발생 가능성.
* **대책**: 모든 동적 할당에 `std::unique_ptr` / `std::shared_ptr` 적용. DX12 자원은 무조건 `Microsoft::WRL::ComPtr`을 사용하여 레퍼런스 카운팅 누수 원천 차단.

### 5.2. 멀티스레딩 데드락(Deadlock) 방지

* **위험**: Session과 Room 간 상호 참조 과정에서의 교착 상태.
* **대책**: 락 순서를 엄격하게 통제. 더 나아가, 비즈니스 로직 연산 시에는 가급적 락을 쓰지 않도록 `Room::PushJob(Lambda)` 형태의 **Message Passing** 기법을 적용하여 구조적으로 데드락을 방지.

### 5.3. 타격 판정 지연 및 불쾌감 (Lag Compensation)

* **위험**: 액션 게임에서 핑(Ping)으로 인해 공격이 빗나가는 현상.
* **대책 (R&D 목표)**: 서버에 `Rewind (과거 물리 상태 스냅샷 저장)` 시스템 구축. 클라이언트의 패킷 수신 시, 해당 클라이언트의 Ping만큼 서버 물리 월드를 과거로 되감아 정밀 타격 판정 수행.

### 5.4. PBR 렌더링 퍼포먼스

* **위험**: GLTF 씬 내 수많은 메쉬와 스킨드 애니메이션 연산 부하.
* **대책**: 다수의 몬스터 등장 시 Instancing 지원 및 Animation 연산을 GPU Compute Shader로 오프로딩하는 구조 고려.
