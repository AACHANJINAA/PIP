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


void MainPlayerScript::update(float deltaTime)
{
    // --- 이동 입력 처리 및 로컬 위치 업데이트 ---
    auto current_transform = this->transform();
    if (!current_transform) return;

	auto animation_comp = game_object()->get_component<AnimationComponent>();

    // 모델의 -90도 X축 회전을 고려한 실제 forward 벡터 계산
    XMFLOAT3 modelForward = Vector3::ScalarProduct(current_transform->forward(), 1.f, false);    // 실제 앞 방향
    XMFLOAT3 modelRight = current_transform->right();   // 실제 오른쪽 방향
    common::Vec3 move_direction{};
    bool is_moving = false;

    XMFLOAT3 camForward = _camera->transform()->forward();
    XMFLOAT3 camFwdV = Vector3::Normalize({ camForward.x, 0.0f, camForward.z });
    XMFLOAT3 camRightV = Vector3::Normalize(Vector3::CrossProduct({ 0, 1.f, 0 }, camFwdV)); // 카메라 기준 오른쪽

    if (InputManager::instance()->IsKeyPress('W')) {
        // 플레이어의 앞쪽 방향으로 이동
        move_direction = Vector3::Add(move_direction, camFwdV);
        is_moving = true;
    }
    if (InputManager::instance()->IsKeyPress('A')) {
        // 플레이어의 왼쪽 방향으로 이동
        XMFLOAT3 playerLeft = Vector3::ScalarProduct(camRightV, -1.0f, false);
        move_direction = Vector3::Add(move_direction, playerLeft);
        is_moving = true;
    }
    if (InputManager::instance()->IsKeyPress('S')) {
        // 플레이어의 뒤쪽 방향으로 이동
        XMFLOAT3 modelBackward = Vector3::ScalarProduct(camFwdV, -1.0f, false);
        move_direction = Vector3::Add(move_direction, modelBackward);
        is_moving = true;
    }
    if (InputManager::instance()->IsKeyPress('D')) {
        // 플레이어의 오른쪽 방향으로 이동
        move_direction = Vector3::Add(move_direction, camRightV);
        is_moving = true;
    }
    // 나중에 점프로 변경
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
        // [수정] 범용 액션 패킷으로 변경
        NetworkManager::instance()->SendActionPacket(
            common::packet::ActionType::NORMAL_ATTACK, // 일반 공격
            0,                                         // action_id (평타는 0)
            -1,                                        // target_id (논타겟)
            current_transform->local_position(),       // 공격 발생 위치
            current_transform->local_rotation()        // 공격 방향
        );
	}

    if (InputManager::instance()->IsKeyDown('P'))
    {
        _speed += 1.f;
    }

    if (InputManager::instance()->IsKeyDown('M')) {
        if(_speed > 5.f)
        {
            _speed -= 1.f;
        }
    }

    Light* sunLight = LightManager::instance()->get_light(0);

    if (sunLight && sunLight->m_nType == 3) // 0번 조명이 방향성 조명인지 확인
    {
        // 'J' 키를 누르면 조명 각도를 Y축 기준으로 회전
        if (InputManager::instance()->IsKeyPress('J'))
        {
            sunLight->m_vDirection = XMFLOAT3(-0.8f, -0.4f, -0.2f);
        }
        // 'K' 키를 누르면 조명 각도를 특정 방향으로 즉시 변경
        if (InputManager::instance()->IsKeyPress('K'))
        {
            // (0.8f, -0.4f, 0.2f}는 동쪽 높은 곳에서 서쪽 낮은 곳을 비추는 형태입니다.
            sunLight->m_vDirection = XMFLOAT3(0.8f, -0.4f, 0.2f);
        }
    }
    if (InputManager::instance()->IsKeyPress(VK_SPACE))
    {
        // 스페이스 누르면 내 주변 3m 다 때림
        if (_attackRangeObject) {
            _attackRangeObject->get_component<PhysicsColliderComponent>()->set_active(true);
        }
    }
    else
    {
        if (_attackRangeObject) {
            _attackRangeObject->get_component<PhysicsColliderComponent>()->set_active(false);
        }
    }
	
    auto anim_comp = game_object()->get_component<AnimationComponent>();
    if (is_moving) {
		if (anim_comp)
		{
            anim_comp->set_state(common::packet::OBJECT_STATE::WALK);
		}
        move_direction = common::Normalize(move_direction); // 대각선 이동 시 속도가 빨라지지 않도록 정규화
        // _speed = 5.0f; // 이동 속도 (임의의 값, 필요시 조정)
        auto new_pos = Vector3::Add(current_transform->local_position() ,Vector3::ScalarProduct(move_direction, _speed * deltaTime));

        // [추가됨] 2. 지형 높이(Y) 보정 로직
		// 클라이언트에서 지형 높이 계산 로직을 서버와 동일하게 적용하여 예측 정확도를 높임.
		if (const auto scene_manager = SceneManager::instance()) {
			if (const auto terrain_obj = scene_manager->get_terrain_object()) {
				if (auto render_comp = terrain_obj->get_component<RenderComponent>()) {
                    auto mesh = render_comp->mesh();
                    // Mesh가 TerrainLoader인지 확인 후 캐스팅 (안전하게 dynamic_pointer_cast 사용)

					if (auto terrain_loader = std::dynamic_pointer_cast<TerrainLoader>(mesh)) {
                        // 해당 X, Z 위치의 정확한 지형 높이를 가져옴
                        float terrain_height = terrain_loader->get_height_at(new_pos.x, new_pos.z);

                        // CLOG("Pre-Height Y: " << new_pos.y << " | Terrain Y: " << terrain_height);
                        
						// 지형 아래로만 못가게, 위로는 자유롭게
                        new_pos.y = terrain_height;
                    }
                }
            }
        }

        if (_camera && _camera->transform()) {
            XMFLOAT3 camForward = _camera->transform()->forward();

            float yawRadians = atan2f(camForward.x, camForward.z);

            float yawDegrees = XMConvertToDegrees(yawRadians);
            yawDegrees -= 180.f;
            // 모델의 기본 -90도 X축 회전 유지하고, yaw만 카메라 방향으로 설정
			// DW수정 : 기존 코드는 X축 회전을 0으로 설정했으나, 모델의 기본 회전을 유지하도록 수정 애니메이션 적용되면 -90 안해도 문제 없음
            current_transform->set_local_rotation(0.0f, yawDegrees, 0.0f);
        }

    	current_transform->set_local_position(new_pos);
    }
    else if(InputManager::instance()->IsKeyPress(VK_SPACE))
    {
        
        // 애니메이션 패킷 전송 등...
        if (anim_comp) {
            anim_comp->set_state(common::packet::OBJECT_STATE::ATTACK);
        }
    }
    else
    {
        if (animation_comp)
        {
            anim_comp->set_state(common::packet::OBJECT_STATE::IDLE);
        }
    }

    // --- 50ms 마다 위치 정보 전송 ---
    _sendTimer += deltaTime;
    if (_sendTimer >= SENDINTERVAL) {
        _sendTimer = 0.f;
        common::packet::OBJECT_STATE current_state = animation_comp ? animation_comp->get_state() : common::packet::OBJECT_STATE::IDLE;
        NetworkManager::instance()->SendMovePacket(current_transform->local_position(), current_transform->local_rotation(), current_state);
    }
}

