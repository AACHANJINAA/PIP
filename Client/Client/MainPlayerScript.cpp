#include "stdafx.h"
#include "MainPlayerScript.h"
#include "GameFramework.h"
#include "InputManager.h"
#include "NetworkManager.h"
#include "ObjectManager.h"
#include "RenderComponent.h"
#include "Renderer.h"

#include "ResourceManager.h"
#include "SceneManager.h"
#include "TerrainLoader.h"
#include "TransformComponent.h"
#include "TimerManager.h"
#include "AnimationComponent.h"

#include "LightManager.h"
#include "PhysicsColliderComponent.h"
#include "WeaponScript.h"
#include "MonsterHPComponent.h"

void MainPlayerScript::set_hp(int hp)
{
	_hp = hp;
	auto hp_ui = game_object()->get_component<MonsterHPComponent>();
	if (hp_ui) {
		hp_ui->set_current_hp(hp);
	}
}

void MainPlayerScript::update(float deltaTime)
{
    auto current_transform = this->transform();
    if (!current_transform) return;

	auto animation_comp = game_object()->get_component<AnimationComponent>();

    common::Vec3 move_direction{};
    bool is_moving = false;

    XMFLOAT3 camForward = _camera->transform()->forward();
    XMFLOAT3 camFwdV = Vector3::Normalize({ camForward.x, 0.0f, camForward.z });
    XMFLOAT3 camRightV = Vector3::Normalize(Vector3::CrossProduct({ 0, 1.f, 0 }, camFwdV));

    if (InputManager::instance()->IsKeyPress('W')) {
        move_direction = Vector3::Add(move_direction, camFwdV);
        is_moving = true;
    }
    if (InputManager::instance()->IsKeyPress('A')) {
        XMFLOAT3 playerLeft = Vector3::ScalarProduct(camRightV, -1.0f, false);
        move_direction = Vector3::Add(move_direction, playerLeft);
        is_moving = true;
    }
    if (InputManager::instance()->IsKeyPress('S')) {
        XMFLOAT3 modelBackward = Vector3::ScalarProduct(camFwdV, -1.0f, false);
        move_direction = Vector3::Add(move_direction, modelBackward);
        is_moving = true;
    }
    if (InputManager::instance()->IsKeyPress('D')) {
        move_direction = Vector3::Add(move_direction, camRightV);
        is_moving = true;
    }
    if (InputManager::instance()->IsKeyPress('Q')) {
        move_direction = Vector3::Add(move_direction ,common::Vec3Up);
        is_moving = true;
    }
    if (InputManager::instance()->IsKeyPress('E')) {
        move_direction = Vector3::Add(move_direction ,common::Vec3Down);
        is_moving = true;
	}

	if (InputManager::instance()->IsKeyPress(VK_SPACE))
	{
        NetworkManager::instance()->SendActionPacket(
            common::packet::ActionType::NORMAL_ATTACK,
            0, -1,
            current_transform->local_position(),
            current_transform->local_rotation()
        );
	}

    if (InputManager::instance()->IsKeyDown('P')) _speed += 1.f;
    if (InputManager::instance()->IsKeyDown('M')) if(_speed > 5.f) _speed -= 1.f;

    if (InputManager::instance()->IsKeyPress(VK_SPACE)) {
        if (_attackRangeObject) _attackRangeObject->get_component<PhysicsColliderComponent>()->set_active(true);
    } else {
        if (_attackRangeObject) _attackRangeObject->get_component<PhysicsColliderComponent>()->set_active(false);
    }
	
    // ---------------------------------------------------------
    // [넉백 물리 연산] 서버와 동일한 공식 (Friction 35.0f)
    // ---------------------------------------------------------
    float impactSpeed = common::Length(_impactVelocity);
    if (impactSpeed > 0.1f) {
        _impactVelocity = common::Normalize(_impactVelocity) * std::max(0.0f, impactSpeed - 35.0f * deltaTime);
    } else {
        _impactVelocity = {0,0,0};
    }

    auto anim_comp = game_object()->get_component<AnimationComponent>();
    common::Vec3 currentPos = current_transform->local_position();
    common::Vec3 new_pos = currentPos;

    if (is_moving) {
		if (anim_comp) anim_comp->set_state(common::packet::OBJECT_STATE::WALK);
        move_direction = common::Normalize(move_direction);
        new_pos = Vector3::Add(currentPos, Vector3::ScalarProduct(move_direction, _speed * deltaTime));

		if (const auto scene_manager = SceneManager::instance()) {
			if (const auto terrain_obj = scene_manager->get_terrain_object()) {
				if (auto rc = terrain_obj->get_component<RenderComponent>()) {
					if (auto terrain_loader = std::dynamic_pointer_cast<TerrainLoader>(rc->mesh())) {
                        new_pos.y = terrain_loader->get_height_at(new_pos.x, new_pos.z);
                    }
                }
            }
        }

        if (_camera && _camera->transform()) {
            XMFLOAT3 camF = _camera->transform()->forward();
            float yawDegrees = XMConvertToDegrees(atan2f(camF.x, camF.z)) - 180.f;
            current_transform->set_local_rotation(0.0f, yawDegrees, 0.0f);
        }
    } else if(InputManager::instance()->IsKeyPress(VK_SPACE)) {
        if (anim_comp) anim_comp->set_state(common::packet::OBJECT_STATE::ATTACK);
    } else {
        if (anim_comp) anim_comp->set_state(common::packet::OBJECT_STATE::IDLE);
    }

    // 넉백 이동량 합산
    new_pos.x += _impactVelocity.x * deltaTime;
    new_pos.z += _impactVelocity.z * deltaTime;

    current_transform->set_local_position(new_pos);

    // --- 20ms 주기 위치 전송 ---
    _sendTimer += deltaTime;
    if (_sendTimer >= SENDINTERVAL) {
        _sendTimer = 0.f;
        common::packet::OBJECT_STATE current_state = anim_comp ? anim_comp->get_state() : common::packet::OBJECT_STATE::IDLE;
        NetworkManager::instance()->SendMovePacket(current_transform->local_position(), current_transform->local_rotation(), current_state);
    }
}

void MainPlayerScript::awake()
{
    _camera = ObjectManager::instance()->find_by_name("FreeCamera").get();
    if (_camera) {
        XMFLOAT3 camF = _camera->transform()->forward();
        float yawDegrees = XMConvertToDegrees(atan2f(camF.x, camF.z)) - 180.f;
        transform()->set_local_rotation(0.0f, yawDegrees, 0.0f);
    }

    _attackRangeObject = ObjectManager::instance()->create_game_object("AttackRange");
    _attackRangeObject->transform()->set_parent(game_object()->transform());
    _attackRangeObject->transform()->set_local_position({ 0, 0, 0 });
    auto col = _attackRangeObject->add_component<PhysicsColliderComponent>();
    col->initialize(PhysicsColliderComponent::ShapeType::Sphere, { 3.0f, 0, 0 }, { 0,0,0 }, { 0,0,0 }, true);
    _attackRangeObject->add_component<WeaponScript>();
    col->set_active(false);
}

void MainPlayerScript::move_pos(common::packet::MOVE_TYPE cmd)
{
    // ... 기존 레거시 함수 유지 ...
}
