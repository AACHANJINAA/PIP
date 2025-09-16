# 컴포넌트 기반 시스템 리팩토링 계획서

## 1. 목표
현재의 상속 기반 객체 구조를 유연하고 유지보수가 용이한 **컴포넌트 기반 아키텍처(Component-Based Architecture)**로 전환하여, 코드의 재사용성을 높이고 확장성을 확보하는 것을 목표로 한다.

---

## 2. 핵심 개념: GameObject 역할의 변화

리팩토링의 핵심은 `GameObject`의 역할을 명확하게 재정의하는 것입니다.

### `[BEFORE]` 현재의 GameObject
- **만능 객체 (God Object):** 위치, 회전, 크기 등 Transform 데이터는 물론, 메시, 머티리얼 같은 렌더링 데이터와 로직, 물리(중력), 입력 처리, 애니메이션 등 **모든 기능과 데이터를 직접 소유하고 처리**합니다.
- **경직된 상속 구조:** `MainPlayer`는 `GameObject`를 상속받아야만 그 기능을 쓸 수 있습니다. 만약 날아다니는 기능이 필요하다면 상속 구조가 복잡해지거나, 여러 클래스에 중복 코드를 작성해야 합니다.
- **단일 책임 원칙(SRP) 위배:** 하나의 클래스가 너무 많은 책임을 져서, 작은 변경이 다른 기능에 예상치 못한 영향을 줄 수 있습니다.

### `[AFTER]` 새로운 GameObject
- **컴포넌트 컨테이너 (Component Container):** 새로운 `GameObject`는 **아무런 기능도 가지지 않은 텅 빈 뼈대**가 됩니다. 이 객체의 유일한 역할은 자신을 구성하는 부품(컴포넌트)들의 목록을 관리하고, 생명주기(Update, Render 등)를 전달하는 것입니다.
- **조합(Composition)을 통한 기능 확장:** `GameObject`는 더 이상 상속으로 기능을 확장하지 않습니다. 대신, 필요한 기능들을 부품처럼 `AddComponent` 하여 **객체를 조립**합니다.
  - 예시: `Player` 객체 = `GameObject` + `TransformComponent` + `RenderComponent` + `PlayerInputComponent` + `HealthComponent`
- **단일 책임 원칙(SRP) 준수:** 각 컴포넌트는 '위치', '렌더링', '입력' 등 단 하나의 책임만 가집니다. 따라서 버그 수정이나 기능 추가 시 해당 컴포넌트만 집중하면 됩니다.

---

## 3. 리팩토링 순서도

안정적인 전환을 위해 아래 4단계 순서에 따라 점진적으로 진행합니다.

### Phase 1: 기반 시스템 구축 (Foundation Setup)
> 새로운 시스템의 뼈대를 만듭니다. 기존 코드는 그대로 둡니다.

1.  **`Component` 추상 클래스 생성 (`Component.h`)**
    - 모든 컴포넌트의 부모 클래스.
    - `GameObject* owner` 포인터를 멤버로 가짐.
    - `virtual void Update(float deltaTime)` 등 공통 인터페이스 정의.

2.  **새로운 `GameObject` 클래스 생성 (`GameObject.h`)**
    - 컴포넌트 목록(`std::vector<std::unique_ptr<Component>>`)을 관리.
    - `AddComponent`, `GetComponent<T>` 템플릿 함수 구현.
    - `Update` 함수는 모든 컴포넌트의 `Update`를 순차적으로 호출.

3.  **`TransformComponent` 생성 (`TransformComponent.h/.cpp`)**
    - 모든 객체의 기본인 위치, 회전, 크기 데이터를 관리.
    - 기존 `GameObject`의 Transform 관련 변수와 로직을 이전.

### Phase 2: 프로토타입 전환 (Prototype Conversion)
> 가장 간단한 객체를 먼저 전환하여 새로운 시스템의 동작을 검증합니다.

1.  **`RenderComponent` 생성 (`RenderComponent.h/.cpp`)**
    - 메시, 머티리얼 정보를 소유하고 렌더링 로직을 담당.
    - 기존 `GameObject::Render` 로직을 이전.

2.  **`BoardCube` 객체 재조립**
    - `Chess_Scene`에서 `new GameObject()`를 생성.
    - 생성된 객체에 `TransformComponent`와 `RenderComponent`를 추가하여 `BoardCube`의 기능을 하도록 조립.

3.  **검증**
    - 씬(Scene)에서 새로운 `BoardCube`가 기존과 동일하게 정상적으로 렌더링되는지 확인.

### Phase 3: 핵심 객체 전환 (Core Object Conversion)
> 입력, 게임 로직 등 복잡한 기능을 가진 객체를 전환합니다.

1.  **`HealthComponent` 생성 (`HealthComponent.h/.cpp`)**
    - 기존 `HPObject`의 기능을 이전.

2.  **`PlayerInputComponent` 생성 (`PlayerInputComponent.h/.cpp`)**
    - 기존 `MainPlayer::ProcessInput`의 입력 처리 로직을 이전.
    - `owner->GetComponent<TransformComponent>()`를 통해 `Transform`을 제어.

3.  **`MainPlayer` 객체 재조립**
    - `GameObject`에 `Transform`, `Render`, `Health`, `PlayerInput` 컴포넌트를 조립하여 새로운 `MainPlayer`를 생성.

4.  **기존 코드 제거**
    - 전환이 완료되면 기존 `MainPlayer`, `HPObject` 클래스를 삭제.

### Phase 4: 전체 시스템 통합 및 정리 (Full System Integration & Cleanup)
> 프로젝트의 모든 객체를 새로운 시스템으로 전환하고 마무리합니다.

1.  **관리자 클래스 수정 (`ObjectManager`, `Scene` 등)**
    - 새로운 `GameObject`를 생성하고 관리하도록 로직 수정.

2.  **`GameObjectFactory` 클래스 도입 (선택 사항)**
    - 객체 생성(컴포넌트 조립) 로직을 전담하는 팩토리 클래스를 만들어 코드 중앙화.

3.  **최종 정리**
    - 모든 객체 전환 후, 더 이상 사용되지 않는 구버전 `GameObject` 관련 클래스들을 프로젝트에서 완전히 삭제.

---

## 4. 기대 효과
- **모듈성:** 기능들이 독립적인 부품(컴포넌트)으로 분리됨.
- **재사용성:** `HealthComponent`는 플레이어, 적, 파괴 가능한 오브젝트 등 여러 곳에 재사용 가능.
- **유지보수성:** 버그 발생 시 문제의 원인이 되는 컴포넌트만 수정하면 되므로 디버깅이 용이.
- **확장성:** 새로운 기능을 추가할 때, 새로운 컴포넌트를 만들기만 하면 되므로 기존 코드에 미치는 영향이 최소화됨.
