#include "stdafx.h"
#include "ShadowManager.h"
#include "CameraComponent.h"
#include "ObjectManager.h"
#include "RenderComponent.h"
#include "Renderer.h"

void ShadowManager::initialize(ID3D12Device* device)
{
    // 1. Shadow Map Array 리소스 생성 (1024x1024, ArraySize=3)
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Alignment = 0;
    texDesc.Width = 1024;
    texDesc.Height = 1024;
    texDesc.DepthOrArraySize = 3; // 3 Cascades
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R32_TYPELESS;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE optClear = {};
    optClear.Format = DXGI_FORMAT_D32_FLOAT;
    optClear.DepthStencil.Depth = 1.0f;
    optClear.DepthStencil.Stencil = 0;

    CD3DX12_HEAP_PROPERTIES heapProp(D3D12_HEAP_TYPE_DEFAULT);
    device->CreateCommittedResource(
        &heapProp,
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
        &optClear,
        IID_PPV_ARGS(&_shadowMapArray));

    // 2. DSV Heap 생성 (CPU Only, 1 Descriptor)
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&_dsvHeap));

    UINT dsvSize = device->GetDescriptorHandleIncrementSize
    (D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle
    (_dsvHeap->GetCPUDescriptorHandleForHeapStart());

    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
    dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
    dsvDesc.Texture2DArray.FirstArraySlice = 0;
    dsvDesc.Texture2DArray.ArraySize = 3;
    dsvDesc.Texture2DArray.MipSlice = 0;
    device->CreateDepthStencilView(_shadowMapArray.Get(), &dsvDesc, dsvHandle);
   
    // 3. SRV Heap 생성 (CPU Only, 1 Descriptor for the Array)
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 1;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    device->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&_srvHeap));

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2DArray.MostDetailedMip = 0;
    srvDesc.Texture2DArray.MipLevels = 1;
    srvDesc.Texture2DArray.FirstArraySlice = 0;
    srvDesc.Texture2DArray.ArraySize = 3;
    device->CreateShaderResourceView(_shadowMapArray.Get(), &srvDesc, _srvHeap->GetCPUDescriptorHandleForHeapStart());

    // 4. 상수 버퍼 생성
    UINT cbCascadesSize = (sizeof(CbCascades) + 255) & ~255;
    CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC cbDesc = CD3DX12_RESOURCE_DESC::Buffer(cbCascadesSize);
    device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &cbDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&_cbCascades));
    _cbCascades->Map(0, nullptr, reinterpret_cast<void**>(&_mappedCbCascades));

    UINT cbShadowSize = (sizeof(CbShadow) + 255) & ~255;
    cbDesc = CD3DX12_RESOURCE_DESC::Buffer(cbShadowSize);
    for (int i = 0; i < 2; ++i)
    {
        device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE,
            &cbDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS
            (&_cbShadow[i]));
        _cbShadow[i]->Map(0, nullptr, reinterpret_cast<void**>(&_mappedCbShadow[i]));
    }
}

void ShadowManager::build_cascade_matrices()
{
    // 빛의 방향 세팅 (하드코딩된 값)
    XMFLOAT3 lightDir = { 0.05f, -0.4f, -0.82f };
    XMVECTOR dir = XMVector3Normalize(XMLoadFloat3(&lightDir));

    // 메인 카메라 위치 가져오기
    XMVECTOR camPos = XMVectorSet(0, 0, 0, 1);
    if (CameraComponent::get_main()) {
        auto pos = CameraComponent::get_main()->game_object()->transform()->
            position();
        camPos = XMVectorSet(pos.x, pos.y, pos.z, 1.0f);
    }

    XMVECTOR lightPos = camPos - dir * 1000.0f; // 빛을 적당히 멀리 떨어뜨림
    XMVECTOR up = XMVectorSet(0, 1, 0, 0);
    XMMATRIX lightView = XMMatrixLookToLH(lightPos, dir, up);

    float radii[3] = { 20.0f, 80.0f, 256.0f }; // 각 Cascade 반경
    for (int c = 0; c < 3; c++)
    {
        XMMATRIX proj = XMMatrixOrthographicLH(radii[c] * 2, radii[c] * 2, 1.0f, 2000.0f);
        XMMATRIX vp = lightView * proj;
        // 행렬을 GPU에 맞게 Transpose하여 저장
        XMStoreFloat4x4(&_cascadeData.lightVP[c], XMMatrixTranspose(vp));
        XMStoreFloat4x4(&_shadowData.lightVP[c], XMMatrixTranspose(vp));
    }

    _shadowData.splitNear = 20.0f;
    _shadowData.splitMid = 80.0f;
    _shadowData.bias = 0.005f;
}

