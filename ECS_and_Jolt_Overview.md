# ECS와 Jolt 물리엔진 개념 설명

## ECS (Entity Component System)란?

### 1. ECS의 기본 개념

**ECS**는 게임 개발에서 사용되는 아키텍처 패턴입니다. 전통적인 상속 기반 설계의 문제점을 해결하기 위해 만들어졌습니다.

### 2. 전통적인 방식 vs ECS

#### 기존 상속 방식의 문제점:
```cpp
// 기존 방식 - 상속의 한계
class GameObject {
    Vector3 position;
    void update();
};

class Player : public GameObject {
    int hp;
    void attack();
};

class FlyingPlayer : public Player {
    void fly();  // 문제: 모든 Player가 날 필요는 없음
};

class SwimmingPlayer : public Player {
    void swim();  // 문제: 날면서 수영하는 Player는?
};
```

**문제점:**
- 다이아몬드 상속 문제
- 기능 조합의 어려움  
- 코드 중복
- 경직된 구조

#### ECS 방식의 해결책:
```cpp
// ECS 방식 - 조합을 통한 유연성
Entity player = createEntity();

// 필요한 기능만 조합해서 붙임
player.addComponent<TransformComponent>();  // 위치
player.addComponent<HealthComponent>();     // HP
player.addComponent<FlyComponent>();        // 날기
player.addComponent<SwimComponent>();       // 수영

// 원하는 조합 자유자재로 가능!
```

### 3. ECS의 3요소

#### **Entity (엔티티)**
- 게임 객체의 "컨테이너" 역할
- 단순히 고유 ID만 가짐
- 직접적인 데이터나 로직은 없음

```cpp
class Entity {
    uint32_t id;  // 단순히 식별자만
    std::vector<Component*> components;  // 컴포넌트들의 컨테이너
};
```

#### **Component (컴포넌트)**  
- 실제 "데이터"를 담는 부품
- 로직은 없고 순수 데이터만
- 레고 블럭처럼 조립 가능

```cpp
struct PositionComponent {
    float x, y, z;  // 위치 데이터만
};

struct HealthComponent {
    int currentHP;  // 체력 데이터만
    int maxHP;
};

struct VelocityComponent {
    float vx, vy, vz;  // 속도 데이터만
};
```

#### **System (시스템)**
- 실제 "로직"을 처리
- 특정 컴포넌트 조합을 가진 엔티티들을 찾아서 처리

```cpp
class MovementSystem {
public:
    void update() {
        // Position과 Velocity를 둘 다 가진 엔티티들 찾기
        for (Entity* entity : entities) {
            auto pos = entity->getComponent<PositionComponent>();
            auto vel = entity->getComponent<VelocityComponent>();
            
            if (pos && vel) {  // 둘 다 있으면
                pos->x += vel->vx * deltaTime;  // 이동 처리
                pos->y += vel->vy * deltaTime;
                pos->z += vel->vz * deltaTime;
            }
        }
    }
};
```

### 4. ECS의 실제 활용 예시

#### 다양한 게임 객체 만들기:

```cpp
// 1. 일반 플레이어 (걷기만 함)
Entity walkingPlayer = createEntity();
walkingPlayer.addComponent<PositionComponent>();
walkingPlayer.addComponent<HealthComponent>();
walkingPlayer.addComponent<WalkComponent>();

// 2. 날아다니는 플레이어
Entity flyingPlayer = createEntity();
flyingPlayer.addComponent<PositionComponent>();
flyingPlayer.addComponent<HealthComponent>();
flyingPlayer.addComponent<FlyComponent>();

// 3. 날면서 공격도 하는 플레이어
Entity combatFlyingPlayer = createEntity();
combatFlyingPlayer.addComponent<PositionComponent>();
combatFlyingPlayer.addComponent<HealthComponent>();
combatFlyingPlayer.addComponent<FlyComponent>();
combatFlyingPlayer.addComponent<AttackComponent>();

// 4. 심지어 플레이어가 아닌 객체도!
Entity flyingRock = createEntity();  // 날아다니는 바위
flyingRock.addComponent<PositionComponent>();
flyingRock.addComponent<FlyComponent>();
// HealthComponent는 없음 - 무적!
```

### 5. ECS의 장점

1. **극한의 유연성**: 레고처럼 조합 가능
2. **재사용성**: 컴포넌트를 다른 엔티티에서도 사용
3. **성능**: 데이터 지향적 설계로 캐시 효율성 높음
4. **유지보수**: 기능별로 분리되어 수정 용이
5. **확장성**: 새 컴포넌트 추가가 기존 코드에 영향 없음

---

## Jolt Physics란?

### 1. Jolt의 정체성

**Jolt Physics**는 게임용 고성능 물리엔진입니다.

- **개발사**: Guerrilla Games (Horizon 시리즈 개발사)
- **사용 게임**: Horizon Zero Dawn, Horizon Forbidden West
- **특징**: AAA급 성능, 오픈소스, 현대적 C++ 설계

### 2. 물리엔진이 하는 일

#### 물리 시뮬레이션 예시:
```cpp
// 물리엔진 없이 직접 구현하면?
void updatePlayer() {
    // 중력 적용
    velocity.y -= 9.8f * deltaTime;
    
    // 바닥 충돌 체크 (매우 복잡!)
    if (position.y < groundLevel) {
        position.y = groundLevel;
        velocity.y = 0;
    }
    
    // 벽 충돌 체크 (더욱 복잡!)
    // ... 수백 줄의 복잡한 수학 계산
}

// Jolt 사용하면?
void updatePlayer() {
    // 끝! Jolt가 모든 물리를 알아서 계산
    physicsWorld->step(deltaTime);
}
```

### 3. Jolt의 핵심 기능

