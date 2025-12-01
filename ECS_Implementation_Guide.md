# ECS + Jolt 구현 가이드

## 실제 코드 구현 예시

### 1. 기본 ECS 구조

#### Common/ECS/Entity.h
```cpp
#pragma once
#include <vector>
#include <memory>
#include <typeindex>
#include <unordered_map>

class Component;

class Entity {
private:
    static uint32_t nextId;
    uint32_t id;
    std::unordered_map<std::type_index, std::unique_ptr<Component>> components;

public:
    Entity() : id(++nextId) {}
    virtual ~Entity() = default;

    uint32_t getId() const { return id; }

    template<typename T, typename... Args>
    T* addComponent(Args&&... args) {
        auto component = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = component.get();
        component->setOwner(this);
        components[std::type_index(typeid(T))] = std::move(component);
        return ptr;
    }

    template<typename T>
    T* getComponent() {
        auto it = components.find(std::type_index(typeid(T)));
        if (it != components.end()) {
            return static_cast<T*>(it->second.get());
        }
        return nullptr;
    }

    template<typename T>
    void removeComponent() {
        components.erase(std::type_index(typeid(T)));
    }

    void update(float deltaTime);
    void fixedUpdate(float deltaTime);
};
```

#### Common/ECS/Component.h
```cpp
#pragma once

class Entity;

class Component {
protected:
    Entity* owner = nullptr;

public:
    virtual ~Component() = default;
    
    void setOwner(Entity* entity) { owner = entity; }
    Entity* getOwner() const { return owner; }

    virtual void awake() {}
    virtual void start() {}
    virtual void update(float deltaTime) {}
    virtual void fixedUpdate(float deltaTime) {}
    virtual void destroy() {}
};
```

### 2. 핵심 컴포넌트 구현

#### Common/Components/TransformComponent.h
```cpp
#pragma once
#include "../ECS/Component.h"
#include "../Math/Vector3.h"
#include "../Math/Quaternion.h"

class TransformComponent : public Component {
public:
    Vector3 position{0, 0, 0};
    Quaternion rotation{0, 0, 0, 1};
    Vector3 scale{1, 1, 1};

    Vector3 getWorldPosition() const { return position; }
    void setWorldPosition(const Vector3& pos) { position = pos; }
    
    void translate(const Vector3& delta) { position += delta; }
    void rotate(const Quaternion& rot) { rotation = rotation * rot; }
};
```

#### Common/Components/PhysicsComponent.h
```cpp
#pragma once
#include "../ECS/Component.h"
#include "../Math/Vector3.h"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/BodyID.h>

namespace JPH = Jolt::Physics;

class PhysicsSystem;

class PhysicsComponent : public Component {
private:
    JPH::BodyID bodyId;
    PhysicsSystem* physicsSystem = nullptr;
    bool isKinematic = false;

public:
    PhysicsComponent() = default;
    virtual ~PhysicsComponent();

    // 물리 바디 초기화
    void createRigidBody(PhysicsSystem* system, const Vector3& size, bool kinematic = false);
    
    // 위치 및 회전
    Vector3 getPosition() const;
    void setPosition(const Vector3& pos);
    
    // 힘과 속도
    void addForce(const Vector3& force);
    void setVelocity(const Vector3& velocity);
    Vector3 getVelocity() const;
    
    // 물리 속성
    void setMass(float mass);
    void setFriction(float friction);
    void setRestitution(float restitution);
    
    JPH::BodyID getBodyId() const { return bodyId; }
    bool getIsKinematic() const { return isKinematic; }
};
```

#### Common/Components/NetworkComponent.h
```cpp
#pragma once
#include "../ECS/Component.h"
#include "../Network/PacketTypes.h"

class NetworkComponent : public Component {
public:
    uint32_t networkId = 0;
    bool needsSync = false;
    float syncInterval = 0.1f; // 100ms
    float lastSyncTime = 0.0f;

    // 동기화할 데이터 타입
    enum SyncFlags {
        SYNC_POSITION = 1 << 0,
        SYNC_ROTATION = 1 << 1,
        SYNC_VELOCITY = 1 << 2,
        SYNC_HEALTH = 1 << 3
    };
    uint32_t syncFlags = SYNC_POSITION | SYNC_ROTATION;

    void markForSync() { needsSync = true; }
    bool shouldSync(float currentTime);
    
    // 패킷 직렬화
    void serialize(PacketWriter& writer);
    void deserialize(PacketReader& reader);
};
```

### 3. Jolt 물리 시스템

#### Common/Physics/JoltPhysicsSystem.h
```cpp
#pragma once
#include <Jolt/Jolt.h>
#include <Jolt/RegisterTypes.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <vector>

class PhysicsComponent;

class JoltPhysicsSystem {
private:
    JPH::PhysicsSystem* physicsSystem;
    JPH::TempAllocatorImpl* tempAllocator;
    JPH::JobSystemThreadPool* jobSystem;
    
    std::vector<PhysicsComponent*> physicsComponents;

public:
    JoltPhysicsSystem();
    ~JoltPhysicsSystem();

    bool initialize();
    void shutdown();
    
    void update(float deltaTime);
    void fixedUpdate(float fixedDeltaTime);
    
    // 컴포넌트 관리
    void registerComponent(PhysicsComponent* component);
    void unregisterComponent(PhysicsComponent* component);
    
    // 바디 생성
    JPH::BodyID createBox(const Vector3& position, const Vector3& size, bool isKinematic = false);
    JPH::BodyID createSphere(const Vector3& position, float radius, bool isKinematic = false);
    JPH::BodyID createCapsule(const Vector3& position, float height, float radius, bool isKinematic = false);
    
    void removeBody(JPH::BodyID bodyId);
    
    JPH::PhysicsSystem* getPhysicsSystem() { return physicsSystem; }
};
```

