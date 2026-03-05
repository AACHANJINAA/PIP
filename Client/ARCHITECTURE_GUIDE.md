# 🎮 PIP Client Engine Architecture Guide (Gemini Reference)

이 문서는 PIP 클라이언트 엔진의 핵심 구조와 데이터 흐름을 빠르게 파악하기 위해 작성된 기술 가이드입니다.

## 1. Engine Core & Main Loop (`GameFramework.h/cpp`)
- **Heartbeat**: `GameFramework::FrameAdvance`가 매 프레임 엔진의 모든 시스템을 구동합니다.
- **Update Sequence**:
    1. `ProcessNetwork`: 수신 패킷 큐 처리 및 `ReplicationSystem` 업데이트.
    2. `ProcessInput`: 사용자 입력 수집 (`InputManager`).
    3. `update_game_logic`: `GameObject` 및 `Behavior` 업데이트.
    4. `update_physics`: Jolt Physics 시뮬레이션 (Fixed Time Step).
    5. **Render**: `Renderer::render` 호출.

## 2. DX12 Rendering Pipeline (`Renderer.h/cpp`, `Shader.h/cpp`)
- **Pipeline Management**: `Renderer`는 PSO(Pipeline State Object)를 이름별로 관리하며, `RootSignature.h`를 통해 용도별(Skinned, UI, Terrain 등) 루트 시그니처를 생성합니다.
- **Resource Binding**: `DescriptorManager`가 디스크립터를 할당하고, `Renderer::bind_texture_table`을 통해 텍스처를 동적으로 바인딩합니다.
- **Optimization**: `build_render_list`에서 프러스텀 컬링을 수행하여 가시 객체만 렌더링 큐에 삽입합니다.

## 3. Entity Component System (`GameObject.h`, `Component.h`)
- **Structure**: `GameObject`는 컴포넌트의 컨테이너이며, `ObjectManager`가 모든 엔티티의 생명주기를 관리합니다.
- **Smart Dependency**: 컴포넌트 정의 시 `required_components` 튜플을 선언하면 `add_component` 시 의존 컴포넌트가 자동으로 생성됩니다.
- **Behavior**: 게임 로직(FSM 등)은 `Behavior`를 상속받은 스크립트 컴포넌트에서 구현합니다.

## 4. Networking & Replication (`NetworkManager.h/cpp`, `ReplicationSystem.h/cpp`)
- **Multi-threading**: `NetworkManager`는 전용 스레드에서 패킷을 수신하여 `concurrent_queue`에 저장합니다.
- **Interpolation**: `ReplicationSystem`은 서버에서 온 `NetSnapshot`을 기반으로 `Dead Reckoning`과 `Lerp`를 결합하여 원격 객체의 움직임을 부드럽게 보간합니다.
- **Sync Interface**: 동기화가 필요한 객체는 `INetSync`를 상속받아 `apply_snapshot`을 구현합니다.

## 5. Animation & glTF (`ReadGLTFMesh.h/cpp`, `AnimationComponent.h/cpp`)
- **glTF Parser**: `ReadGLTFMesh`가 직접 JSON 및 바이너리를 파싱하여 메시, 본(Bone), 애니메이션 클립을 로드합니다.
- **Skinned Mesh**: 매 프레임 CPU에서 계산된 본 행렬 팔레트를 GPU 상수 버퍼로 업로드하여 셰이더에서 스키닝을 수행합니다.
- **Animation FSM**: `AnimationComponent`가 상태(IDLE, WALK 등)에 따라 메시와 클립을 관리합니다.

## 6. Authoritative Physics (`PhysicsManager.h/cpp`, `PhysicsCharacterControllerComponent.h/cpp`)
- **Jolt Integration**: 클라이언트에서도 Jolt 물리 월드를 운영하여 충돌 및 캐릭터 이동을 예측(Prediction)합니다.
- **Visual Correction**: 서버의 보정 패킷 수신 시 물리 위치는 즉시 동기화하되, `_visualOffset`을 사용하여 렌더링 위치는 부드럽게 보정하여 튐 현상을 방지합니다.

## 7. UI System (`UIRenderComponent.h/cpp`)
- **UI Rendering**: 화면 좌표계(Screen Space)를 사용하는 전용 셰이더와 `UIQuadMesh`를 통해 UI를 렌더링합니다.
- **Monster HP**: `MonsterHPComponent`는 3D 월드 상의 객체 위에 체력 바를 띄우기 위해 월드-스크린 좌표 변환을 지원합니다.

---
*이 가이드는 엔진의 기술적 설계를 요약한 것이며, 상세 구현은 각 소스 파일의 주석을 참조하십시오.*
