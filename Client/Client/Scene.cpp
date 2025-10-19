#include "stdafx.h"
#include "Scene.h"

#include "BoardCubeScript.h"
#include "ObjectManager.h"

#include "ResourceManager.h"
#include "Renderer.h"
#include "GameObject.h"
#include "TextureManager.h"
#include "TransformComponent.h"
#include "RenderComponent.h"
#include "GltfMaterial.h"

#include "json.hpp"
#include <fstream>


// load_scene_from_file load scene dataa from a JSON file
void Scene::load_scene_from_file(const std::string& filename, ID3D12Device* device,
    ID3D12GraphicsCommandList* commandList)
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

        // 메쉬 로드
        std::string mesh_path = (basePath / data.meshFile).string();
        std::shared_ptr<Mesh> mesh = ResourceManager::Instance()->load_mesh(mesh_path);
        if (!mesh) {
            CLOG("Failed to load mesh : " << mesh_path);
            continue;
        }

        // 게임 오브젝트 생성 및 컴포넌트 추가
        std::shared_ptr<GameObject> gameObject = ObjectManager::Instance()->create_game_object(data.name);
        auto renderComp = gameObject->add_component<RenderComponent>();
        renderComp->set_mesh(mesh);

        // MaterialOverrides 파싱 및 재질 생성
        if (objectJson.contains("MaterialOverrides"))
        {
            const auto& overridesArray = objectJson["MaterialOverrides"];

            if (overridesArray.size() == 1)
            {
                auto material = std::make_shared<GltfMaterial>(data.name + "_Material");
                material->set_shader(Renderer::Instance()->get_shader("gltf"));
                const auto& overrides = overridesArray[0];

                if (overrides.contains("baseColorTexture")) {
                    std::string textureFile = overrides["baseColorTexture"];
                    std::string texture_path = (basePath / textureFile).string();
                    auto texture = TextureManager::Instance()->load_texture(texture_path, commandList);
                    if (texture) material->set_texture(texture, 0);
                }

                if (overrides.contains("normalTexture")) {
                    std::string textureFile = overrides["normalTexture"];
                    std::string texture_path = (basePath / textureFile).string();
                    auto texture = TextureManager::Instance()->load_texture(texture_path, commandList);
                    if (texture) material->set_texture(texture, 1);
                }

                if (overrides.contains("ormTexture")) {
                    std::string textureFile = overrides["ormTexture"];
                    std::string texture_path = (basePath / textureFile).string();
                    auto texture = TextureManager::Instance()->load_texture(texture_path, commandList);
                    if (texture) material->set_texture(texture, 2);
                }

                if (overrides.contains("emissiveTexture")) {
                    std::string textureFile = overrides["emissiveTexture"];
                    std::string texture_path = (basePath / textureFile).string();
                    auto texture = TextureManager::Instance()->load_texture(texture_path, commandList);
                    if (texture) material->set_texture(texture, 3);
                }
                renderComp->set_material(material);
            }
            else if (overridesArray.size() > 1)
            {
                std::vector<std::shared_ptr<GltfMaterial>> materials;

                for (const auto& overrideItem : overridesArray)
                {
                    auto material = std::make_shared<GltfMaterial>(data.name + "_Material");
                    material->set_shader(Renderer::Instance()->get_shader("gltf"));

                    // overrideItem은 {"baseColorTexture", "path"} 형태의 객체

                    for (auto& [key, val] : overrideItem.items())
                    {
                        std::string textureFile = val.get<std::string>();
                        std::string texture_path = (basePath / textureFile).string();
                        auto texture = TextureManager::Instance()->load_texture(texture_path, commandList);

                        if (texture)
                        {
                            if (key == "baseColorTexture") material->set_texture(texture, 0);
                            else if (key == "normalTexture") material->set_texture(texture, 1);
                            else if (key == "ormTexture") material->set_texture(texture, 2);
                            else if (key == "emissiveTexture") material->set_texture(texture, 3);
                        }
                    }
                    materials.push_back(material);
                }
                renderComp->set_materials(materials);
            }
        }

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
                }
            );
            transformComp->set_local_scale({
                transformJson["Scale"].value("X", 1.0f),
                transformJson["Scale"].value("Y", 1.0f),
                transformJson["Scale"].value("Z", 1.0f)
            });
        }
    }
    ResourceManager::Instance()->upload_pending_meshes(device, commandList);
}