### 4. 게임 로직 컴포넌트

#### Common/Components/PlayerControllerComponent.h
```cpp
#pragma once
#include "../ECS/Component.h"
#include "../Input/InputTypes.h"

class TransformComponent;
class PhysicsComponent;

class PlayerControllerComponent : public Component {
private:
    float moveSpeed = 5.0f;
    float jumpForce = 10.0f;
    bool isGrounded = false;
    
    TransformComponent* transform = nullptr;
    PhysicsComponent* physics = nullptr;

public:
    void start() override;
    void update(float deltaTime) override;
    
    void handleInput(const InputState& input);
    void move(const Vector3& direction);
    void jump();
    
    // 설정
    void setMoveSpeed(float speed) { moveSpeed = speed; }
    void setJumpForce(float force) { jumpForce = force; }
};
```

#### Server/Components/AIComponent.h
```cpp
#pragma once
#include "../../Common/ECS/Component.h"
#include "../../Common/Math/Vector3.h"

class TransformComponent;
class PhysicsComponent;

enum class AIState {
    Idle,
    Patrol,
    Chase,
    Attack,
    ReturnToStart
};

class AIComponent : public Component {
private:
    AIState currentState = AIState::Idle;
    Vector3 startPosition;
    Vector3 targetPosition;
    float detectionRange = 10.0f;
    float attackRange = 2.0f;
    float moveSpeed = 3.0f;
    
    TransformComponent* transform = nullptr;
    PhysicsComponent* physics = nullptr;

public:
    void start() override;
    void update(float deltaTime) override;
    
    void setState(AIState newState);
    void setTarget(const Vector3& target);
    
    // AI 행동
    void patrol();
    void chaseTarget();
    void attackTarget();
    void returnToStart();
};
```

### 5. 시스템 관리자

#### Common/ECS/SystemManager.h
```cpp
#pragma once
#include <vector>
#include <memory>

class System;
class Entity;

class SystemManager {
private:
    std::vector<std::unique_ptr<System>> systems;
    std::vector<Entity*> entities;

public:
    template<typename T>
    T* addSystem() {
        auto system = std::make_unique<T>();
        T* ptr = system.get();
        systems.push_back(std::move(system));
        return ptr;
    }
    
    void addEntity(Entity* entity);
    void removeEntity(Entity* entity);
    
    void update(float deltaTime);
    void fixedUpdate(float deltaTime);
    
    template<typename T>
    T* getSystem() {
        for (auto& system : systems) {
            if (auto castedSystem = dynamic_cast<T*>(system.get())) {
                return castedSystem;
            }
        }
        return nullptr;
    }
};
```

### 6. 사용 예시

#### Server/PlayerEntity.cpp
```cpp
#include "PlayerEntity.h"
#include "../Common/Components/TransformComponent.h"
#include "../Common/Components/PhysicsComponent.h"
#include "../Common/Components/NetworkComponent.h"
#include "Components/PlayerDataComponent.h"

Entity* createPlayerEntity(const Vector3& spawnPos) {
    Entity* player = new Entity();
    
    // 기본 컴포넌트들
    auto transform = player->addComponent<TransformComponent>();
    transform->setWorldPosition(spawnPos);
    
    auto physics = player->addComponent<PhysicsComponent>();
    physics->createRigidBody(g_PhysicsSystem, Vector3(0.5f, 1.8f, 0.5f), false);
    physics->setMass(70.0f); // 70kg
    
    auto network = player->addComponent<NetworkComponent>();
    network->syncFlags = NetworkComponent::SYNC_POSITION | 
                        NetworkComponent::SYNC_ROTATION | 
                        NetworkComponent::SYNC_VELOCITY;
    
    // 게임 데이터
    auto playerData = player->addComponent<PlayerDataComponent>();
    playerData->setMaxHP(100);
    playerData->setCurrentHP(100);
    playerData->setLevel(1);
    
    return player;
}
```

#### Client/PlayerSetup.cpp
```cpp
Entity* setupClientPlayer(uint32_t networkId) {
    Entity* player = new Entity();
    
    // 기본 컴포넌트
    player->addComponent<TransformComponent>();
    player->addComponent<PhysicsComponent>(); // 클라이언트 예측용
    
    // 클라이언트 전용 컴포넌트
    player->addComponent<RenderComponent>();
    player->addComponent<PlayerControllerComponent>();
    player->addComponent<AnimationComponent>();
    
    // 네트워크
    auto network = player->addComponent<NetworkComponent>();
    network->networkId = networkId;
    
    return player;
}
```

이 가이드를 따라 단계별로 구현하면 안정적인 ECS + Jolt 시스템을 구축할 수 있습니다.