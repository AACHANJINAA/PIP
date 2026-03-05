# 🎮 PIP (Slay The Lord) - Client Engine Reference

이 파일은 PIP 클라이언트 프로젝트의 구조, 주요 시스템, 빌드 방식 및 개발 가이드를 담고 있는 지침서입니다.

## 1. 프로젝트 개요 (Project Overview)
PIP(Slay The Lord) 클라이언트는 **DirectX 12** 기반의 자체 엔진으로 개발되는 3D 다크 판타지 액션 RPG입니다. 고성능 렌더링, 정밀한 물리 시뮬레이션, 그리고 서버 권위적(Server-Authoritative) 네트워크 동기화를 목표로 합니다.

- **핵심 기술 스택**: C++20, DirectX 12, Jolt Physics, Multi-threaded Networking, glTF 2.0
- **디자인 패턴**: Singleton, Component-Based Architecture (ECS-like), Behavior Tree, Observer

## 2. 엔진 아키텍처 (Engine Architecture)

### 2.1 메인 루프 및 시스템 제어 (`GameFramework`)
엔진의 심장부로, 매 프레임 다음 순서로 시스템을 업데이트합니다.
1. **Network**: `NetworkManager` 수신 큐 처리 및 `ReplicationSystem` 보간 업데이트.
2. **Input**: `InputManager`를 통한 사용자 입력 수집.
3. **Logic**: `ObjectManager`가 관리하는 모든 `GameObject` 및 `Behavior` 업데이트.
4. **Physics**: `PhysicsManager`를 통한 Jolt 물리 시뮬레이션 (Fixed Time-step).
5. **Render**: `Renderer`를 통한 DX12 렌더링 파이프라인 실행.

### 2.2 렌더링 시스템 (`Renderer`, `Shader`, `Mesh`)
- **PBR & IBL**: Cook-Torrance BRDF 기반의 물리 기반 렌더링 및 이미지 기반 라이팅 지원.
- **glTF 로더**: `ReadGLTFMesh`를 통해 외부 라이브러리(Assimp 등) 없이 직접 glTF/GLB 데이터를 파싱하여 메시 및 애니메이션 로드.
- **Resource Management**: `DescriptorManager`를 통한 효율적인 디스크립터 힙 관리 및 동기화.

### 2.3 네트워크 및 동기화 (`NetworkManager`, `ReplicationSystem`)
- **Multi-threaded Sync Networking**: 전용 네트워크 워커 스레드(`_networkThread`)에서 **Blocking 소켓**을 사용하여 데이터를 수신합니다.
- **Packet Queue**: 수신된 패킷은 `concurrent_queue`에 저장되어 메인 스레드에서 안전하게 소모됩니다.
- **Snapshot Interpolation**: 서버로부터 수신된 `NetSnapshot`을 기반으로 원격 객체의 위치와 회전을 부드럽게 보간합니다.
- **Client-Side Prediction & Correction**: 로컬 물리 엔진(Jolt)으로 즉각적인 이동을 예측하며, 서버의 위치와 오차가 발생할 경우 `sync_with_server`를 통해 보정합니다.

### 2.4 물리 엔진 (`PhysicsManager`)
- **Jolt Physics**: 서버와 동일한 물리 엔진을 사용하여 충돌 판정 및 캐릭터 컨트롤러(`PhysicsCharacterControllerComponent`) 구동.

## 3. 빌드 및 실행 (Building and Running)

### 3.1 요구 사항
- **OS**: Windows 10/11 (DX12 지원)
- **IDE**: Visual Studio 2022
- **SDK**: Windows 10 SDK (10.0.19041.0 이상), DirectX Agility SDK

### 3.2 빌드 방법
1. `Client.sln` 파일을 Visual Studio 2022로 엽니다.
2. 솔루션 구성을 `Debug` 또는 `Release`, 플랫폼을 `x64`로 설정합니다.
3. `F7` 또는 `Build -> Build Solution`을 눌러 컴파일합니다.

### 3.3 실행 방법
- 실행 시 서버 주소와 플레이어 이름을 입력하는 로그인 다이얼로그가 나타납니다.
- 기본 서버 주소는 `127.0.0.1`이며, 서버 프로젝트가 먼저 실행되어 있어야 정상적인 접속이 가능합니다.

## 4. 개발 규칙 및 컨벤션 (Development Conventions)

### 4.1 코딩 스타일
- **파일 인코딩**: 반드시 **UTF-8 with BOM**을 사용하십시오. (한글 주석 깨짐 방지)
- **명명 규칙 (Naming)**:
    - **Types (Class, Struct, Enum)**: `PascalCase` (예: `GameObject`, `TransformComponent`)
    - **Functions**: `snake_case` (예: `update_logic()`)
    - **Variables / Parameters**: `snake_case` (예: `delta_time`)
    - **Member Variables**: `_` 접두사 + `camelCase` (예: `_physicsManager`)
    - **Constants / Enums**: `ALL_CAPS_SNAKE_CASE` (예: `MAX_PLAYERS`)

### 4.2 메모리 관리
- 생포인터(`new`/`delete`) 사용을 지양하고 `std::unique_ptr`, `std::shared_ptr`를 사용하십시오.
- DX12 자원은 `Microsoft::WRL::ComPtr`을 사용하여 관리하십시오.

### 4.3 컴포넌트 추가 규칙
- 새로운 기능을 구현할 때는 `Component` 또는 `Behavior`를 상속받아 구현하십시오.
- `GameObject::add_component<T>()`를 사용하여 동적으로 컴포넌트를 추가할 수 있습니다.

---
*이 문서는 프로젝트의 변화에 따라 지속적으로 업데이트되어야 합니다.*
