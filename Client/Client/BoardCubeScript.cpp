#include "stdafx.h"
#include "BoardCubeScript.h"
#include "GameObject.h"
#include "ReadGlbMesh.h"
#include "RenderComponent.h"
#include "ResourceManager.h"

BoardCubeScript::BoardCubeScript()
{
}

void BoardCubeScript::awake()
{
    /*
      [철학 구현]
      프로그래머는 빈 GameObject에 이 스크립트 하나만 추가했습니다.
      이제 이 스크립트가 '스스로' 필요한 기능들을 GameObject에 추가하여
      자신을 완전한 '보드 큐브'로 만듭니다.
      */

      // 1. 렌더링 기능 추가 (스스로를 보이게 만들기)
    auto render_component = game_object()->add_component<RenderComponent>();

    // 2. 메쉬 설정 (자신의 모양 결정)
    // TODO: 메쉬를 동적으로 설정할 수 있도록 개선해야 합니다. 현재는 기본값을 사용합니다.
    auto board_mesh = ResourceManager::Instance()->load_mesh("Resource/MapData/SM_Crate_01.glb");
    render_component->set_mesh(board_mesh);

    // 3. 셰이더(PSO) 설정 (자신을 어떻게 그릴지 결정)
    // .glb 파일의 경우 "skinned" PSO를 사용하는 것이 일반적입니다.
    render_component->set_pso_name("skinned");
}

void BoardCubeScript::update(float delta_time)
{
    // 매 프레임 실행될 로직
}