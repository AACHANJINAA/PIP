#include "stdafx.h"
#include "TainerScript.h"

#include "AnimationComponent.h"
#include "DebugDrawManager.h"
#include "ReadGLTFMesh.h"
#include "ResourceManager.h"
#include "MonsterHPComponent.h"

void TainerScript::awake()
{
	NPCScript::awake();
    game_object()->get_component<TransformComponent>()->set_local_scale({ 5.f,5.f ,5.f });
    auto hp = get_hp();
    game_object()->get_component<MonsterHPComponent>()->set_max_hp(get_hp());
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
            default:
				anim_comp->play("idle");
				CLOG("[TainerScript] Unknown ActionID: " << _actionId);
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

