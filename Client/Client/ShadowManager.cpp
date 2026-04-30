#include "stdafx.h"
#include "ShadowManager.h"
#include "CameraComponent.h"
#include "AnimationComponent.h"
#include "GameFramework.h"
#include "RenderComponent.h"
#include "Renderer.h"
#include "LightManager.h"
#include "OcclusionManager.h"

void ShadowManager::initialize(ID3D12Device* device)
{
    // 1. Shadow Map Array 리소스 생성 (1024x1024, ArraySize=3)
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Alignment = 0;
    texDesc.Width = _shadowmapSize;
    texDesc.Height = _shadowmapSize;
    texDesc.DepthOrArraySize = 3; // 2 Cascade
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
    dsvHeapDesc.NumDescriptors = 3;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&_dsvHeap));

    UINT dsvSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

    CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle
    (_dsvHeap->GetCPUDescriptorHandleForHeapStart());

    for (int i = 0; i < 3; ++i)
    {
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
        dsvDesc.Texture2DArray.FirstArraySlice = i; // 슬라이스 0, 1
    	dsvDesc.Texture2DArray.ArraySize = 1;       // 한 번에 하나씩만 그림
        dsvDesc.Texture2DArray.MipSlice = 0;

        CD3DX12_CPU_DESCRIPTOR_HANDLE hDsv(_dsvHeap->GetCPUDescriptorHandleForHeapStart(), i, dsvSize);
        device->CreateDepthStencilView(_shadowMapArray.Get(), &dsvDesc, hDsv);
    }
   
    // 3. SRV Heap 생성 
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
    srvHeapDesc.NumDescriptors = 3;
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
    for (int i = 0; i < 3; ++i)
    {
        device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE,
            &cbDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS
            (&_cbShadow[i]));
        _cbShadow[i]->Map(0, nullptr, reinterpret_cast<void**>(&_mappedCbShadow[i]));
    }
}

void ShadowManager::build_cascade_matrices()
{
    // 빛의 방향 세팅
    XMFLOAT3 lightDir = LightManager::instance()->get_sun_direction();
    XMVECTOR dir = XMVector3Normalize(XMLoadFloat3(&lightDir));

    // 메인 카메라 위치 가져오기
    XMVECTOR camPos = XMVectorSet(0, 0, 0, 1);
    if (CameraComponent::get_main()) {
        auto pos = CameraComponent::get_main()->game_object()->transform()->position();
        camPos = XMVectorSet(pos.x, pos.y, pos.z, 1.0f);
    }

    XMVECTOR lightPos = camPos - dir * 1000.0f; // 빛을 적당히 멀리 떨어뜨림
    XMVECTOR up = XMVectorSet(0, 1, 0, 0);
    XMMATRIX lightView = XMMatrixLookToLH(lightPos, dir, up);

    float radii[3] = {
         shadow_max_distance * 0.1f,
         shadow_max_distance * 0.3f,
         shadow_max_distance * 1.0f
    };

    for (int c = 0; c < 3; c++)
    {
        XMMATRIX proj = XMMatrixOrthographicLH(radii[c] * 2, radii[c] * 2, 1.0f, 2000.0f);

        // 텍셀 스내핑 (Texel Snapping) - 지글거림 제거
        const float shadowMapResolution = (float)_shadowmapSize;
        XMMATRIX lightViewProj = lightView * proj;

        // 1. 월드 원점을 섀도우 맵 픽셀 공간으로 투영
        XMVECTOR shadowOrigin = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
        shadowOrigin = XMVector4Transform(shadowOrigin, lightViewProj);

        // 2. NDC 공간을 픽셀 단위로 스케일 업
        shadowOrigin = XMVectorScale(shadowOrigin, shadowMapResolution / 2.0f);

        // 3. 텍셀 스내핑 (반올림)
        XMVECTOR roundedOrigin = XMVectorRound(shadowOrigin);

        // 4. 오차 계산
        XMVECTOR roundOffset = XMVectorSubtract(roundedOrigin, shadowOrigin);

        // 5. 오차를 NDC 스케일로 축소
        roundOffset = XMVectorScale(roundOffset, 2.0f / shadowMapResolution);

        // 6. Z와 W축은 0으로 초기화 (X, Y만 보정)
        roundOffset = XMVectorSetZ(roundOffset, 0.0f);
        roundOffset = XMVectorSetW(roundOffset, 0.0f);

        // 7. 투영 행렬에 오프셋 적용
        XMMATRIX shadowProjOffset = XMMatrixTranslationFromVector(roundOffset);
        proj = XMMatrixMultiply(proj, shadowProjOffset);

        // 8. 최종 VP 행렬
        XMMATRIX vp = lightView * proj;

        // 셰이더용 GPU에 맞게 Transpose하여 저장
        XMStoreFloat4x4(&_cascadeData.cascades[c].lightVP, XMMatrixTranspose(vp));
        XMStoreFloat4x4(&_shadowData.lightVP[c], XMMatrixTranspose(vp));
    }

    _shadowData.splitNear = radii[0];
    _shadowData.splitMid = radii[1];
    _shadowData.bias = 0.00f;
}

