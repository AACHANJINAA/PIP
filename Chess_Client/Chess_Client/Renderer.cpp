#include "stdafx.h"
#include "Renderer.h"
#include "Camera.h"
#include "DebugShader.h"
#include "DefaultObjectShader.h"
#include "GameObject.h"
#include "Shader.h"
#include "ObjectManager.h"
#include "PlayerShader.h"
#include "RenderComponent.h"
#include "ResourceManager.h"

void Renderer::initialize(ID3D12Device* device)
{
    _device = device;
    create_root_signatures(device);
    // [추가] 사용할 루트 시그니처 생성기들을 등록합니다.
    _rootSignatureGenerators.push_back(std::make_unique<DefaultRootSignatureGenerator>());
    _rootSignatureGenerators.push_back(std::make_unique<SkinnedRootSignatureGenerator>());
    // 새 루트 시그니처가 필요하면 여기에 생성기만 추가하면 끝입니다.

    // [추가] PSO를 생성할 셰이더 프로토타입들을 등록합니다.
    // [변경] 셰이더 프로토타입을 등록할 때, 셰이더의 이름을 키로 하여 map에 저장합니다.
	auto default_shader = std::make_shared<DefaultObjectShader>();
    _shaderPrototypes[default_shader->pso_name()] = default_shader;

    auto player_shader = std::make_shared<PlayerShader>();
    _shaderPrototypes[player_shader->pso_name()] = player_shader;

    auto debug_shader = std::make_shared<DebugShader>();
    _shaderPrototypes[debug_shader->pso_name()] = debug_shader;
    // 앞으로 새로운 셰이더를 추가할 땐 이 목록에 한 줄만 추가하면 됩니다.

    create_pipeline_state_objects(device);
}
void Renderer::create_root_signatures(ID3D12Device* device)
{
    _rootSignatures.clear();
    for (const auto& generator : _rootSignatureGenerators)
    {
        _rootSignatures[generator->name()] = generator->create(device);
    }
}
//void Renderer::create_root_signatures(ID3D12Device* device)
//{
//    ID3D12RootSignature* pd3dGraphicsRootSignature = NULL;
//    D3D12_ROOT_PARAMETER pd3dRootParameters[4];
//    // [수정] 0번 파라미터: 월드 행렬용 CBV
//    pd3dRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
//    pd3dRootParameters[0].Descriptor.ShaderRegister = 0; // b0
//    pd3dRootParameters[0].Descriptor.RegisterSpace = 0;
//    pd3dRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
//
//    // [수정] 1번 파라미터: 카메라용 CBV
//    pd3dRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
//    pd3dRootParameters[1].Descriptor.ShaderRegister = 1; // b1
//    pd3dRootParameters[1].Descriptor.RegisterSpace = 0;
//    pd3dRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
//
//    // 머터리얼 정보를 위한 상수 버퍼 뷰(CBV) 추가
//    pd3dRootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
//    pd3dRootParameters[2].Descriptor.ShaderRegister = 2; // 셰이더의 b2 레지스터
//    pd3dRootParameters[2].Descriptor.RegisterSpace = 0;
//    pd3dRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
//
//    // 조명 정보를 위한 상수 버퍼 뷰(CBV) 추가
//    pd3dRootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
//    pd3dRootParameters[3].Descriptor.ShaderRegister = 3; // 셰이더의 b3 레지스터
//    pd3dRootParameters[3].Descriptor.RegisterSpace = 0;
//    pd3dRootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
//
//    D3D12_ROOT_SIGNATURE_FLAGS d3dRootSignatureFlags =
//        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
//        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
//        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
//        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
//    // 이 부분에 픽셀 쉐이더 접근안되게 하는거 지움
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
//    device->CreateRootSignature(0, pd3dSignatureBlob->GetBufferPointer(),
//        pd3dSignatureBlob->GetBufferSize(), __uuidof(ID3D12RootSignature), (void**)&pd3dGraphicsRootSignature);
//
//	_defaultRootSignature = pd3dGraphicsRootSignature;
//}

void Renderer::create_pipeline_state_objects(ID3D12Device* device)
{
    _pipelineStates.clear(); // 기존 PSO 맵 비우기

    // 등록된 모든 셰이더 프로토타입을 순회합니다.
    for (const auto& [shader_name, shader_prototype] : _shaderPrototypes)
    {
        // 1. 셰이더가 요구하는 루트 시그니처를 가져옵니다.
        ID3D12RootSignature* root_signature =
            get_root_signature(shader_prototype->required_root_signature());
        if (!root_signature) continue;

        // 2. 셰이더 프로토타입에게 PSO 생성을 위임합니다.
        ComPtr<ID3D12PipelineState> pso = shader_prototype->create_pso(device, root_signature);

        // 3. 반환된 PSO를 맵에 저장합니다. 이름은 셰이더가 직접 알려줍니다.
        if (pso)
        {
            _pipelineStates[shader_prototype->pso_name()] = pso;
        }
    }
}

