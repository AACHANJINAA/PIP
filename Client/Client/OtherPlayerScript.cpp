#include "stdafx.h"
#include "OtherPlayerScript.h"
#include "gameobject.h"
#include "RenderComponent.h"
#include "ResourceManager.h"

void OtherPlayerScript::on_sync_position(const XMFLOAT3& newPosition)
{
    // 서버가 알려준 위치로 내 GameObject의 위치를 설정
    if (transform())
    {
        transform()->set_local_position(newPosition);
    }
}

void OtherPlayerScript::update(float deltaTime)
{
    // OtherPlayer는 클라이언트에서 직접 조작하지 않으므로,
    // 이 update 함수는 보통 애니메이션 갱신이나 보간(interpolation) 로직을 처리합니다.
}

void OtherPlayerScript::awake()
{
	// --- 이 스크립트가 부착된 GameObject에 필요한 모든 설정을 여기서 수행 ---
	auto render_comp = game_object()->add_component<RenderComponent>();
    render_comp->set_mesh(ResourceManager::Instance()->load_mesh("Resource/Monster/test_monster.obj"));
	render_comp->set_pso_name("default"); // 사용할 PSO 이름 지정
}
