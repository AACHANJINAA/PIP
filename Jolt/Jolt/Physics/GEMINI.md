# Jolt Physics - Core Physics Module (JPH::Physics)

이 디렉토리는 Jolt 물리 엔진의 핵심 시뮬레이션 로직과 객체 관리 시스템을 포함하고 있습니다. PIP 프로젝트에서는 이 모듈을 사용하여 서버 측의 정밀한 충돌 판정 및 캐릭터 이동을 처리합니다.

## 🏗️ Core Architecture

### 1. PhysicsSystem (`PhysicsSystem.h`)
물리 세계의 중앙 허브입니다. 모든 바디와 제약 조건을 관리하며 시뮬레이션 루프(`Update`)를 실행합니다.
- **BodyManager**: 모든 물리 객체의 생명주기를 관리합니다.
- **BroadPhase**: AABB 기반의 빠른 충돌 후보군 검색을 수행합니다.
- **NarrowPhaseQuery**: GJK/EPA 알고리즘을 사용한 정밀한 충돌 판정 및 레이캐스트를 수행합니다.
- **ContactConstraintManager**: 충돌 지점(Contact)의 물리적 반응을 계산합니다.
- **IslandBuilder**: 병렬 처리를 위해 독립적인 객체 그룹(Island)을 생성합니다.

### 2. Body System (`Body/`)
물리적 실체를 나타내는 핵심 클래스입니다.
- **Static**: 움직이지 않는 배경 객체.
- **Kinematic**: 속도에 의해 움직이며 물리 법칙(중력 등)의 영향을 직접 받지 않음 (PIP의 주요 NPC 방식).
- **Dynamic**: 중력과 힘의 영향을 받는 완전한 물리 객체.

### 3. Character Controller (`Character/`)
캐릭터 이동을 위한 고수준 인터페이스입니다.
- **CharacterVirtual**: PIP 프로젝트의 핵심 구성 요소입니다. 물리 엔진의 직접적인 시뮬레이션 대신 `ShapeCast`와 `Sweep`을 사용하여 이동하며, 벽 타기/계단 오르기 등의 복잡한 처리를 지원합니다.
- **Character**: 실제 Dynamic Body를 사용하는 캐릭터 컨트롤러입니다.

## ⚙️ Key Settings (`PhysicsSettings.h`)

물리 시뮬레이션의 정확도와 성능 사이의 균형을 조절하는 전역 설정입니다.
- `mNumVelocitySteps`: 속도 제약 조건 해결 반복 횟수 (기본 10).
- `mNumPositionSteps`: 위치 에러 수정 반복 횟수 (기본 2).
- `mBaumgarte`: 위치 에러 수정 강도.
- `mDeterministicSimulation`: 멀티스레드 환경에서도 동일한 결과를 보장하는 결정론적 시뮬레이션 여부 (PIP에서는 `true` 권장).

## 🛠️ PIP Project Integration Guidelines

### 1. Server-Authoritative NPCs
- 서버 부하를 최소화하기 위해 대부분의 NPC는 **Kinematic** 또는 **Static** 레이어로 설정합니다.
- 실제 이동은 `CharacterVirtual::Update`를 통해 직접 제어하며, 공격 판정 시에만 `ShapeCast`를 사용하여 정밀 충돌을 체크합니다.

### 2. CharacterVirtual Update Loop
`CharacterVirtual`은 `PhysicsSystem::Update`에서 자동으로 업데이트되지 않으므로, 서버 로직 스레드에서 수동으로 업데이트해야 합니다.
```cpp
// 예시: 캐릭터 업데이트 순서
character->SetLinearVelocity(desired_velocity);
character->Update(delta_time, gravity, broadphase_filter, object_filter, body_filter, shape_filter, temp_allocator);
```

### 3. Collision Filtering
- `ObjectLayer`와 `BroadPhaseLayer`를 엄격히 구분하여 불필요한 충돌 계산을 방지합니다.
- 센서(Trigger) 객체는 `SetIsSensor(true)`를 사용하여 물리 반응 없이 콜백만 받도록 설정합니다.

## 📝 Coding Conventions (Jolt Specific)

- **Namespace**: 모든 Jolt 코드는 `JPH` 네임스페이스 내에 존재합니다.
- **Memory**: `TempAllocator`를 사용하여 시뮬레이션 중 발생하는 일시적인 메모리 할당을 최적화합니다.
- **Types**: `Vec3`, `Quat`, `Mat44` 등 SIMD 최적화된 수학 타입을 사용합니다. (PIP 프로젝트의 `common::Vec3`와 변환 시 `PIP::Utils` 활용)

---
*이 문서는 Gemini CLI에 의해 자동 생성되었습니다. Jolt Physics 엔진의 깊은 이해와 안전한 코드 작성을 위한 가이드로 활용하십시오.*