//Scene::Scene()
//{
//
//}
//
//Scene::~Scene()
//{
//}
//
//bool Scene::OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
//{
//
//
//    switch (nMessageID)
//    {
//    case WM_LBUTTONDOWN: 
//        GameObject* m_pLockedObject;
//
//        m_pLockedObject = PickObjectPointedByCursor(LOWORD(lParam), HIWORD(lParam));
//
//        if (m_pLockedObject)
//        {
//            m_pLockedObject->_isCollided = true;
//        }
//
//        break;
//    }
//
//    return(false);
//}
//
//bool Scene::OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam)
//{
//    return(false);
//}
//
//void Scene::LoadSceneFromFile(const std::string& filename, ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
//{
//    std::ifstream file(filename);
//    if (!file.is_open()) {
//        std::cerr << "Fatal Error: Failed to open scene file: " << filename << std::endl;
//        return;
//    }
//
//    try
//    {
//        // 2. JSON �Ľ�
//        json sceneJson;
//        file >> sceneJson; // �� �κп��� JSON ������ �ƴϸ� ���� �߻�
//        file.close();
//
//        // 3. JSON �迭 ��ȸ
//        for (const auto& objectData : sceneJson)
//        {
//            // .at() �Լ��� ����ϸ� Ű�� ���� �� ���ܰ� �߻��Ͽ� catch���� ó�� ����
//            std::string name = objectData.at("Name");
//            std::string meshName = objectData.at("Mesh");
//
//            // Transform ���� �Ľ�
//            const auto& transformData = objectData.at("Transform");
//            XMFLOAT3 location = JsonHelper::ParseVector3(transformData.at("Location"));
//            XMFLOAT3 rotation = JsonHelper::ParseRotation(transformData.at("Rotation"));
//            XMFLOAT3 scale = JsonHelper::ParseVector3(transformData.at("Scale"));
//
//            // �ؽ�ó ���� �Ľ� (�������� �� �����Ƿ� .contains�� Ȯ��)
//            if (objectData.contains("Textures")) {
//                for (const auto& texturePath : objectData["Textures"]) {
//                    // TODO: �ؽ�ó ��� ó�� ����
//                }
//            }
//            // TODO: ���� ������Ʈ ���� �� ��ġ ����
//
//            std::shared_ptr<GameObject> Board = std::make_shared<BoardCube>();
//            std::shared_ptr<Mesh> BoardMesh;
//            meshName = "Resource/MapData/" + meshName + ".glb";
//
//            auto Board_Transform = Board->get_component<TransformComponent>();
//            auto Board_Render = Board->get_component<RenderComponent>();
//
//            BoardMesh = make_shared<ReadGlbMesh>(pd3dDevice, pd3dCommandList, meshName, (Scene*)this);
//
//            if (!BoardMesh || !BoardMesh->IsValid())
//                continue;
//
//            if (Board_Render)
//            {
//                auto materialShader = std::make_shared<Material_Shader>();
//                Board_Render->set_material(materialShader);
//
//                Board_Render->CreateShaderVariables(pd3dDevice, pd3dCommandList); // ��� ���� ���� ���� �߰�
//                Board_Render->set_mesh(BoardMesh);
//                Board_Render->set_shader(_AllShaders[1]); // GLB
//                Board_Render->get_material_shader()->set_shader_root_signature(_AllRootSignature[1].Get());
//            }
//
//            if (Board_Transform)
//            {
//                Board_Transform->rotate(rotation.x, -rotation.y, rotation.z);
//                Board_Transform->set_scale(scale.x, scale.y, scale.z);
//                Board_Transform->set_position(location.x, location.y, location.z);
//            }
//            ObjectManager::Instance()->PushFloorObject(Board);
//
//        }
//    }
//    catch (const json::exception& e)
//    {
//        // JSON �Ľ� �Ǵ� ������ ���� �� �߻��ϴ� ��� ������ ������ ���⼭ ó��
//        std::cerr << "Scene file load error: " << e.what() << std::endl;
//        return;
//    }
//}
//
//// (����) ���͸��� + ���� �Ķ�����߰�
//ID3D12RootSignature* Scene::CreateGraphicsRootSignature(ID3D12Device* pd3dDevice)
//{
//    ID3D12RootSignature* pd3dGraphicsRootSignature = NULL;
//    D3D12_ROOT_PARAMETER pd3dRootParameters[4];
//    // [����] 0�� �Ķ����: ���� ��Ŀ� CBV
//    pd3dRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
//    pd3dRootParameters[0].Descriptor.ShaderRegister = 0; // b0
//    pd3dRootParameters[0].Descriptor.RegisterSpace = 0;
//    pd3dRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
//
//    // [����] 1�� �Ķ����: ī�޶�� CBV
//    pd3dRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
//    pd3dRootParameters[1].Descriptor.ShaderRegister = 1; // b1
//    pd3dRootParameters[1].Descriptor.RegisterSpace = 0;
//    pd3dRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
//
//    // ���͸��� ������ ���� ��� ���� ��(CBV) �߰�
//    pd3dRootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
//    pd3dRootParameters[2].Descriptor.ShaderRegister = 2; // ���̴��� b2 ��������
//    pd3dRootParameters[2].Descriptor.RegisterSpace = 0;
//    pd3dRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
//
//    // ���� ������ ���� ��� ���� ��(CBV) �߰�
//    pd3dRootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
//    pd3dRootParameters[3].Descriptor.ShaderRegister = 3; // ���̴��� b3 ��������
//    pd3dRootParameters[3].Descriptor.RegisterSpace = 0;
//    pd3dRootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
//
//    D3D12_ROOT_SIGNATURE_FLAGS d3dRootSignatureFlags =
//        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
//        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
//        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
//        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
//    // �� �κп� �ȼ� ���̴� ���پȵǰ� �ϴ°� ����
//
//    D3D12_ROOT_SIGNATURE_DESC d3dRootSignatureDesc;
//    ::ZeroMemory(&d3dRootSignatureDesc, sizeof(D3D12_ROOT_SIGNATURE_DESC));
//    d3dRootSignatureDesc.NumParameters = _countof(pd3dRootParameters);
//    d3dRootSignatureDesc.pParameters = pd3dRootParameters;
//    d3dRootSignatureDesc.NumStaticSamplers = 0;
//    d3dRootSignatureDesc.pStaticSamplers = NULL;
//    d3dRootSignatureDesc.Flags = d3dRootSignatureFlags;
//
//    ComPtr<ID3DBlob> pd3dSignatureBlob;
//    ComPtr<ID3DBlob> pd3dErrorBlob;
//
//    ::D3D12SerializeRootSignature(&d3dRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pd3dSignatureBlob, &pd3dErrorBlob);
//    pd3dDevice->CreateRootSignature(0, pd3dSignatureBlob->GetBufferPointer(),
//        pd3dSignatureBlob->GetBufferSize(), __uuidof(ID3D12RootSignature), (void**)&pd3dGraphicsRootSignature);
//
//    return(pd3dGraphicsRootSignature);
//}
//
//ID3D12RootSignature* Scene::CreateSkinnedGraphicsRootSignature(ID3D12Device* pd3dDevice)
//{
//    D3D12_ROOT_SIGNATURE_DESC d3dRootSignatureDesc;
//    ::ZeroMemory(&d3dRootSignatureDesc, sizeof(D3D12_ROOT_SIGNATURE_DESC));
//    d3dRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
//
//    // 1. �ؽ�ó(SRV)�� ���� ��ũ���� ���̺� ����
//    D3D12_DESCRIPTOR_RANGE d3dDescriptorRanges[1];
//    d3dDescriptorRanges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
//    d3dDescriptorRanges[0].NumDescriptors = 1; // �ؽ�ó�� 1��
//    d3dDescriptorRanges[0].BaseShaderRegister = 0; // ���̴��� t0 �������Ϳ� ����
//    d3dDescriptorRanges[0].RegisterSpace = 0;
//    d3dDescriptorRanges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
//
//    // 2. ���̴��� ����� ��ü �Ķ���� ����� ���� (�� 6��)
//    D3D12_ROOT_PARAMETER d3dRootParameters[6];
//
//    // [����] 0�� �Ķ����: ���� ��Ŀ� CBV
//    d3dRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
//    d3dRootParameters[0].Descriptor.ShaderRegister = 0; // b0
//    d3dRootParameters[0].Descriptor.RegisterSpace = 0;
//    d3dRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
//
//    // [����] 1�� �Ķ����: ī�޶�� CBV
//    d3dRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
//    d3dRootParameters[1].Descriptor.ShaderRegister = 1; // b1
//    d3dRootParameters[1].Descriptor.RegisterSpace = 0;
//    d3dRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
//
//    d3dRootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
//    d3dRootParameters[2].Descriptor.ShaderRegister = 2; // b2: ����
//    d3dRootParameters[2].Descriptor.RegisterSpace = 0;
//    d3dRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
//
//    d3dRootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
//    d3dRootParameters[3].Descriptor.ShaderRegister = 3; // b3: ����
//    d3dRootParameters[3].Descriptor.RegisterSpace = 0;
//    d3dRootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
//
//    // [���ο� �Ķ����] ��Ű�� ��� ����
//    d3dRootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
//    d3dRootParameters[4].Descriptor.ShaderRegister = 4; // b4: ��Ű�� �� ���
//    d3dRootParameters[4].Descriptor.RegisterSpace = 0;
//    d3dRootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // ���ؽ� ���̴������� �ʿ�
//
//    // [���ο� �Ķ����] �ؽ�ó ��ũ���� ���̺�
//    d3dRootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
//    d3dRootParameters[5].DescriptorTable.NumDescriptorRanges = 1;
//    d3dRootParameters[5].DescriptorTable.pDescriptorRanges = &d3dDescriptorRanges[0];
//    d3dRootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // �ȼ� ���̴������� �ʿ�
//
//    d3dRootSignatureDesc.NumParameters = _countof(d3dRootParameters);
//    d3dRootSignatureDesc.pParameters = d3dRootParameters;
//
//    // 3. �ؽ�ó ���÷� ����
//    D3D12_STATIC_SAMPLER_DESC d3dStaticSamplerDesc = {};
//    d3dStaticSamplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
//    d3dStaticSamplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
//    d3dStaticSamplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
//    d3dStaticSamplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
//    d3dStaticSamplerDesc.MipLODBias = 0;
//    d3dStaticSamplerDesc.MaxAnisotropy = 1;
//    d3dStaticSamplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_ALWAYS;
//    d3dStaticSamplerDesc.MinLOD = 0;
//    d3dStaticSamplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
//    d3dStaticSamplerDesc.ShaderRegister = 0; // ���̴��� s0 �������Ϳ� ����
//    d3dStaticSamplerDesc.RegisterSpace = 0;
//    d3dStaticSamplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
//
//    d3dRootSignatureDesc.NumStaticSamplers = 1;
//    d3dRootSignatureDesc.pStaticSamplers = &d3dStaticSamplerDesc;
//
//    // 4. ��Ʈ ���� ����
//    ID3D12RootSignature* pd3dGraphicsRootSignature = nullptr;
//    ComPtr<ID3DBlob> pd3dSignatureBlob, pd3dErrorBlob;
//    D3D12SerializeRootSignature(&d3dRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pd3dSignatureBlob, &pd3dErrorBlob);
//    pd3dDevice->CreateRootSignature(0, pd3dSignatureBlob->GetBufferPointer(), pd3dSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&pd3dGraphicsRootSignature));
//
//    return pd3dGraphicsRootSignature;
//}
//
//ID3D12RootSignature* Scene::GetGraphicsRootSignature()
//{
//    //return(m_pd3dGraphicsRootSignature);
//    return (nullptr);
//}
//
//// �ڵ� �Ҵ� �Լ� ����
//void Scene::AllocateNextSrvDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE& outCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE& outGpuHandle)
//{
//    outCpuHandle = _SrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
//    outCpuHandle.ptr += (_SrvDescriptorIncrementSize * _AllocatedSrvCount);
//
//    outGpuHandle = _SrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
//    outGpuHandle.ptr += (_SrvDescriptorIncrementSize * _AllocatedSrvCount);
//
//    _AllocatedSrvCount++; // ī���� ����
//}
//
//GameObject* Scene::PickObjectPointedByCursor(int xClient, int yClient)
//{
//    XMFLOAT3 xmf3PickPosition = {0.0f, 0.0f, 0.0f};
//    xmf3PickPosition.x = (((2.0f * xClient) / (float)m_pCamera->m_d3dViewport.Width) - 1) / m_pCamera->m_xmf4x4Projection._11;
//    xmf3PickPosition.y = -(((2.0f * yClient) / (float)m_pCamera->m_d3dViewport.Height) - 1) / m_pCamera->m_xmf4x4Projection._22;
//    xmf3PickPosition.z = 1.0f;
//
//    XMVECTOR xmvPickPosition = XMLoadFloat3(&xmf3PickPosition);
//    XMMATRIX xmmtxView = XMLoadFloat4x4(&m_pCamera->m_xmf4x4View);
//
//    bool nIntersected = false;
//    float fNearestHitDistance = FLT_MAX;
//    GameObject* pNearestObject = NULL;
//
//
//    std::array<std::list<std::shared_ptr<GameObject>>, ALLARRAYSIZE>& Arr = ObjectManager::Instance()->GetAllObject();
//
//    if (Arr.size()) {
//        int i = 0;
//        for (auto& Objects : Arr) {
//            for (auto& Object : Objects) {
//                float fHitDistance = FLT_MAX;
//                nIntersected = Object.get()->pick_model_obb(xmvPickPosition, xmmtxView, &fHitDistance);
//                if (nIntersected && (fHitDistance < fNearestHitDistance))
//                {
//                    fNearestHitDistance = fHitDistance;
//                    pNearestObject = Object.get();
//                }
//            }
//            ++i;
//            if (4 == i) {
//                break;
//            }
//            
//        }
//    }
//
//    return(pNearestObject);
//    return nullptr;
//}
//
//void Scene::MakeDummyBonebuffer(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
//{
//    UINT nBoneCount = 128;
//    UINT nBufferSize = sizeof(XMFLOAT4X4) * nBoneCount;
//
//    // UPLOAD ���� �����Ͽ� CPU�� �׻� ���� �����ϵ��� �մϴ�.
//    // ������ ����־ ������, 0���� �ʱ�ȭ�صθ� �� �������Դϴ�.
//    m_pd3dcbDummyBoneTransforms = ::CreateBufferResource(pd3dDevice, pd3dCommandList, nullptr, nBufferSize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);
//
//    // ������ ������ 0���� �ʱ�ȭ (���� ���������� ����)
//    UINT8* pMappedData = nullptr;
//    D3D12_RANGE readRange{ 0, 0 };
//    m_pd3dcbDummyBoneTransforms->Map(0, &readRange, reinterpret_cast<void**>(&pMappedData));
//    memset(pMappedData, 0, nBufferSize);
//    m_pd3dcbDummyBoneTransforms->Unmap(0, nullptr);
//}
//
//D3D12_GPU_VIRTUAL_ADDRESS Scene::GetDummyBoneBufferAddress() const
//{
//    if (m_pd3dcbDummyBoneTransforms)
//    {
//        return m_pd3dcbDummyBoneTransforms->GetGPUVirtualAddress();
//    }
//    return 0;
//}
//
//void Scene::MakeSrv(ID3D12Device* pd3dDevice)
//{
//    // �������������������������� SRV ��ũ���� �� ���� ��������������������������
//    // �ؽ�ó�� ���� ���̴� ���ҽ� ��(SRV)���� ���� ��ũ���� ���� �����մϴ�.
//    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
//    srvHeapDesc.NumDescriptors = 1024; // �� ������ ����� �ִ� �ؽ�ó ���� (���Ƿ� 128�� ����, �ʿ�� ����)
//    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
//    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; // **�ſ� �߿�**: ���̴��� ���� �����ؾ� ��
//    srvHeapDesc.NodeMask = 0;
//
//    // ��ũ���� �� ����
//    pd3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&_SrvDescriptorHeap));
//
//    // SRV ��ũ������ ũ�⸦ ������ �Ӵϴ�. �ڵ� �ּҸ� ����� �� �ʿ��մϴ�.
//    _SrvDescriptorIncrementSize = pd3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
//
//    // �Ҵ�� ��ũ���� ���� ī���͸� 0���� �ʱ�ȭ�մϴ�.
//    _AllocatedSrvCount = 0;
//    // ���������������������������������������������������������������������
//}
//
//void Scene::ReleaseUploadBuffers()
//{
//    auto& Arr = ObjectManager::Instance()->GetAllObject();
//
//    for (auto& Objects : Arr) {
//        for (auto& Object : Objects) {
//            auto Object_Render = Object->get_component<RenderComponent>();
//            if (Object_Render)
//                Object_Render->release_upload_buffers();
//        }
//    }
//}
//
//void Scene::BuildLightsAndMaterials()
//{
//    m_pLights = new LIGHTS;
//    ::ZeroMemory(m_pLights, sizeof(LIGHTS));
//    m_pLights->m_xmf4GlobalAmbient = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
//
//    // ��� ���⼺ ����
//    m_pLights->m_pLights[0].m_bEnable = true;
//    m_pLights->m_pLights[0].m_nType = DIRECTIONAL_LIGHT; 
//    m_pLights->m_pLights[0].m_xmf4Ambient = XMFLOAT4(0.3f, 0.3f, 0.3f, 1.0f);
//    m_pLights->m_pLights[0].m_xmf4Diffuse = XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f);
//    m_pLights->m_pLights[0].m_xmf4Specular = XMFLOAT4(1.f, 1.f, 1.f, 0.0f);
//    m_pLights->m_pLights[0].m_xmf3Direction = XMFLOAT3(0.0f, -1.0f, 1.0f);
//
//    m_pMaterials = new MATERIALS;
//    ::ZeroMemory(m_pMaterials, sizeof(MATERIALS));
//
//    // ��� �ö�ƽ ����
//    m_pMaterials->m_pReflections[0]._ambient = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
//    m_pMaterials->m_pReflections[0]._diffuse = XMFLOAT4(0.8f, 0.8f, 0.8f, 1.0f);
//    m_pMaterials->m_pReflections[0]._specular = XMFLOAT4(1.f, 1.f, 1.f, 16.0f);
//    m_pMaterials->m_pReflections[0]._emissive = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
//}
//
//void Scene::CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
//{
//    UINT ncbElementBytes = ((sizeof(LIGHTS) + 255) & ~255); // 256�� ���
//    m_pd3dcbLights = ::CreateBufferResource(pd3dDevice, pd3dCommandList, NULL, ncbElementBytes, 
//        D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, NULL);
//    m_pd3dcbLights->Map(0, NULL, (void**)&m_pcbMappedLights);
//
//    ncbElementBytes = ((sizeof(MATERIALS) + 255) & ~255); // 256�� ���
//    m_pd3dcbMaterials = ::CreateBufferResource(pd3dDevice, pd3dCommandList, NULL, ncbElementBytes, 
//        D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, NULL);
//    m_pd3dcbMaterials->Map(0, NULL, (void**)&m_pcbMappedMaterials);
//}
//
//void Scene::UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList)
//{
//    ::memcpy(m_pcbMappedLights, m_pLights, sizeof(LIGHTS));
//    ::memcpy(m_pcbMappedMaterials, m_pMaterials, sizeof(MATERIALS));
//}
//
//void Scene::ReleaseShaderVariables()
//{
//    if (m_pd3dcbLights) m_pd3dcbLights->Unmap(0, NULL);
//    if (m_pd3dcbMaterials) m_pd3dcbMaterials->Unmap(0, NULL);
//}