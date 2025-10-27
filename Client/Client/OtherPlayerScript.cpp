#include "stdafx.h"
#include "OtherPlayerScript.h"
#include "gameobject.h"
#include "RenderComponent.h"
#include "ResourceManager.h"
#include "Renderer.h"
#include "GltfMaterial.h"

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
    auto render_comp = game_object()->add_component<RenderComponent>().get();
    auto playerMesh = ResourceManager::instance()->load_mesh("Resource/Character/BruteHi/bruteHi.gltf");

    // 재질 및 쉐이더 설정
    render_comp->set_mesh(playerMesh);
    auto material = std::make_shared<GltfMaterial>("test_Material");
    material->set_shader(Renderer::instance()->get_shader("gltf"));
    render_comp->set_material(material);

    // gltf
    render_comp->set_pso_name("gltf");

    // 위치, 회전 정보
    transform()->set_local_rotation(-90.f, 0.f, 0.f);
    transform()->set_local_scale({ 200.0f, 200.0f, 200.0f });
}
