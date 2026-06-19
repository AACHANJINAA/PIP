#include "stdafx.h"
#include "TainerScript.h"

#include "AnimationComponent.h"
#include "ReadGLTFMesh.h"
#include "ResourceManager.h"
#include "MonsterHPComponent.h"
#include "ObjectManager.h"
#include "UIManager.h"

void TainerScript::awake()
{
	auto hp_bar_obj = ObjectManager::instance()->find_by_name("Boss_HP_Bar");
	if (hp_bar_obj) _hpBar_ui = hp_bar_obj->get_component<UIRenderComponent>();

	NPCScript::awake();

	UIManager::instance()->set_visible(UILayer::BACKGROUND, "Boss_HP_Frame", true);
	UIManager::instance()->set_visible(UILayer::MIDDLE, "Boss_HP_Bar", true);

    game_object()->get_component<TransformComponent>()->set_local_scale({ 5.f,5.f ,5.f });
    auto hp = get_hp();
    CLOG("[TainerScript] Boss Initialization Complete.");
}

void TainerScript::init_visual()
{
    auto obj = game_object();
    // 1. 보스 크기 설정
    

    auto animComp = obj->get_component<AnimationComponent>();
    auto renderComp = obj->get_component<RenderComponent>();

    if (animComp && renderComp)
    {
        const std::string basePath = "Resource/Character/BoneGolem/";

        // 1. 메인 메쉬 로드 (이 파일은 반드시 메쉬 데이터를 포함해야 함)
        auto mainMesh = ResourceManager::instance()->load_mesh(basePath + "BoneGolemRd.gltf", true);
        ReadGLTFMesh* gltfMesh = static_cast<ReadGLTFMesh*>(mainMesh.get());
        //gltfMesh->set_shader_for_all_materials("skinned");

        // 2. 애니메이션만 별도로 로드하여 병합 (load_mesh 대신 load_animation_only 사용)
        gltfMesh->load_animation_only(basePath + "A_BoneGolem_Idle.gltf", "idle");
        gltfMesh->load_animation_only(basePath + "A_BoneGolem_Walk.gltf", "walk");
        gltfMesh->load_animation_only(basePath + "A_BoneGolem_Run.gltf", "run");
        gltfMesh->load_animation_only(basePath + "A_BoneGolem_Attack.gltf", "claw_right");
        gltfMesh->load_animation_only(basePath + "A_BoneGolem_Attack01.gltf", "claw_left");
		gltfMesh->load_animation_only(basePath + "A_BoneGolem_Attack02.gltf", "slam");
        gltfMesh->load_animation_only(basePath + "A_BoneGolem_Hit.gltf", "hit");
        gltfMesh->load_animation_only(basePath + "A_BoneGolem_Roar.gltf", "roar");
        gltfMesh->load_animation_only(basePath + "A_BoneGolem_Swim.gltf", "swim");
        gltfMesh->load_animation_only(basePath + "A_BoneGolem_Death.gltf", "death");


        renderComp->set_mesh(mainMesh);

        // 3. 상태별 애니메이션 매핑
        using namespace common::packet;
        animComp->add_animation("idle", mainMesh);
        animComp->add_animation("walk", mainMesh);
        animComp->add_animation("run", mainMesh);
        animComp->add_animation("claw_right", mainMesh);
        animComp->add_animation("claw_left", mainMesh);
        animComp->add_animation("slam", mainMesh);
        animComp->add_animation("swim", mainMesh);
        animComp->add_animation("hit", mainMesh);
        animComp->add_animation("roar", mainMesh);
        animComp->add_animation("death", mainMesh);

        CLOG("[TainerScript] BoneGolem Boss Visuals Settings Completed.");
    }

    // 4. 재질 및 쉐이더 설정 (Skinned Shader)
    std::string matName = "mat_tainer_boss_" + std::to_string(id());
    ResourceManager::instance()->create_material(matName);
    ResourceManager::instance()->set_shader_for_material(matName, "skinned");
    renderComp->set_pso_name("skinned");

    CLOG("[TainerScript] BoneGolem Boss Visuals Initialized.");
}

