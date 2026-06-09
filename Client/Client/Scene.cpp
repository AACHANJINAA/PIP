#include "stdafx.h"
#include "Scene.h"

#include "ObjectManager.h"

#include "ResourceManager.h"
#include "Renderer.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "RenderComponent.h"
#include "InstancedRenderComponent.h"

#include "json.hpp"
#include <fstream>


// load_scene_from_file load scene dataa from a JSON file

Scene::~Scene()
{
   
}

void Scene::on_scene_loaded()
{
}

void Scene::load_scene_from_file(const std::string& filename, ID3D12Device* device,ID3D12GraphicsCommandList* commandList, bool IsTitle)
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
            CERROR("Failed to load mesh : " << mesh_path);
            continue;
        }

        // 2. 게임 오브젝트 생성 및 컴포넌트 추가
        std::shared_ptr<GameObject> gameObject = ObjectManager::instance()->create_game_object(data.name);
        auto renderComp = gameObject->add_component<RenderComponent>();

        // 메쉬에 바인딩
        renderComp->set_mesh(mesh);


        std::string material_name = "npc_material"; // player는 고정된 재질
        ResourceManager::instance()->create_material(material_name);
        ResourceManager::instance()->set_shader_for_material(material_name, "gltf");
        // gltf
        renderComp->set_pso_name("gltf");

        // 3. 트랜스폼 파싱 (기존과 동일)
        if (objectJson.contains("Transform")) {
            const auto& transformJson = objectJson["Transform"];
            auto transformComp = gameObject->transform();


            transformComp->set_local_position({
                transformJson["Location"].value("X", 0.0f),
                transformJson["Location"].value("Y", 0.0f),
                transformJson["Location"].value("Z", 0.0f)
                });
            transformComp->set_local_rotation(XMFLOAT4{
                transformJson["Rotation"].value("X", 0.0f),
                transformJson["Rotation"].value("Y", 0.0f),
                transformJson["Rotation"].value("Z", 0.0f),
                transformJson["Rotation"].value("W", 1.0f)
                });
            transformComp->set_local_scale({
                transformJson["Scale"].value("X", 1.0f),
                transformJson["Scale"].value("Y", 1.0f),
                transformJson["Scale"].value("Z", 1.0f)
                });
        }

        if (IsTitle)
        {
            renderComp->set_culling_distance(500.0f);
        }
        else
        {
            if (filename.find("Landscape_-1_0") != std::string::npos) renderComp->set_culling_distance(200.0f);
            else  renderComp->set_culling_distance(300.0f);
        }
        Renderer::instance()->register_static_object(gameObject);
    }
}

void Scene::load_foliage_from_file(const std::string& filename, ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        CERROR("Failed to open foliage file: " << filename);
        return;
    }

    nlohmann::json rootJson;
    try
    {
        file >> rootJson;
        file.close();
    }
    catch (const json::exception& e)
    {
        CERROR("Foliage file load error: " << e.what());
        return;
    }

    std::filesystem::path basePath = std::filesystem::path(filename).parent_path();

    // 1. MeshLibrary 파싱 및 메쉬 미리 로드
    std::unordered_map<std::string, std::shared_ptr<Mesh>> loadedMeshes;

    if (rootJson.contains("MeshLibrary"))
    {
        for (auto& [meshName, meshInfo] : rootJson["MeshLibrary"].items())
        {
            std::string meshFile = meshInfo.value("MeshFile", "");
            if (meshFile.empty()) continue;

            std::string mesh_path = (basePath / meshFile).string();
            std::shared_ptr<Mesh> mesh = ResourceManager::instance()->load_mesh(mesh_path);

            if (mesh) {
                loadedMeshes[meshName] = mesh;
                CLOG("Loaded Foliage Mesh: " << meshName);
            }
            else {
                CERROR("Failed to load foliage mesh : " << mesh_path);
            }
        }
    }

    // 2. FoliageGroups 파싱 및 인스턴싱 객체 생성
    if (rootJson.contains("FoliageGroups"))
    {
        for (const auto& groupJson : rootJson["FoliageGroups"])
        {
            std::string meshName = groupJson.value("MeshName", "");
            if (loadedMeshes.find(meshName) == loadedMeshes.end()) continue;

            std::shared_ptr<Mesh> instancedMesh = loadedMeshes[meshName];

            // 폴리지 그룹당 GameObject는 딱 1개만 생성합니다.
            std::shared_ptr<GameObject> groupObject = ObjectManager::instance()->create_game_object(meshName + "_InstancedGroup");

            // 주의: 기존 RenderComponent 대신 인스턴싱을 지원하는 컴포넌트가 필요합니다.
            // (예: InstancedRenderComponent)
            auto renderComp = groupObject->add_component<InstancedRenderComponent>();
            renderComp->set_frustum_culling_enabled(false);
            renderComp->set_mesh(instancedMesh);

            std::string material_name = meshName + "_mat";
            ResourceManager::instance()->create_material(material_name);
            ResourceManager::instance()->set_shader_for_material(material_name, "gltf_instanced"); // 인스턴싱 전용 셰이더!
            renderComp->set_pso_name("gltf_instanced");

            // 3. 트랜스폼 배열 파싱 (Instance Buffer에 들어갈 데이터)
            std::vector<XMMATRIX> instanceTransforms; // D3D12 버퍼로 넘길 행렬 배열

            if (groupJson.contains("Transforms"))
            {
                for (const auto& transformJson : groupJson["Transforms"])
                {
                    // 언리얼에서 넘겨준 Pos, Rot, Scale 파싱 (키 이름이 줄어들었음에 주의)
                    XMFLOAT3 pos = {
                        transformJson["Pos"].value("X", 0.0f),
                        transformJson["Pos"].value("Y", 0.0f),
                        transformJson["Pos"].value("Z", 0.0f)
                    };

                    XMFLOAT4 rot = {
                        transformJson["Rot"].value("X", 0.0f),
                        transformJson["Rot"].value("Y", 0.0f),
                        transformJson["Rot"].value("Z", 0.0f),
                        transformJson["Rot"].value("W", 1.0f)
                    };

                    XMFLOAT3 scale = {
                        transformJson["Scale"].value("X", 1.0f),
                        transformJson["Scale"].value("Y", 1.0f),
                        transformJson["Scale"].value("Z", 1.0f)
                    };

                    // SRT 행렬 조립
                    XMVECTOR vPos = XMLoadFloat3(&pos);
                    XMVECTOR vRot = XMLoadFloat4(&rot);
                    XMVECTOR vScale = XMLoadFloat3(&scale);

                    XMMATRIX worldMatrix = XMMatrixAffineTransformation(vScale, XMVectorZero(), vRot, vPos);
                    instanceTransforms.push_back(worldMatrix);
                }
            }

            // 파싱한 행렬 배열을 인스턴싱 컴포넌트에 넘겨줍니다.
            // 내부적으로 D3D12 StructuredBuffer 또는 InstanceBuffer를 생성하게 끔 구현하셔야 합니다.
            renderComp->set_instance_data(instanceTransforms);

            Renderer::instance()->register_static_object(groupObject);

            CLOG("Created Foliage Group: " << meshName << " with " << instanceTransforms.size() << " instances.");
        }
    }
}