void Renderer::render(ID3D12GraphicsCommandList* commandList, Camera* camera)
{
    // 1. 이번 프레임에 그릴 객체들을 추려낸다.
    build_render_list(camera);

    // 2. 추려낸 목록을 바탕으로 실제 그리기를 수행한다.
    draw_render_list(commandList, camera);
}

void Renderer::build_render_list(Camera* camera)
{
    _renderMap.clear();
    const auto& allGameObjects = ObjectManager::Instance()->get_all_game_objects();

    for (const auto& gameObject : allGameObjects)
    {
        if (!gameObject || gameObject->is_destroyed()) continue;

        auto renderComp = gameObject->get_component<RenderComponent>();
        if (renderComp && renderComp->is_enabled() /* && renderComp->is_visible(camera) */)
        {
            _renderMap[renderComp->pso_name()].push_back(gameObject);
        }
    }
}

void Renderer::draw_render_list(ID3D12GraphicsCommandList* commandList, Camera* camera)
{
    // --- 전역 UpdateShaderVariables의 역할이 여기로 왔습니다 ---
	// 카메라 상수 버퍼 업데이트
    //if (camera) camera->UpdateShaderVariables(commandList);

    // 조명 상수 버퍼 업데이트
    // if (LightManager::get_instance()) LightManager::get_instance()->update_shader_variables(commandList);
    // ---------------------------------------------------------

    // --- [추가] ResourceManager로부터 SRV 힙을 가져와 설정 ---
    ID3D12DescriptorHeap* ppHeaps[] = { ResourceManager::Instance()->get_srv_heap() };
    commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
    // ---------------------------------------------------------
    for (auto const& [psoName, gameObjects] : _renderMap)
    {
        if (gameObjects.empty()) continue;

        // 1. psoName으로 PSO를 가져옵니다.
        ID3D12PipelineState* pso = get_pso(psoName);
        if (!pso) continue;

        // 2. psoName으로 이 PSO를 만든 셰이더 프로토타입을 찾습니다.
        auto proto_it = _shaderPrototypes.find(psoName);
        if (proto_it == _shaderPrototypes.end()) 
        {
			CLOG("[Renderer] Shader prototype not found for PSO name: " << psoName);
            continue;
        }
        const auto& shader_prototype = proto_it->second;

        // 3. 찾은 셰이더에게 필요한 루트 시그니처의 '이름'을 물어봅니다.
        const std::string& root_sig_name = shader_prototype->required_root_signature();

        // 4. 그 이름으로 실제 루트 시그니처 '객체'를 가져옵니다.
        ID3D12RootSignature* root_signature = get_root_signature(root_sig_name);
        if (!root_signature) continue;

        // 5. 가져온 PSO와 루트 시그니처를 설정합니다. (이제 if문이 완전히 사라졌습니다!)
        commandList->SetPipelineState(pso);
        commandList->SetGraphicsRootSignature(root_signature);

        // 6. 이 PSO 그룹에 속한 모든 오브젝트를 그립니다.
        for (const auto& gameObject : gameObjects)
        {
            auto renderComp = gameObject->get_component<RenderComponent>();
            if (!renderComp) continue;

            auto mesh = renderComp->mesh();
            if (!mesh) continue;

            // --- [추가] GPU 업로드 확인 및 실행 ---
            if (!mesh->is_uploaded())
            {
                mesh->upload_to_gpu(_device, commandList);
            }
            // ------------------------------------

            renderComp->render(commandList, camera);
        }
    }
    //KJ 설명: OnPrepareRender 함수는 더 이상 필요 없으며, 그 역할은 Renderer가 더 효율적인 방식으로 수행하게 됩니다.
}

ID3D12RootSignature* Renderer::get_root_signature(const std::string& name) const
{
    // _rootSignatures 맵에서 'name'을 키로 가지는 원소를 찾습니다.
    auto it = _rootSignatures.find(name);

    // 찾았다면, 해당 원소의 값(ComPtr<ID3D12RootSignature>)에 접근하여
    // .Get() 함수로 실제 포인터를 반환합니다.
    if (it != _rootSignatures.end())
    {
        return it->second.Get();
    }

    // 맵에 해당 이름의 루트 시그니처가 없으면 nullptr을 반환합니다.
    // (또는 에러를 로그로 남기거나 기본값을 반환할 수도 있습니다.)
    return nullptr;
}

ID3D12PipelineState* Renderer::get_pso(const std::string& name) const
{
    auto it = _pipelineStates.find(name);
    if (it != _pipelineStates.end())
    {
        return it->second.Get();
    }
    return nullptr;
}