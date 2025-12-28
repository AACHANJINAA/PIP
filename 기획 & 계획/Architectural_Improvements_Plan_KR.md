# 프로젝트 "Slay The Lord" - 아키텍처 고도화 계획 (1월 로드맵)

이 문서는 현재 프로토타입을 확장성 있고, 데이터 중심적이며, 안정적인 액션 RPG 프레임워크로 변환하기 위한 아키텍처 개선 사항과 신규 기능을 정리한 문서입니다. 개발 생산성 향상, 게임 플레이의 깊이 확보, 네트워크 안정성 확보를 목표로 합니다.

---

## 1. 공통 (Client & Server)

### 1.1. 데이터 주도 설계 (Data-Driven Framework)
*   **목표:** 하드코딩된 값(스탯, ID, 파일 경로)을 제거하여 기획자가 코드를 수정하지 않고도 밸런스를 조정할 수 있게 함.
*   **실행 과제:**
    *   **JSON 데이터 구조:** `MonsterData.json`, `SkillData.json`, `ItemData.json` 스키마 정의.
    *   **공용 데이터 정의:** `StatInfo`, `SkillInfo` 등 공용 구조체를 `Common/` 디렉토리로 이동하여 공유.

### 1.2. 패킷 구조 확장
*   **목표:** 단순 이동과 기본 공격을 넘어 복잡한 액션 게임 플레이 지원.
*   **실행 과제:**
    *   **스킬 패킷:** `C2S_USE_SKILL`, `S2C_SKILL_RESULT` (범위, 방향, 타겟 정보 포함).
    *   **상태 패킷:** `OBJECT_STATE` 동기화를 세분화하여 동작의 전이 상태(예: `StartCast`, `Channeling`, `Impact`)까지 표현.

---

## 2. 클라이언트 아키텍처 (Client Architecture)

### 2.1. 핵심 시스템 (Core Systems)
*   **[New] DataManager:** JSON 게임 데이터를 로드하고 캐싱하는 싱글톤 매니저.
    *   *예시:* `DataManager::instance()->GetItem(1001).name`
*   **[New] SceneManager 리팩토링:** `LobbyScene`, `InGameScene` 등을 클래스로 명확히 분리하고, 리소스 로딩/해제 로직 자동화.
*   **[Upgrade] InputManager:** 키 코드를 직접 쓰는 대신 **액션 매핑(Action Mapping)** 시스템 도입 (예: `IsKeyDown('A')` 대신 `IsAction("Attack")`).

### 2.2. 시각 효과 및 피드백 (Visuals & Juice)
*   **[New] UIManager:** Orthographic Projection이나 SpriteBatch를 활용하여 2D HUD(체력바, 데미지 폰트, 스킬 아이콘) 렌더링.
*   **[New] ParticleSystem / EffectManager:** 오브젝트 풀링을 활용하여 애니메이션과 연동되는 시각 효과(검기, 폭발) 관리.
*   **[New] SoundManager:** 오디오 라이브러리(FMOD/XAudio2)를 래핑하여 BGM 및 3D 효과음 처리.

### 2.3. 애니메이션 및 로직
*   **[Upgrade] 애니메이션 상태 머신 (FSM):** 현재의 단순 상태 설정을 넘어, 전이 조건(Conditions)과 블렌딩(Blending)을 지원하는 FSM 클래스 구현.
*   **[New] DebugDraw / Gizmos:** 충돌체(Collider), 이동 경로, AI 인식 범위 등을 인게임에서 선으로 그려주는 디버깅 툴.

---

## 3. 서버 아키텍처 (Server Architecture)

### 3.1. 로직 및 AI
*   **[Upgrade] Behavior Tree (BT):** `Monster.lua`를 대체하는 고성능 C++ 기반 Behavior Tree 시스템 구축.
    *   *구성 요소:* `Selector`, `Sequence`, `Action`, `Condition` 노드.
*   **[New] Blackboard:** AI 에이전트들이 상태(타겟 ID, 마지막 위치, 체력 상태 등)를 기억하고 공유하는 메모리 공간.
*   **[New] 어그로 시스템 (Aggro System):** 데미지 및 위협 수준에 따라 공격 대상을 결정하는 `AggroTable` 구현 (레이드 기믹의 핵심).

### 3.2. 게임 플레이 시스템
*   **[New] SkillComponent:** 쿨타임, 자원 소모(MP/스태미나), 효과 실행을 담당하는 모듈형 시스템.
*   **[New] DataManager (Server):** 밸런스 데이터(몬스터 HP, 공격력, 드랍 테이블)를 로드하는 서버 측 매니저.
*   **[New] InventoryComponent:** 플레이어의 아이템, 장비, 소비 아이템을 관리하며 DB 저장/로드 연동 고려.

### 3.3. 물리 및 네트워크
*   **[Upgrade] 충돌 감지 (Collision Detection):** 단순 AABB를 넘어 정교한 히트박스(Capsule, Cylinder, Attack Sectors) 판정 구현. 지형이 복잡해질 경우 **Jolt Physics** 서버 도입 검토.
*   **[New] 공간 분할 (Spatial Partitioning):** Grid 또는 QuadTree를 도입하여 `Broadcast` 범위 및 충돌 검사를 최적화하고, 동접자 확장성 확보.
*   **[New] 서버 권한 (Server-Side Authority):** 모든 중요 연산(데미지, 위치 검증)을 서버에서 수행하여 핵 방지.

---

## 4. 구현 로드맵 (우선순위 순)

1.  **1단계: 기반 다지기 (데이터 & 구조)**
    *   `DataManager` 구현 (클라이언트 & 서버, JSON 연동).
    *   서버 C++ Behavior Tree 구조 완성.

2.  **2단계: 핵심 플레이 (액션)**
    *   `SkillComponent` 및 서버 측 히트 판정 구현.
    *   클라이언트 `UIManager` 추가 (체력/상태 피드백).

3.  **3단계: AI 심화 및 폴리싱**
    *   `AggroTable` 및 BT를 활용한 복합 몬스터 패턴 구현.
    *   `ParticleSystem` 및 사운드 추가로 타격감 강화.

4.  **4단계: 최적화 및 안정화**
    *   공간 분할(Spatial Partitioning) 적용.
    *   네트워크 예측/보간(Dead Reckoning) 로직 정교화.
