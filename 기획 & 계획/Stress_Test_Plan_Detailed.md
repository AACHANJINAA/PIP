# PIP 프로젝트: 대규모 서버 스트레스 테스트 및 봇 클라이언트 상세 개발 계획서

## 1. 개요 (Overview)
본 계획서는 PIP 프로젝트의 서버 가용성과 물리 엔진(Jolt Physics)의 안정성을 극한의 상황에서 검증하기 위한 상세 로드맵이다. 100개의 독립된 게임 룸(Room), 총 400명의 동시 접속자(Player), 그리고 전역에 배치된 10,000마리의 NPC가 존재하는 환경에서 서버의 성능 임계점을 파악하고 최적화 지점을 도출하는 것을 목적으로 한다.

---

## 2. 테스트 환경 및 목표 수치 (Target Metrics)

### 2.1. 테스트 규모
- **동시 접속자:** 400명 (Headless Bot Client 사용)
- **활성 룸 수:** 100개 (방당 4인 입실)
- **전체 NPC 수:** 10,000마리 (방당 100마리 AI 및 물리 시뮬레이션)
- **맵 오브젝트:** 방당 100개 이상의 OBB(Oriented Bounding Box) 기반 충돌체

### 2.2. 목표 성능 (SLA)
- **서버 틱 레이트(Tick Rate):** 초당 30회(33.3ms) 이상 유지 (최악의 상황에서도 20fps 하한선 방어)
- **물리 연산 부하:** `UpdatePhysics` 소요 시간이 전체 틱 타임의 40% 이하로 유지
- **응답 지연(RTT):** 봇 클라이언트 기준 평균 50ms 이내 (로컬 테스트 환경 기준)
- **메모리 안정성:** 12시간 연속 가동 시 메모리 누수 0% 및 `TempAllocator` 오버플로우 발생 0회

---

## 3. 봇 클라이언트(Headless Stress-Tester) 설계

### 3.1. 아키텍처 및 기술 스택
- **언어:** C++ 20 (서버 코드베이스와의 구조적 호환성 유지)
- **네트워크 라이브러리:** `Boost.Asio` (Proactor 패턴 기반의 고성능 비동기 I/O)
- **멀티스레딩:** 400개 세션을 관리하기 위한 I/O 스레드 풀(Thread Pool) 구성
- **경량화:** 렌더링, 사운드, 리소스 로딩을 배제한 순수 로직 패킷 송수신 모드

### 3.2. 인텔리전트 이동 시뮬레이션 (Movement AI)
단순한 랜덤 이동은 물리 엔진에 충분한 부하를 주지 못하므로 다음과 같은 이동 패턴을 구현한다.
1. **Wall-Sliding Pattern:** `ExportedServerData.json`에서 읽어온 OBB의 모서리나 벽면을 향해 지속적으로 이동하며 비비는 동작 (Jolt의 `CharacterVirtual` 연산 부하 극대화)
2. **Congestion Pattern:** 특정 좁은 구역에 다수의 봇이 모여 서로 밀쳐내며 이동 (Narrow-phase 충돌 쌍 폭증 유도)
3. **AOI Boundary Crossing:** 그리드 맵의 경계선을 반복해서 가로지르며 이동 (서버의 AOI Enter/Leave 패킷 처리 부하 유도)

---

## 4. 상세 테스트 시나리오 (Test Scenarios)

### 시나리오 1: Massive Ramp-up (폭발적 접속 부하)
- **방법:** 1분 이내에 400개의 세션을 순차적으로 로그인 및 방 입장 처리.
- **측정:** 
    - Accept() 루프의 병목 여부
    - 방 입장 시 `SpawnPlayer` 및 `SpawnNPC` 패킷이 유실 없이 전달되는지 확인
    - 초기 메모리 할당 스파이크 현상 점검

