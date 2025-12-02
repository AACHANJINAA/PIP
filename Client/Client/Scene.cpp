#include "stdafx.h"
#include "Scene.h"

#include "BoardCubeScript.h"
#include "ObjectManager.h"

#include "ResourceManager.h"
#include "Renderer.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "RenderComponent.h"

#include "json.hpp"
#include <fstream>


// load_scene_from_file load scene dataa from a JSON file

Scene::~Scene()
{
   
}

void Scene::load_scene_from_file(const std::string& filename, ID3D12Device* device,ID3D12GraphicsCommandList* commandList)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        CERROR("Failed to open scene file: " << filename);
        return;
    }

    nlohmann::json sceneJson;

    try
    {
        file >> sceneJson;
        file.close();
    }
    catch (const json::exception& e)
    {
        CERROR("Scene file load error: " << e.what());
        return;
    }

    std::filesystem::path basePath = std::filesystem::path(filename).parent_path();

    for (const auto& objectJson : sceneJson)
    {
        SceneObjectData data;
        data.name = objectJson.value("Name", "");
        data.meshFile = objectJson.value("MeshFile", "");

        if (data.meshFile.empty())
        {
            CLOG("Skipping object with empty MeshFile name: " << data.name);
            continue;
        }

        // 1. 메쉬 로드 (GLTF 파일 파싱 및 재질 정보 포함)
        std::string mesh_path = (basePath / data.meshFile).string();

        // 메쉬 로드 시, 메쉬 내부에서 GLTF 표준 재질 정보를 로드했다고 가정 
        std::shared_ptr<Mesh> mesh = ResourceManager::instance()->load_mesh(mesh_path); // device, commandList 인수가 필요할 수 있습니다.
        if (!mesh) {
            CLOG("Failed to load mesh : " << mesh_path);
            continue;
        }

        // 2. 게임 오브젝트 생성 및 컴포넌트 추가
        std::shared_ptr<GameObject> gameObject = ObjectManager::instance()->create_game_object(data.name);
        auto renderComp = gameObject->add_component<RenderComponent>();

        // 메쉬에 바인딩
        renderComp->set_mesh(mesh);

        // ★★★ MaterialOverrides 파싱 및 재질 생성 로직 제거 ★★★
        // 재질은 이제 mesh->get_materials() 또는 메쉬 로드 시점에 RenderComponent에 직접 설정되어야 합니다.
        // 현재 로직은 renderComp->set_mesh(mesh)가 메쉬 내부의 재질을 자동으로 바인딩한다고 가정합니다.

		// DW비동의 : 나는 위 주석에 동의할 수 없음 왜냐하면 npc는 고정된 재질이 필요하기 때문
        //          모든 생성은 하나도 통일하는 것이 좋을 것 같다.
        //          저번에 지형만 렌더링이 검정색으로 나온 경우가 있는 그것도 이 구조 때문이다.

        std::string material_name = "npc_material"; // player는 고정된 재질
        ResourceManager::instance()->create_material(material_name);
        ResourceManager::instance()->set_shader_for_material(material_name, "gltf");
        // gltf
        renderComp->set_pso_name("gltf");
       


        // 3. 트랜스폼 파싱 (기존과 동일)
        if (objectJson.contains("Transform")) {
            const auto& transformJson = objectJson["Transform"];
            auto transformComp = gameObject->transform();

            float offsetX = 0.0f;  // 필요시 조정
            float offsetY = 100.0f;
            float offsetZ = 0.0f;

            transformComp->set_local_position({
                transformJson["Location"].value("X", 0.0f) + offsetX,
                transformJson["Location"].value("Y", 0.0f) + offsetY,
                transformJson["Location"].value("Z", 0.0f) + offsetZ
                });
            transformComp->set_local_rotation(XMFLOAT4{
                transformJson["Rotation"].value("X", 0.0f),
                transformJson["Rotation"].value("Y", 0.0f),
                transformJson["Rotation"].value("Z", 0.0f),
                transformJson["Rotation"].value("W", 1.0f)
                }
            );
            transformComp->set_local_scale({
                transformJson["Scale"].value("X", 1.0f),
                transformJson["Scale"].value("Y", 1.0f),
                transformJson["Scale"].value("Z", 1.0f)
                });
        }

        
    }
    // 리소스 업로드 (메쉬 로드 시점에 텍스처 업로드도 포함된다고 가정)
    ResourceManager::instance()->upload_pending_meshes(device, commandList);
}