void ShadowManager::update_and_execute(ID3D12GraphicsCommandList* cmd, UINT frame_index)
{
    // [수정] 현재 프레임 인덱스 업데이트(lighting 패스에서 올바른 CB를 참조하기 위함)
    _currentFrameIndex = frame_index;

    // 1. 행렬 계산 및 데이터 업로드
    build_cascade_matrices();
    memcpy(_mappedCbCascades, &_cascadeData, sizeof(CbCascades));
    memcpy(_mappedCbShadow[frame_index], &_shadowData, sizeof(CbShadow));

    // [수정 2] 시작할 때 배리어를 칩니다. (READ -> WRITE)
    CD3DX12_RESOURCE_BARRIER barriersW[3]; 
    for (int i = 0; i < 3; ++i) {
        barriersW[i] = CD3DX12_RESOURCE_BARRIER::Transition(
            _shadowMapArray.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            i); // <-- 명시적으로 서브리소스 인덱스(0, 1, 2) 지정
    }
    cmd->ResourceBarrier(3, barriersW); // 3개를 한 번에 실행

    // 3. 렌더타겟 설정
    // DSV를 0번 슬라이스 주소 하나만 넘겨줌 (배열 크기가 3으로 잡혀있어서 가능
    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle
    (_dsvHeap->GetCPUDescriptorHandleForHeapStart());
    cmd->OMSetRenderTargets(0, nullptr, FALSE, &dsvHandle);

    cmd->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    // 5. 뷰포트 & 시저 설정
    D3D12_VIEWPORT viewport = { 0.0f, 0.0f, 1024.0f, 1024.0f, 0.0f, 1.0f };
    D3D12_RECT scissor = { 0, 0, 1024, 1024 };
    cmd->RSSetViewports(1, &viewport);
    cmd->RSSetScissorRects(1, &scissor);

    // 6. 파이프라인 및 루트 시그니처 바인딩
    auto renderer = Renderer::instance();
    ID3D12PipelineState* pso = renderer->get_pso("csm_depth");
    ID3D12RootSignature* rootSig = renderer->get_root_signature("csm_depth");

    if (pso && rootSig) {
        cmd->SetPipelineState(pso);
        cmd->SetGraphicsRootSignature(rootSig);
    }
    else {
        CD3DX12_RESOURCE_BARRIER barriersR[3];
        for (int i = 0; i < 3; ++i) {
            barriersR[i] = CD3DX12_RESOURCE_BARRIER::Transition(
                _shadowMapArray.Get(),
                D3D12_RESOURCE_STATE_DEPTH_WRITE,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                i);
        }
        cmd->ResourceBarrier(3, barriersR);
        return; // 배리어 원복 후 안전하게 종료
    }
    cmd->SetGraphicsRootConstantBufferView(1, _cbCascades->GetGPUVirtualAddress());

    // 7. 오브젝트 그리기
    const auto& objs = ObjectManager::instance()->get_all_game_objects();
    for (const auto& obj : objs) {
        if (!obj || obj->is_destroyed()) continue;

        auto renderComp = obj->get_component<RenderComponent>();
        if (!renderComp) continue;

        auto shaderName = renderComp->pso_name();
        // 현재는 gltf, terrain(glb)만 그림자 생성
        if (shaderName == "gltf" || shaderName == "glb") {
            renderComp->render_CascadeShadowMap(cmd, frame_index);
        }
    }

    CD3DX12_RESOURCE_BARRIER barriersR[3];
    for (int i = 0; i < 3; ++i) {
        barriersR[i] = CD3DX12_RESOURCE_BARRIER::Transition(
            _shadowMapArray.Get(),
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            i); // <-- 명시적으로 서브리소스 인덱스(0, 1, 2) 지정
    }
    cmd->ResourceBarrier(3, barriersR); // 3개를 한 번에 실행
}

void ShadowManager::bind_for_lighting(ID3D12GraphicsCommandList* cmd, UINT shadowCbParamIdx, UINT shadowSrvParamIdx, Renderer* renderer)
{
    if (!_cbShadow[0] || !_cbShadow[1] || !_srvHeap || !_shadowMapArray)
    {
        // 초기화되지 않았으면 바인딩하지 않음
        return;
    }

    UINT frame_index = _currentFrameIndex;

    // b5 상수 버퍼 바인딩
    cmd->SetGraphicsRootConstantBufferView(shadowCbParamIdx,
        _cbShadow[frame_index]->GetGPUVirtualAddress());

    // t11 Descriptor Table 바인딩
    // (CPU의 SRV Descriptor를 Renderer의 GPU Descriptor Heap에 복사 후 바인딩)
    CD3DX12_CPU_DESCRIPTOR_HANDLE cpuSrvHandle(_srvHeap->GetCPUDescriptorHandleForHeapStart());
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> cpuHandles = { cpuSrvHandle };

    renderer->bind_texture_table(cmd, shadowSrvParamIdx, cpuHandles);
}