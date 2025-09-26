#include "stdafx.h"
#include "Renderer.h"
#include "Camera.h"
#include "GameObject.h"
#include "Shader.h"
#include "ObjectManager.h"
#include "RenderComponent.h"
#include "ResourceManager.h"

void Renderer::initialize(ID3D12Device* device)
{
    create_root_signatures(device);
    create_pipeline_state_objects(device);
}

void Renderer::create_root_signatures(ID3D12Device* device)
{
    ID3D12RootSignature* pd3dGraphicsRootSignature = NULL;
    D3D12_ROOT_PARAMETER pd3dRootParameters[4];
    // [수정] 0번 파라미터: 월드 행렬용 CBV
    pd3dRootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    pd3dRootParameters[0].Descriptor.ShaderRegister = 0; // b0
    pd3dRootParameters[0].Descriptor.RegisterSpace = 0;
    pd3dRootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // [수정] 1번 파라미터: 카메라용 CBV
    pd3dRootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    pd3dRootParameters[1].Descriptor.ShaderRegister = 1; // b1
    pd3dRootParameters[1].Descriptor.RegisterSpace = 0;
    pd3dRootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // 머터리얼 정보를 위한 상수 버퍼 뷰(CBV) 추가
    pd3dRootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    pd3dRootParameters[2].Descriptor.ShaderRegister = 2; // 셰이더의 b2 레지스터
    pd3dRootParameters[2].Descriptor.RegisterSpace = 0;
    pd3dRootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // 조명 정보를 위한 상수 버퍼 뷰(CBV) 추가
    pd3dRootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
    pd3dRootParameters[3].Descriptor.ShaderRegister = 3; // 셰이더의 b3 레지스터
    pd3dRootParameters[3].Descriptor.RegisterSpace = 0;
    pd3dRootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_FLAGS d3dRootSignatureFlags =
        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
        D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
    // 이 부분에 픽셀 쉐이더 접근안되게 하는거 지움

    D3D12_ROOT_SIGNATURE_DESC d3dRootSignatureDesc;
    ::ZeroMemory(&d3dRootSignatureDesc, sizeof(D3D12_ROOT_SIGNATURE_DESC));
    d3dRootSignatureDesc.NumParameters = _countof(pd3dRootParameters);
    d3dRootSignatureDesc.pParameters = pd3dRootParameters;
    d3dRootSignatureDesc.NumStaticSamplers = 0;
    d3dRootSignatureDesc.pStaticSamplers = NULL;
    d3dRootSignatureDesc.Flags = d3dRootSignatureFlags;

    ComPtr<ID3DBlob> pd3dSignatureBlob;
    ComPtr<ID3DBlob> pd3dErrorBlob;

    ::D3D12SerializeRootSignature(&d3dRootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pd3dSignatureBlob, &pd3dErrorBlob);
    device->CreateRootSignature(0, pd3dSignatureBlob->GetBufferPointer(),
        pd3dSignatureBlob->GetBufferSize(), __uuidof(ID3D12RootSignature), (void**)&pd3dGraphicsRootSignature);

	_defaultRootSignature = pd3dGraphicsRootSignature;
}

void Renderer::create_pipeline_state_objects(ID3D12Device* device)
{
    ComPtr<ID3DBlob> vsBlob, psBlob;

    // --- 1. "default" PSO (CObjectsShader 기반) ---
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = _defaultRootSignature.Get();
        psoDesc.VS = Shader::CompileShaderFromFile(L"Shaders.hlsl", "VSLighting", "vs_5_1", &vsBlob);
        psoDesc.PS = Shader::CompileShaderFromFile(L"Shaders.hlsl", "PSLighting", "ps_5_1", &psBlob);
        psoDesc.RasterizerState = Shader::CreateRasterizerState();
        psoDesc.BlendState = Shader::CreateBlendState();
        psoDesc.DepthStencilState = Shader::CreateDepthStencilState();

        D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };
        psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        psoDesc.SampleDesc.Count = 1;

        device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&_pipelineStates["default"]));
    }

    // --- 2. "skinned" PSO (GlbShader 기반) ---
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = _skinnedRootSignature ? _skinnedRootSignature.Get() : _defaultRootSignature.Get();
        psoDesc.VS = Shader::CompileShaderFromFile(L"GLB_Shader.hlsl", "VSSkinning", "vs_5_1", &vsBlob);
        psoDesc.PS = Shader::CompileShaderFromFile(L"GLB_Shader.hlsl", "PSSkinning", "ps_5_1", &psBlob);
        psoDesc.RasterizerState = Shader::CreateRasterizerState();
        psoDesc.BlendState = Shader::CreateBlendState();
        psoDesc.DepthStencilState = Shader::CreateDepthStencilState();

        D3D12_INPUT_ELEMENT_DESC skinnedInputLayout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "BLENDWEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };
        psoDesc.InputLayout = { skinnedInputLayout, _countof(skinnedInputLayout) };
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        psoDesc.SampleDesc.Count = 1;

        device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&_pipelineStates["skinned"]));
    }

    // --- 3. "debug" PSO (DebugShader 기반) ---
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
        psoDesc.pRootSignature = _defaultRootSignature.Get();
        psoDesc.VS = Shader::CompileShaderFromFile(L"Debug.hlsl", "VS_Debug", "vs_5_1", &vsBlob);
        psoDesc.PS = Shader::CompileShaderFromFile(L"Debug.hlsl", "PS_Debug", "ps_5_1", &psBlob);
        psoDesc.RasterizerState = Shader::CreateRasterizerState();
        psoDesc.BlendState = Shader::CreateBlendState();
        psoDesc.DepthStencilState = Shader::CreateDepthStencilState();

        D3D12_INPUT_ELEMENT_DESC debugInputLayout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
        };
        psoDesc.InputLayout = { debugInputLayout, _countof(debugInputLayout) };
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE; // 라인 렌더링용
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
        psoDesc.SampleDesc.Count = 1;

        device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&_pipelineStates["debug"]));
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

        // --- OnPrepareRender의 역할이 여기로 왔습니다 ---
        ID3D12PipelineState* pso = get_pso(psoName);
        if (!pso) continue;

        // 1. PSO를 설정합니다.
        commandList->SetPipelineState(pso);

        // 2. 루트 시그니처도 설정합니다.
        if (psoName == "skinned") {
            commandList->SetGraphicsRootSignature(get_root_signature("skinned"));
        }
        else {
            commandList->SetGraphicsRootSignature(get_root_signature("default"));
        }

        // 이 그룹의 모든 객체를 그립니다.
        for (const auto& gameObject : gameObjects)
        {
            gameObject->get_component<RenderComponent>()->render(commandList, camera);
        }
    }
    //KJ 설명: OnPrepareRender 함수는 더 이상 필요 없으며, 그 역할은 Renderer가 더 효율적인 방식으로 수행하게 됩니다.
}

ID3D12RootSignature* Renderer::get_root_signature(const std::string& name) const
{
    if (name == "skinned") return _skinnedRootSignature.Get();
    return _defaultRootSignature.Get();
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