### 시나리오 2: Jolt Physics OBB Nightmare (물리 한계 테스트)
- **방법:** 모든 봇이 건물의 모서리(Corner)나 좁은 틈새에 끼어있는 상태로 초당 30회 이동 패킷 전송.
- **측정:** 
    - Jolt의 `Update` 함수 내부에서 발생하는 Narrow-phase 충돌 연산 시간
    - 접촉점(Contact Points)이 1,000개 이상 발생할 때 CPU 점유율 변화
    - 물리 동기화 패킷(`S2C_P_MOVE`)의 발생 빈도 및 대역폭 점유

### 시나리오 3: Combat Chaos (전투 판정 부하)
- **방법:** 모든 플레이어와 NPC가 서로 사거리 안에 들어오도록 유도한 뒤, 최대 빈도로 공격 액션 수행.
- **측정:** 
    - `GridMap` 검색 알고리즘의 성능 (주변 타겟 쿼리 비용)
    - `ShapeCast` 및 `ValidateHit` 판정 로직의 CPU 시간 점유
    - 피격 결과 패킷(`S2C_P_NPC_ATTACK`)의 브로드캐스트 부하

### 시나리오 4: Network Batching & AOI Stress
- **방법:** NPC 100마리가 한 화면에 보이는 위치에 플레이어 4명을 집결시키고, NPC들을 격렬하게 이동시킴.
- **측정:** 
    - `S2C_NPC_MOVE_BATCH` 패킷의 크기가 MTU(1500 byte)를 효율적으로 활용하는지 확인
    - AOI 필터링으로 인해 불필요한 패킷이 차단되는 비율(Optimization Ratio) 측정

---

## 5. 서버 측 성능 모니터링 인프라 (Server Monitoring)

테스트 결과의 객관성을 확보하기 위해 서버에 다음과 같은 모니터링 기능을 통합한다.

### 5.1. 정밀 타이머 (Granular Timing)
- `LogicThread` 내부에 스코프 기반 타이머 적용
- `UpdatePhysics()`, `UpdateLogics()`, `ProcessJobs()`, `BroadcastBatch()` 각각의 실행 시간을 마이크로초(μs) 단위로 기록 및 5초 평균 출력

### 5.2. Jolt 엔진 통계 (Physics Stats)
- `PhysicsSystem::GetStats()`를 통해 다음 지표 수집
    - `mNumBodies`: 전체 바디 수
    - `mNumActiveBodies`: 활성 상태 바디 수
    - `mNumContactPairs`: 충돌 검사 쌍 수
    - `mNumConstraints`: 물리 제약 조건 수

### 5.3. 네트워크 프로파일링
- 세션별 `BytesPerSecond`, `PacketsPerSecond` 측정
- `do_send` 큐의 대기 시간 및 드랍된 패킷 수 집계

---

## 6. 성공 및 실패 기준 (Success/Failure Criteria)

### 6.1. 성공 조건
- 400명 접속 유지 상태에서 1시간 이상 무정지 가동
- 서버 틱 타임 평균 33ms 이하 (최대 50ms 미만)
- 위치 보정(Reconcile) 패킷 발생률이 전체 이동 패킷의 5% 미만 (정상적인 클라이언트 기준)

### 6.2. 실패 조건 (즉시 조치 필요)
- 특정 방의 물리 엔진 업데이트 시간이 100ms를 초과할 경우 (물리 엔진 최적화 필요)
- `TempAllocator`의 `Can't allocate more memory` 오류 발생 (할당기 크기 조정 필요)
- 세션 연결이 10개 이상 동시에 끊기는 현상 발생 (네트워크 데드락 의심)

---

## 7. 향후 최적화 로드맵 (Optimization Roadmap)
스트레스 테스트 결과에 따라 다음 기술을 도입할 예정이다.
1. **Delta/Threshold Checking:** 유의미한 이동 변화가 없을 경우 패킷 전송 생략
2. **Actor Culling:** 플레이어가 보지 않는 방의 물리 시뮬레이션 빈도 낮춤 (LOD 시스템)
3. **Lock-Free Queue Optimization:** 작업 큐(JobQueue)의 경합을 줄이기 위한 알고리즘 개선

---
*문서 버전: 1.0 (2026-02-20)*
*작성자: Gemini CLI Agent (PIP Project Infrastructure Team)*