void MainPlayerScript::awake()
{
    _camera = ObjectManager::instance()->find_by_name("FreeCamera").get();

    if (_camera) {
        XMFLOAT3 camForward = _camera->transform()->forward();
        float yawRadians = atan2f(camForward.x, camForward.z);
        float yawDegrees = XMConvertToDegrees(yawRadians);
        yawDegrees -= 180.f; // 모델이 뒤를 보고 있다면 180도 회전 필요        
        transform()->set_local_rotation(0.0f, yawDegrees, 0.0f);
    }
    else {
        // 카메라가 없으면 기본 회전 (필요에 따라 조정)                        
        transform()->set_local_rotation(0.0f, 0.0f, 0.0f);
    }


    // [테스트] 플레이어 중심 공격 범위 (Attack Range) 생성
    _attackRangeObject = ObjectManager::instance()->create_game_object("AttackRange");
    _attackRangeObject->transform()->set_parent(game_object()->transform());
    _attackRangeObject->transform()->set_local_position({ 0, 0, 0 });

    auto col = _attackRangeObject->add_component<PhysicsColliderComponent>();
    // 반지름 3.0m짜리 거대한 구 (센서)
    col->initialize(PhysicsColliderComponent::ShapeType::Sphere, { 3.0f, 0, 0 }, { 0,0,0 }, { 0,0,0 }, true);

    // 로그 찍는 스크립트 붙이기
    _attackRangeObject->add_component<WeaponScript>();

    col->set_active(false); // 평소엔 끔

	/*_renderComponent = game_object()->add_component<RenderComponent>().get();
    auto playerMesh = ResourceManager::instance()->load_mesh("Resource/Character/BruteHi/bruteHi.gltf");

    // 재질 및 쉐이더 설정
    _renderComponent->set_mesh(playerMesh);

	// ResourceManager을 통해 재질 생성 및 셰이더 할당
    std::string material_name = "player_material";
	ResourceManager::instance()->create_material(material_name);
	ResourceManager::instance()->set_shader_for_material(material_name, "gltf_hp");

    // gltf
    _renderComponent->set_pso_name("gltf");

    // 위치, 회전 정보
    transform()->set_local_rotation(-90.f, 0.f, 0.f);
    transform()->set_local_scale({ 200.0f, 200.0f, 200.0f });*/
}

void MainPlayerScript::move_pos(common::packet::MOVE_TYPE cmd)
{
    common::Vec3 direction{};
    switch (cmd)
    {
    case common::packet::MOVE_TYPE::MOVE_UP:
        direction = common::Vec3Forward;
        break;
    case common::packet::MOVE_TYPE::MOVE_DOWN:
        direction = common::Vec3Backward;
        break;
    case common::packet::MOVE_TYPE::MOVE_RIGHT:
        direction = common::Vec3Right;
        break;
    case common::packet::MOVE_TYPE::MOVE_LEFT:
        direction = common::Vec3Left;
        break;
    default:
        return; // 정의되지 않은 타입이면 아무것도 하지 않음
    }

    this->game_object()->get_component<TransformComponent>()->set_local_position(
        {
            this->position().x + direction.x * TimerManager::instance()->GetTimeElapsed(),
            this->position().y + direction.y * TimerManager::instance()->GetTimeElapsed(),
            this->position().z + direction.z * TimerManager::instance()->GetTimeElapsed()
        }
	);
	// 서버로 이동 명령 전송은 정해진 타임에 전송되도록 함
}