#### **Rigid Body (강체)**
- 물리적으로 움직이는 객체
- 중력, 충돌, 마찰 등이 자동 적용

```cpp
// 플레이어 물리 바디 생성
JPH::BoxShapeSettings box(JPH::Vec3(0.5f, 1.0f, 0.5f));  // 크기
JPH::BodyCreationSettings bodySettings(&box, 
    JPH::Vec3(0, 10, 0),      // 위치
    JPH::Quat::sIdentity(),   // 회전
    JPH::EMotionType::Dynamic, // 동적 (움직임)
    LayerManager::MOVING);     // 충돌 레이어
```

#### **충돌 감지 (Collision Detection)**
```cpp
// A와 B가 충돌했을 때 자동 호출
class MyContactListener : public JPH::ContactListener {
public:
    virtual void OnContactAdded(const JPH::Body& body1, const JPH::Body& body2) {
        std::cout << "충돌 발생!" << std::endl;
        // 데미지 계산, 사운드 재생 등
    }
};
```

#### **힘과 충격 (Forces & Impulses)**
```cpp
// 플레이어 점프
bodyInterface.AddImpulse(playerId, JPH::Vec3(0, 500, 0));

// 지속적인 힘 (바람 등)
bodyInterface.AddForce(playerId, JPH::Vec3(10, 0, 0));
```

### 4. 다른 물리엔진과의 비교

| 엔진 | 성능 | 사용 편의성 | 라이센스 | 게임 사례 |
|------|------|------------|----------|-----------|
| **Jolt** | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐ | MIT (무료) | Horizon 시리즈 |
| Bullet | ⭐⭐⭐ | ⭐⭐ | Zlib (무료) | GTA V |
| PhysX | ⭐⭐⭐⭐ | ⭐⭐⭐ | BSD (무료) | Unreal Engine |
| Havok | ⭐⭐⭐⭐⭐ | ⭐⭐⭐⭐⭐ | 상용 (유료) | Skyrim, Fallout |

### 5. Jolt의 장점

1. **최고 수준 성능**: Horizon 게임에서 검증된 성능
2. **현대적 설계**: 멀티스레드, SIMD 최적화
3. **무료 오픈소스**: MIT 라이센스로 상업적 사용 가능  
4. **활발한 개발**: 지속적인 업데이트와 버그 수정
5. **좋은 문서화**: API 문서와 예제가 풍부

---

## ECS + Jolt 조합의 시너지

### 왜 ECS와 Jolt를 함께 쓰면 좋을까?

#### **1. 깔끔한 분리**
```cpp
// 물리는 PhysicsComponent가 담당
class PhysicsComponent : public Component {
    JPH::BodyID joltBodyId;  // Jolt 물리 바디
};

// 렌더링은 RenderComponent가 담당  
class RenderComponent : public Component {
    Mesh* mesh;
    Material* material;
};

// 게임 로직은 PlayerController가 담당
class PlayerControllerComponent : public Component {
    void handleInput();
};
```

#### **2. 유연한 물리 적용**
```cpp
// 물리가 필요한 객체만 PhysicsComponent 추가
Entity movingPlatform = createEntity();
movingPlatform.addComponent<TransformComponent>();
movingPlatform.addComponent<PhysicsComponent>();    // 물리 O
movingPlatform.addComponent<RenderComponent>();

// 정적 배경은 물리 없이
Entity background = createEntity();
background.addComponent<TransformComponent>();
// PhysicsComponent 없음 - 물리 X, 성능 절약!
background.addComponent<RenderComponent>();
```

#### **3. 물리-게임로직 분리**
```cpp
// PhysicsSystem: 순수 물리 계산만
class PhysicsSystem {
    void update() {
        joltWorld->step(deltaTime);  // 물리 계산
        
        // 물리 결과를 TransformComponent에 반영
        syncPhysicsToTransform();
    }
};

// GameLogicSystem: 게임 규칙만
class GameLogicSystem {
    void update() {
        // 체력이 0이면 사망 처리
        handleDeathLogic();
        
        // 아이템 수집 처리  
        handleItemCollection();
    }
};
```

### 실제 활용 예시

```cpp
// 1. 물리 기반 플레이어
Entity player = createEntity();
player.addComponent<TransformComponent>();
player.addComponent<PhysicsComponent>();      // Jolt 물리
player.addComponent<PlayerControllerComponent>();
player.addComponent<HealthComponent>();
player.addComponent<RenderComponent>();

// 2. 물리 기반 NPC (AI 추가)
Entity npc = createEntity();  
npc.addComponent<TransformComponent>();
npc.addComponent<PhysicsComponent>();         // Jolt 물리
npc.addComponent<AIComponent>();              // AI 행동
npc.addComponent<HealthComponent>();
npc.addComponent<RenderComponent>();

// 3. 물리 기반 아이템 (수집 가능)
Entity item = createEntity();
item.addComponent<TransformComponent>();
item.addComponent<PhysicsComponent>();        // Jolt 물리 (떨어짐)
item.addComponent<CollectibleComponent>();    // 수집 로직
item.addComponent<RenderComponent>();

// 4. UI 요소 (물리 없음)
Entity healthBar = createEntity();
healthBar.addComponent<TransformComponent>(); 
// PhysicsComponent 없음 - UI는 물리 불필요
healthBar.addComponent<UIRenderComponent>();
```

## 결론

- **ECS**: 레고블럭처럼 조합 가능한 유연한 게임 아키텍처
- **Jolt**: AAA급 성능의 현대적 물리엔진  
- **조합**: 깔끔하고 확장 가능하며 고성능인 게임 시스템

이 두 기술을 결합하면 복잡한 게임도 체계적이고 유지보수하기 쉬운 구조로 만들 수 있습니다!