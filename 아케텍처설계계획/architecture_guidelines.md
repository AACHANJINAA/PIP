### \[개발 아키텍처 변경 지침] 상속 기반에서 컴포넌트 기반 설계로 전환 (v3)

**1. 배경 및 목적**

현재 우리 프로젝트는 상속을 통해 `GameObject`를 확장하고 있습니다. 이 방식은 구조가 경직되어 기능 추가 및 변경이 어렵고, 코드 재사용성이 떨어지는 문제가 있습니다.
이에, 최신 게임 개발의 표준인 \*\*컴포넌트 기반 아키텍처(Component-Based Architecture)\*\*로 전환하여 프로젝트의 유연성과 확장성을 극대화하고자 합니다.

**2. 핵심 원칙: "상속 대신 구성(Composition)을 사용하라"**

* **`GameObject`는 뼈대(Container)입니다:** `GameObject` 클래스 자체에는 위치, 회전 등 최소한의 데이터만 유지합니다. 더 이상 `GameObject`를 상속받아 `Player`, `Monster` 등을 만들지 않습니다.
* **기능은 부품(Component)입니다:** 모든 로직, 데이터, 기능은 독립적인 `Component` 클래스에 작성합니다.
* **조립(Assembly)을 통해 객체를 만듭니다:** `GameObject` 인스턴스를 생성한 뒤, 필요한 `Component` 인스턴스들을 부착하여 하나의 완전한 게임 객체를 '조립'합니다.

**3. 구현 가이드라인**

* **`Component` 기반 클래스:**

  * 모든 컴포넌트는 `Component` 베이스 클래스를 상속받아야 합니다.
  * `Component`는 자신을 소유한 `GameObject`에 대한 포인터(`owner\_game\_object`)를 가져야 합니다.

* **게임 로직은 '스크립트 컴포넌트'에 작성:**

  * 플레이어 조작, 몬스터 AI와 같은 구체적인 게임 로직은 `PlayerScript`, `MonsterAI` 등 `Component`를 상속받는 **스크립트 클래스**를 만들어 작성합니다.
  * 이 스크립트 안에서 `get\_component<T>()`를 호출하여 같은 오브젝트의 다른 컴포넌트(예: `PhysicsComponent`)와 상호작용하는 것이 핵심입니다.

> \*\*\[상세 설명]\*\*
    > `GameObject` 클래스 자체는 최대한 범용적으로 두고, '관리'나 '로직'은 각각의 컴포넌트(스크립트)에 위임하는 것입니다.
    >
    > 그리고 한 컴포넌트가 다른 컴포넌트의 기능이 필요할 때 `get\_component`를 사용해서 소통하는 것이 이 아키텍처의 핵심입니다.
    >
    > 예를 들어, `PlayerLogicComponent`의 `update` 함수는 이렇게 생길 수 있습니다.
    >
    > ```cpp
    > // PlayerLogicComponent.cpp
    > void PlayerLogicComponent::update(float delta\_time)
    > {
    >     // 1. 같은 게임오브젝트에 붙어있는 PhysicsComponent를 가져온다.
    >     PhysicsComponent\* physics = owner\_game\_object->get\_component<PhysicsComponent>();
    >
    >     // 2. 키보드 입력을 확인한다.
    >     if (input\_manager->is\_key\_down('W'))
    >     {
    >         // 3. 가져온 PhysicsComponent를 사용해 오브젝트에 힘을 가한다.
    >         physics->add\_force({0, 0, 1});
    >     }
    > }
    > ```
    > 이런 식으로 각 부품(컴포넌트)들이 독립적으로 자기 할 일을 하면서, 필요할 때만 서로의 기능을 빌려 쓰는 구조가 됩니다. 매우 유연하고 강력한 설계입니다.



* **`GameObject` 클래스 변경:**

  * `GameObject`는 `std::vector<Component\*>`와 같은 컨테이너로 자신의 컴포넌트 목록을 관리해야 합니다.
  * `update` 루프에서, `GameObject`는 자신이 가진 모든 컴포넌트의 `update`를 순차적으로 호출해야 합니다.
  * 컴포넌트 간 통신을 위해 `get\_component<T>()` 템플릿 함수를 반드시 구현해야 합니다.

**4. 실제 적용 예시: `Player` 객체 생성**

**AS-IS (현재 방식 - 상속):**

```cpp
// Player.h
class Player : public GameObject { ... };
```

**TO-BE (새로운 방식 - 구성):**

```cpp
// PlayerScript.h
class PlayerScript : public Component { ... }; // 플레이어 로직은 여기에!
// main.cpp
GameObject\* player\_object = new GameObject();
player\_object->add\_component(new RenderComponent());
player\_object->add\_component(new PlayerScript()); // 스크립트 컴포넌트 부착
player\_object->add\_component(new PhysicsComponent());
```

**5. 다음 단계 (Action Items)**

1. 모든 팀원은 위 내용을 숙지하고, 새로운 게임 객체는 반드시 컴포넌트 방식으로 구현합니다.
2. 기존의 상속 기반 클래스(`OtherPlayer` 등)를 점진적으로 새로운 컴포넌트 기반 아키텍처로 리팩토링합니다.
3. `GameObject`와 `Component` 베이스 클래스 설계를 우선적으로 진행합니다.
