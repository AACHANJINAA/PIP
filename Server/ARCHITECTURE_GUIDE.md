# 🏰 PIP Server Architecture Guide (Gemini Reference)

이 문서는 PIP 서버의 핵심 구조와 로직 흐름을 빠르게 파악하기 위해 작성된 기술 가이드입니다.

## 1. Core Entry & Threading Model (`main.cpp`)
- **Entry Point**: `main.cpp`에서 `PhysicsManager`와 `Server` 인스턴스를 초기화하고 시작합니다.
- **Thread Model**: 
    - **I/O Workers**: IOCP 이벤트를 처리하며 `EXP_OVER` 구조체를 사용해 비동기 Recv/Send를 수행합니다.
    - **Logic Workers**: 각 `Room`의 물리 및 게임 로직 업데이트를 전담합니다. P-Core 친화도(Affinity) 설정이 적용되어 있습니다.

## 2. Networking Layer (`server.h/cpp`, `PacketManager.h/cpp`)
- **IOCP Server**: `Server` 클래스가 `AcceptEx`를 통해 연결을 수락하고 세션을 관리합니다.
- **Session Management**: `SESSION` 클래스는 세션 풀(`_session_pool`)을 통해 재사용되며, `shared_from_this()`를 사용해 I/O 중 객체 생존을 보장합니다.
- **Packet Dispatching**: `PacketManager`가 수신된 패킷 ID에 따라 `PacketHandlers.cpp`의 핸들러 함수로 분기합니다.

## 3. World & Simulation (`Room.h/cpp`, `GridMap.h/cpp`)
- **Room-based Partitioning**: 서버는 다수의 `Room`으로 나뉘어 있으며, 각 방은 독립적인 `Jolt Physics System`을 가집니다.
- **Spatial Partitioning**: `GridMap`을 사용해 AOI(Area of Interest)를 관리합니다. 플레이어 주변의 NPC만 활성화하거나 패킷을 전송하는 최적화 로직이 포함되어 있습니다.
- **Logic Loop**: `Room::UpdatePhysics` (물리 단계) -> `Room::UpdateLogics` (AI/상태 단계) 순으로 진행됩니다.

## 4. Entity & Component System (`GameObject.h`, `Actor.h`)
- **Base Structure**: `GameObject`는 컴포넌트의 컨테이너입니다.
- **Specialized Actors**:
    - `Player`: 세션과 연결된 플레이어 엔티티.
    - `NPC`: AI와 물리 컨트롤러를 가진 몬스터/NPC 엔티티.
- **Core Components**:
    - `TransformComponent`: 위치 및 회전 관리.
    - `CharacterControllerComponent`: Jolt `CharacterVirtual`을 이용한 물리 이동.
    - `PhysicsComponent`: 일반 물리 바디 관리.
    - `HitboxComponent`: 부위별 정밀 타격 판정 및 리와인드 검증.

## 5. Authoritative Physics & Combat (`PhysicsManager.h`, `JoltSetup.h`)
- **Jolt Integration**: 서버 권위적 물리 시뮬레이션을 수행합니다.
- **Hit Detection**: `Room::ExecuteActorAction`에서 공격 범위를 생성하고 `GridMap`으로 대상을 선별한 뒤, `HitboxComponent::CheckCollision`으로 최종 판정합니다.
- **Lag Compensation**: `Actor` 클래스는 `_history` 데큐를 통해 스냅샷을 저장하며, 공격 시점의 과거 위치로 되감아 판정하는 로직이 준비되어 있습니다.

## 6. AI System (`AIComponent.h`, `BehaviorTree.h`, `BT_Nodes.h`)
- **Hybrid AI**: `AIComponent`는 Lua 스크립트 또는 C++ Behavior Tree를 선택적으로 사용할 수 있습니다.
- **Behavior Tree**: `BTBuilder`를 통해 `Selector`, `Sequence`, `Action`, `Condition` 노드를 조합하여 복잡한 패턴을 정의합니다. (`NPC::SetupBT`)

## 7. Data & Common (`MapDataManager.h`, `Common/`)
- **Map Data**: `MapDataManager`가 JSON에서 지형 및 OBB 충돌체 데이터를 로드합니다.
- **Shared Definitions**: `Common/Packet.h`에 클라이언트와 공유하는 패킷 구조체가 정의되어 있습니다.

---
*이 가이드는 서버의 기술적 뼈대를 요약한 것이며, 구버전일수도 있으므로 상세 구현은 각 소스 파일의 주석을 참조하십시오.*