void ShadowManager::update_and_execute(ID3D12GraphicsCommandList* cmd, UINT frame_index)
{
    _currentFrameIndex = frame_index;

    // 1. 행렬 빌드 및 복사
    build_cascade_matrices();
    memcpy(_mappedCbCascades, &_cascadeData, sizeof(CbCascades));
    memcpy(_mappedCbShadow[frame_index], &_shadowData, sizeof(CbShadow));

    // 2. Resource Barrier: PSR -> DEPTH_WRITE (모든 슬라이스)
    CD3DX12_RESOURCE_BARRIER barriersW[3];
    for (int i = 0; i < 3; ++i) {
        barriersW[i] = CD3DX12_RESOURCE_BARRIER::Transition(
            _shadowMapArray.Get(),
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            i);
    }
    cmd->ResourceBarrier(3, barriersW);

    // 공통 뷰포트/시저 설정
    D3D12_VIEWPORT viewport = { 0.0f, 0.0f, (float)_shadowmapSize, (float)_shadowmapSize, 0.0f, 1.0f };
    D3D12_RECT scissor = {0, 0, _shadowmapSize, _shadowmapSize };
    cmd->RSSetViewports(1, &viewport);
    cmd->RSSetScissorRects(1, &scissor);

    auto renderer = Renderer::instance();
    const auto& renderMap = renderer->get_shadow_render_map();
    // [추가] 오클루전 쿼리 결과 버퍼 가져오기
    ID3D12Resource* prevBuffer = OcclusionManager::instance()->get_result_buffer_for_predication(frame_index);
    f3 camPos = (CameraComponent::get_main()) ? CameraComponent::get_main()->game_object()->transform()->get_world_position() : f3{ 0,0,0 };

    // 캐스케이드별 거리 기준 (build_cascade_matrices와 동일하게 맞춤)
    float radii[3] = {
          shadow_max_distance * 0.1f,  // Cascade 0: 30m
          shadow_max_distance * 0.3f,  // Cascade 1: 90m
          shadow_max_distance * 1.0f   // Cascade 2: 300m
    };

    // 카메라와 매우 가까운 거리는 쿼리 없이 무조건 그림자 생성
    const float nearShadowThreshold = 30.0f; // 오클루전 테스트 스킵 기준
    UINT dsvSize = GameFramework::instance()->device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

    // 3단계 캐스케이드 렌더링 루프 시작
    for (int i = 0; i < 3; ++i)
    {
        // A. 현재 캐스케이드에 해당하는 DSV 바인딩 및 Clear
        CD3DX12_CPU_DESCRIPTOR_HANDLE dsvHandle(_dsvHeap->GetCPUDescriptorHandleForHeapStart(), i, dsvSize);
        cmd->OMSetRenderTargets(0, nullptr, FALSE, &dsvHandle);
        cmd->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

        // B. 현재 캐스케이드 전용 Constant Buffer 주소 계산 (b1 레지스터용)
        D3D12_GPU_VIRTUAL_ADDRESS currentCbAddress = _cbCascades->GetGPUVirtualAddress() + (i * sizeof(CbCascadeSingle));

        // C. 일반 메시 렌더링 (gltf)
        {
            ID3D12PipelineState* pso = renderer->get_pso("csm_depth");
            ID3D12RootSignature* rootSig = renderer->get_root_signature("csm_depth");

            if (pso && rootSig) {
                cmd->SetPipelineState(pso);
                cmd->SetGraphicsRootSignature(rootSig);
                cmd->SetGraphicsRootConstantBufferView(1, currentCbAddress);

                auto it = renderMap.find("gltf");
                if (it != renderMap.end()) {
                    for (const auto& obj : it->second) {
                        if (!obj || obj->is_destroyed()) continue;

                        // [최적화 1]: 캐스케이드별 거리 컬링
                        // 현재 그리는 캐스케이드의 범위를 벗어나면 Draw Call을 아예 날리지 않음
                        f3 objPos = obj->transform()->get_world_position();
                        float dist = Vector3::Length(Vector3::Subtract(camPos, objPos));
                        if (dist > radii[i]) continue;

                        auto rc = obj->get_component<RenderComponent>();
                        if (!rc) continue;

                        // [최적화 2]: 오클루전 쿼리 결과에 따른 조건부 렌더링 (Predication)
                        if (dist < nearShadowThreshold || rc->skip_occlusion()) {
                            rc->render_CascadeShadowMap(cmd, frame_index);
                        }
                        else {
                            cmd->SetPredication(prevBuffer, rc->get_occlusion_query_index() * sizeof(UINT64), D3D12_PREDICATION_OP_NOT_EQUAL_ZERO);
                            rc->render_CascadeShadowMap(cmd, frame_index);
                            cmd->SetPredication(nullptr, 0, D3D12_PREDICATION_OP_EQUAL_ZERO);
                        }
                    }
                }
            }
        }

        // D. 스킨드 애니메이션 메시 렌더링 (skinned)
        {
            ID3D12PipelineState* pso = renderer->get_pso("csm_depth_skinned");
            ID3D12RootSignature* rootSig = renderer->get_root_signature("csm_depth_skinned");

            if (pso && rootSig) {
                cmd->SetPipelineState(pso);
                cmd->SetGraphicsRootSignature(rootSig);
                cmd->SetGraphicsRootConstantBufferView(1, currentCbAddress);

                auto it = renderMap.find("skinned");
                if (it != renderMap.end()) {
                    for (const auto& obj : it->second) {
                        if (!obj || obj->is_destroyed()) continue;

                        // [최적화 1]: 거리 컬링
                        f3 objPos = obj->transform()->get_world_position();
                        float dist = Vector3::Length(Vector3::Subtract(camPos, objPos));
                        if (dist > radii[i]) continue;

                        auto rc = obj->get_component<RenderComponent>();
                        auto animComp = obj->get_component<AnimationComponent>();
                        if (!rc || !animComp) continue;

                        // 본 행렬 데이터 바인딩
                        D3D12_GPU_VIRTUAL_ADDRESS boneGpuAddr = animComp->get_bone_gpu_virtual_address();
                        if (boneGpuAddr != 0) cmd->SetGraphicsRootConstantBufferView(2, boneGpuAddr);

                        // [최적화 2]: 오클루전 쿼리 기반 렌더링
                        if (dist < nearShadowThreshold || rc->skip_occlusion()) {
                            rc->render_CascadeShadowMap(cmd, frame_index);
                        }
                        else {
                            cmd->SetPredication(prevBuffer, rc->get_occlusion_query_index() * sizeof(UINT64), D3D12_PREDICATION_OP_NOT_EQUAL_ZERO);
                            rc->render_CascadeShadowMap(cmd, frame_index);
                            cmd->SetPredication(nullptr, 0, D3D12_PREDICATION_OP_EQUAL_ZERO);
                        }
                    }
                }
            }
        }
    }

    // 3. Resource Barrier: DEPTH_WRITE -> PSR (모든 슬라이스)
    CD3DX12_RESOURCE_BARRIER barriersR[3];
    for (int i = 0; i < 3; ++i) {
        barriersR[i] = CD3DX12_RESOURCE_BARRIER::Transition(
            _shadowMapArray.Get(),
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
            i);
    }
    cmd->ResourceBarrier(3, barriersR);
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