void TainerScript::update(float deltaTime)
{
    update_hp_bar(deltaTime);
    NPCScript::update(deltaTime);
    // 디버그 드로우 매니저를 통해 보스 머리 위에 현재 노드 이름 표시
    if (!_currentBTNodeName.empty()) {
        common::Vec3 headPos = position();
        headPos.y += 5.0f; // 보스 키만큼 올림
		CLOG("[TainerScript] Current BT Node: " << _currentBTNodeName);
    }
}

void TainerScript::handle_animation_branching()
{
	using namespace common::packet;
	auto anim_comp = game_object()->get_component<AnimationComponent>();
	switch (_state)
	{
	case EntityState::IDLE:
        anim_comp->play("idle");
		break;
    case EntityState::MOVE:
	    {
	        // 속도에 따라 걷기/달리기 분기 (예시에서는 단순히 걷기로 설정)
	        float speed = common::Length(_serverVel);
	        if (speed > 0.1f) {
	            anim_comp->play("walk");
	        }
	        else {
	            anim_comp->play("idle");
	        }
		}
	    break;
	case EntityState::ACTION:
		{
			// [방어 코드] Action 상태인데 actionId가 0이면 가장 최근의 공격 모션을 유지하거나 기본 공격 시도
			if (_actionId == 0) return; 

	        switch (_actionId)
	        {
	        case ActionID::Tainer::Charge:
	            anim_comp->play("swim");
				break;
	        case ActionID::Tainer::Roar:
				anim_comp->play("roar");
	            break;
            case ActionID::Tainer::Claw:
                anim_comp->play("claw_right", false);
				break;
	        case ActionID::Tainer::Slam:
                anim_comp->play("slam", false);
				break;
	        case ActionID::Tainer::Grab:
				anim_comp->play("claw_left", false);
				break;
			case ActionID::Tainer::GrabCharge:
				anim_comp->play("swim", true, 2.0f); // 돌진 연출
				break;
			case ActionID::Tainer::GrabCarry:
				anim_comp->play("claw_right", true, 1.5f); // 난타 연출
				break;
			case ActionID::Tainer::GrabSlam:
				anim_comp->play("slam", false, 0.8f); // 슬램 피니시
				break;
            default:
				// 알 수 없는 액션일 때만 로그를 찍고, 애니메이션을 강제로 바꾸지 않음
				static int lastUnknownId = -1;
				if (lastUnknownId != _actionId) {
					CLOG("[TainerScript] Unknown ActionID: " << _actionId);
					lastUnknownId = _actionId;
				}
                break;
	        }
		}
        break;
	case EntityState::HITTED:
        anim_comp->play("hit", false);
		break;
    case EntityState::DEAD:
        anim_comp->play("death", false);
		break;
    default:
		CLOG("[TainerScript] Unknown EntityState: " << static_cast<int>(_state));
        anim_comp->play("idle");
		break;
	}
}

void TainerScript::update_hp_bar(float deltaTime)
{
	if (!_hpBar_ui) return;
	auto hpComp = game_object()->get_component<MonsterHPComponent>();
	if (!hpComp) return;

	// 보간 로직 (MainPlayerScript 참고)
	float lerp = std::min(1.0f, deltaTime * 10.0f);
	_displayHp += (static_cast<float>(hpComp->get_current_hp()) - _displayHp) * lerp;
	// 보스가 DEAD 상태가 되면 HP 프레임과 바를 숨김
	if (_state == common::packet::EntityState::DEAD)
	{
		UIManager::instance()->set_visible(UILayer::BACKGROUND, "Boss_HP_Frame", false);
		UIManager::instance()->set_visible(UILayer::MIDDLE, "Boss_HP_Bar", false);
		_hpBar_ui = nullptr;
		return;
	}

	float ratio = _displayHp / static_cast<float>(hpComp->get_max_hp());
	_hpBar_ui->set_size_x(946.0f * ratio); // 946.0f는 초기 size와 동일
	_hpBar_ui->set_uv_scale(ratio, 1.0f);
	_hpBar_ui->set_uv_scale(ratio, 1.0f);
}