void Scene::load_from_file_with_light(const std::string& filename, ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
	std::ifstream file(filename);
	if (!file.is_open()) {
		CERROR("Failed to open scene file with light: " << filename);
		return;
	}

	nlohmann::json sceneJson;
	try {
		file >> sceneJson;
		file.close();
	}
	catch (const json::exception& e) {
		CERROR("Scene file load error: " << e.what());
		return;
	}

	std::filesystem::path basePath = std::filesystem::path(filename).parent_path();

	for (const auto& objectJson : sceneJson)
	{
		std::string name = objectJson.value("Name", "");
		std::string meshFile = objectJson.value("MeshFile", "");

		if (meshFile.empty()) continue;

		// 1. 메쉬 로드 및 오브젝트 생성
		std::string mesh_path = (basePath / meshFile).string();
		std::shared_ptr<Mesh> mesh = ResourceManager::instance()->load_mesh(mesh_path);
		if (!mesh) continue;

		std::shared_ptr<GameObject> gameObject = ObjectManager::instance()->create_game_object(name);
		auto renderComp = gameObject->add_component<RenderComponent>();
		renderComp->set_mesh(mesh);

		// 재질 및 셰이더 설정 (기존 로직 유지)
		std::string material_name = "scene_object_material";
		ResourceManager::instance()->create_material(material_name);
		ResourceManager::instance()->set_shader_for_material(material_name, "gltf");
		renderComp->set_pso_name("gltf");

		// 2. 트랜스폼 적용
		XMFLOAT3 pos = { 0, 0, 0 };
		if (objectJson.contains("Transform")) {
			const auto& transformJson = objectJson["Transform"];
			auto transformComp = gameObject->transform();

			pos = {
				transformJson["Location"].value("X", 0.0f),
				transformJson["Location"].value("Y", 0.0f),
				transformJson["Location"].value("Z", 0.0f)
			};
			transformComp->set_local_position(pos);
			transformComp->set_local_rotation(XMFLOAT4{
				transformJson["Rotation"].value("X", 0.0f),
				transformJson["Rotation"].value("Y", 0.0f),
				transformJson["Rotation"].value("Z", 0.0f),
				transformJson["Rotation"].value("W", 1.0f)
				});
			transformComp->set_local_scale({
				transformJson["Scale"].value("X", 1.0f),
				transformJson["Scale"].value("Y", 1.0f),
				transformJson["Scale"].value("Z", 1.0f)
				});
		}

		// 3. 해당 위치에 Point Light 추가
		Light spotLight;
		spotLight.m_bEnable = true;
		spotLight.m_nType = 2; // SPOT_LIGHT (LightManager.cpp의 #define SPOT_LIGHT 2)

		// 1. 위치: 메쉬보다 한참 위로 올립니다. (예: 300~500 정도)
		spotLight.m_vPosition = { pos.x, pos.y + 7.0f, pos.z };

		// 2. 방향: 위에서 아래로 정직하게 쏩니다.
		spotLight.m_vDirection = { 0.0f, -1.0f, 0.0f };

		// 3. 색상 및 강도: 티가 확 나도록 강도를 높입니다.
		spotLight.m_cDiffuse = { 2.0f, 4.0f, 2.0f, 1.0f };

		// 4. 조명 범위 및 감쇄
		spotLight.m_fRange = 800.0f;
		spotLight.m_vAttenuation = { 1.0f, 0.001f, 0.0001f }; // 먼 거리까지 빛이 전달되도록 아주 낮게 설정

		// 5. [중요] Spot Light 각도 설정 (cos값으로 넣어야 함)
		// m_fTheta: 안쪽 원 (빛이 가장 센 구간)
		// m_fPhi: 바깥쪽 원 (빛이 사라지기 시작하는 경계 구간)
		spotLight.m_fTheta = cosf(XMConvertToRadians(15.0f)); // 15도 안쪽은 풀 파워
		spotLight.m_fPhi = cosf(XMConvertToRadians(30.0f));   // 30도 밖은 빛 없음
		spotLight.m_fFalloff = 1.0f; // 중심에서 경계로 갈수록 흐려지는 정도

		LightManager::instance()->add_light(std::move(spotLight));
	}
}
