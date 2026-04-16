#include "stdafx.h"
#include "DebugDrawManager.h"
#include "Renderer.h"

const int MAX_DEBUG_SHAPES = 100;

void DebugDrawManager::Initialize(ID3D12Device* device)
{
    CreateUnitBox(device);
    CreateUnitSphere(device);
    CreateUnitCapsule(device);
    CreateDynamicLineBuffer(device);
    UINT cbSize = (sizeof(XMFLOAT4X4) + 255) & ~255;
    for (int i = 0; i < SWAP_CHAIN_BUFFERS; ++i) {
        _cbWorld[i] = CreateBufferResource(device, nullptr, nullptr, cbSize * MAX_DEBUG_SHAPES,
            D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);
        _cbWorld[i]->Map(0, nullptr, reinterpret_cast<void**>(&_mappedWorld[i]));
    }
}

void DebugDrawManager::AddDebugRequest(const common::packet::SC_PACKET_DEBUG_DRAW& packet)
{
    _requests.push_back({
        packet._shape_type,
        {packet._position.x, packet._position.y, packet._position.z},
        {packet._rotation.x, packet._rotation.y, packet._rotation.z, packet._rotation.w},
        {packet._extents.x, packet._extents.y, packet._extents.z},
        packet._duration
    });
}

void DebugDrawManager::AddDebugShape(common::packet::DebugShapeType type, DirectX::XMFLOAT3 pos, DirectX::XMFLOAT4 rot, DirectX::XMFLOAT3 extents, float lifeTime)
{
    _requests.push_back({ type, pos, rot, extents, lifeTime });
}

void DebugDrawManager::AddDebugMeshShape(common::packet::DebugShapeType type, const std::vector<common::Vec3>& vertices,
	DirectX::XMFLOAT3 pos, DirectX::XMFLOAT4 rot, float lifeTime)
{
	_remoteShapes.push_back({ vertices, {pos.x, pos.y, pos.z}, {rot.x, rot.y, rot.z, rot.w} });
}

void DebugDrawManager::Update(float deltaTime)
{
    for (auto it = _requests.begin(); it != _requests.end();) {
        it->lifeTime -= deltaTime;
        if (it->lifeTime <= 0) it = _requests.erase(it);
        else ++it;
    }
}

void DebugDrawManager::Render(ID3D12GraphicsCommandList* cmdList, UINT frameIndex)
{
    if (_requests.empty()) return;
    if (frameIndex >= 2 || !_cbWorld[frameIndex]) return;

    auto camera = CameraComponent::get_main();
    auto renderer = Renderer::instance();
    ID3D12PipelineState* pso = renderer->get_pso("debug");
    ID3D12RootSignature* rootSig = renderer->get_root_signature("debug");

    cmdList->SetGraphicsRootSignature(rootSig);
    cmdList->SetPipelineState(pso);
    camera->update_shader_variables(cmdList, frameIndex);

    UINT cbSize = (sizeof(XMFLOAT4X4) + 255) & ~255;
    int shapeIdx = 0;

    for (const auto& req : _requests) {
        if (shapeIdx >= MAX_DEBUG_SHAPES) break;

        XMMATRIX world;
        XMMATRIX rot = XMMatrixRotationQuaternion(XMLoadFloat4(&req.rotation));
        XMMATRIX trans = XMMatrixTranslation(req.position.x, req.position.y, req.position.z);

        if (req.shapeType == common::packet::DebugShapeType::SPHERE) {
            float radius = req.extents.x;
            world = XMMatrixScaling(radius, radius, radius) * trans;
        }
        else if (req.shapeType == common::packet::DebugShapeType::BOX) {
            world = XMMatrixScaling(req.extents.x, req.extents.y, req.extents.z) * rot * trans;
        }
        else if (req.shapeType == common::packet::DebugShapeType::CAPSULE) {
            // Capsule: extents.x = radius, extents.y = half-height
            // Unit capsule is height 2.0 (cylinder 1.0 + caps 1.0), radius 1.0
            // We scale X,Z by radius, and Y by half-height
            world = XMMatrixScaling(req.extents.x, req.extents.y, req.extents.x) * rot * trans;
        }

        XMFLOAT4X4* pMapped = (XMFLOAT4X4*)((BYTE*)_mappedWorld[frameIndex] + (cbSize * shapeIdx));
        XMStoreFloat4x4(pMapped, XMMatrixTranspose(world));

        D3D12_GPU_VIRTUAL_ADDRESS cbAddr = _cbWorld[frameIndex]->GetGPUVirtualAddress() + (cbSize * shapeIdx);
        cmdList->SetGraphicsRootConstantBufferView(0, cbAddr);

        if (req.shapeType == common::packet::DebugShapeType::BOX) {
            cmdList->IASetVertexBuffers(0, 1, &_boxVBView);
            cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
            cmdList->DrawInstanced(_boxVertexCount, 1, 0, 0);
        }
        else if (req.shapeType == common::packet::DebugShapeType::SPHERE) {
            cmdList->IASetVertexBuffers(0, 1, &_sphereVBView);
            cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
            cmdList->DrawInstanced(_sphereVertexCount, 1, 0, 0);
        }
        else if (req.shapeType == common::packet::DebugShapeType::CAPSULE) {
            cmdList->IASetVertexBuffers(0, 1, &_capsuleVBView);
            cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
            cmdList->DrawInstanced(_capsuleVertexCount, 1, 0, 0);
        }
        shapeIdx++;
    }

    _remoteLineVertices.clear();

    for (const auto& shape : _remoteShapes) {
        XMMATRIX world = XMMatrixRotationQuaternion(XMLoadFloat4((XMFLOAT4*)&shape.rot)) *
            XMMatrixTranslation(shape.pos.x, shape.pos.y, shape.pos.z);

        for (size_t i = 0; i < shape.triangles.size(); i += 3) {
            XMVECTOR v0 = XMVector3Transform(XMLoadFloat3((XMFLOAT3*)&shape.triangles[i]), world);
            XMVECTOR v1 = XMVector3Transform(XMLoadFloat3((XMFLOAT3*)&shape.triangles[i + 1]), world);
            XMVECTOR v2 = XMVector3Transform(XMLoadFloat3((XMFLOAT3*)&shape.triangles[i + 2]), world);

            // 삼각형의 세 변을 선으로 추가
            _remoteLineVertices.push_back({ *(XMFLOAT3*)&v0 }); _remoteLineVertices.push_back({ *(XMFLOAT3*)&v1
                });
            _remoteLineVertices.push_back({ *(XMFLOAT3*)&v1 }); _remoteLineVertices.push_back({ *(XMFLOAT3*)&v2
                });
            _remoteLineVertices.push_back({ *(XMFLOAT3*)&v2 }); _remoteLineVertices.push_back({ *(XMFLOAT3*)&v0
                });
        }
    }

    if (!_remoteLineVertices.empty()) {
        // 1. 데이터를 GPU 업로드 버퍼로 복사
        void* pData = nullptr;
        _remoteLineVB->Map(0, nullptr, &pData);
        memcpy(pData, _remoteLineVertices.data(), sizeof(DebugVertex) * _remoteLineVertices.size());
        _remoteLineVB->Unmap(0, nullptr);

        // 2. 월드 행렬을 Identity로 설정 (이미 월드 좌표로 변환했으므로)
        XMMATRIX identity = XMMatrixIdentity();
        XMFLOAT4X4* pMapped = (XMFLOAT4X4*)((BYTE*)_mappedWorld[frameIndex] + (cbSize * shapeIdx));
        XMStoreFloat4x4(pMapped, XMMatrixTranspose(identity));
        cmdList->SetGraphicsRootConstantBufferView(0, _cbWorld[frameIndex]->GetGPUVirtualAddress() + (cbSize *
            shapeIdx));

        // 3. 그리기
        cmdList->IASetVertexBuffers(0, 1, &_remoteLineVBView);
        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
        cmdList->DrawInstanced((UINT)_remoteLineVertices.size(), 1, 0, 0);
    }
}

void DebugDrawManager::CreateDynamicLineBuffer(ID3D12Device* device)
{
    // 최대 10만 개의 선 정점(약 5만 개 선)을 수용할 수 있는 공간 확보
    UINT bufferSize = sizeof(DebugVertex) * 100000;
    _remoteLineVB = CreateBufferResource(device, nullptr, nullptr, bufferSize,
        D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);

    _remoteLineVBView.BufferLocation = _remoteLineVB->GetGPUVirtualAddress();
    _remoteLineVBView.StrideInBytes = sizeof(DebugVertex);
    _remoteLineVBView.SizeInBytes = bufferSize;
}
void DebugDrawManager::CreateUnitBox(ID3D12Device* device)
{
    DebugVertex vertices[] = {
        {{-1,-1,-1}}, {{1,-1,-1}}, {{1,-1,-1}}, {{1,1,-1}}, {{1,1,-1}}, {{-1,1,-1}}, {{-1,1,-1}}, {{-1,-1,-1}},
        {{-1,-1,1}}, {{1,-1,1}}, {{1,-1,1}}, {{1,1,1}}, {{1,1,1}}, {{-1,1,1}}, {{-1,1,1}}, {{-1,-1,1}},
        {{-1,-1,-1}}, {{-1,-1,1}}, {{1,-1,-1}}, {{1,-1,1}}, {{1,1,-1}}, {{1,1,1}}, {{-1,1,-1}}, {{-1,1,1}}
    };
    _boxVertexCount = _countof(vertices);
    ID3D12Resource* res = CreateBufferResource(device, nullptr, vertices, sizeof(DebugVertex) * _boxVertexCount,
        D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);
    _boxVB.Attach(res);
    _boxVBView.BufferLocation = _boxVB->GetGPUVirtualAddress();
    _boxVBView.StrideInBytes = sizeof(DebugVertex);
    _boxVBView.SizeInBytes = sizeof(DebugVertex) * _boxVertexCount;
}

void DebugDrawManager::CreateUnitSphere(ID3D12Device* device)
{
    std::vector<DebugVertex> v;
    const int slices = 32;

    for (int i = 0; i < slices; ++i) {
        float a1 = (float)i / slices * DirectX::XM_2PI;
        float a2 = (float)(i + 1) / slices * DirectX::XM_2PI;

        v.push_back({ {cosf(a1), sinf(a1), 0.0f} });
        v.push_back({ {cosf(a2), sinf(a2), 0.0f} });

        v.push_back({ {cosf(a1), 0.0f, sinf(a1)} });
        v.push_back({ {cosf(a2), 0.0f, sinf(a2)} });

        v.push_back({ {0.0f, cosf(a1), sinf(a1)} });
        v.push_back({ {0.0f, cosf(a2), sinf(a2)} });
    }

    _sphereVertexCount = (UINT)v.size();
    ID3D12Resource* res = CreateBufferResource(device, nullptr, v.data(),
        sizeof(DebugVertex) * _sphereVertexCount,
        D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);
    _sphereVB.Attach(res);
    _sphereVBView.BufferLocation = _sphereVB->GetGPUVirtualAddress();
    _sphereVBView.StrideInBytes = sizeof(DebugVertex);
    _sphereVBView.SizeInBytes = sizeof(DebugVertex) * _sphereVertexCount;
}

void DebugDrawManager::CreateUnitCapsule(ID3D12Device* device)
{
    std::vector<DebugVertex> v;
    const int slices = 16;
    
    // Cylinder part (height from -0.5 to 0.5)
    for (int i = 0; i < slices; ++i) {
        float a1 = (float)i / slices * DirectX::XM_2PI;
        float a2 = (float)(i + 1) / slices * DirectX::XM_2PI;
        
        // Vertical lines
        if (i % 4 == 0) {
            v.push_back({ {cosf(a1), -0.5f, sinf(a1)} });
            v.push_back({ {cosf(a1),  0.5f, sinf(a1)} });
        }
        
        // Top/Bottom rings
        v.push_back({ {cosf(a1), -0.5f, sinf(a1)} });
        v.push_back({ {cosf(a2), -0.5f, sinf(a2)} });
        v.push_back({ {cosf(a1),  0.5f, sinf(a1)} });
        v.push_back({ {cosf(a2),  0.5f, sinf(a2)} });
    }
    
    // Top Hemisphere (Cap)
    for (int i = 0; i < slices; ++i) {
        float a1 = (float)i / slices * DirectX::XM_2PI;
        float a2 = (float)(i + 1) / slices * DirectX::XM_2PI;
        
        // Latitudinal lines
        for (int j = 0; j < 8; ++j) {
            float p1 = (float)j / 16 * DirectX::XM_PI;
            float p2 = (float)(j + 1) / 16 * DirectX::XM_PI;
            
            // Draw arcs for the cap (simplified)
            if (i % 4 == 0) {
                v.push_back({ {cosf(a1)*sinf(p1), 0.5f + cosf(p1), sinf(a1)*sinf(p1)} });
                v.push_back({ {cosf(a1)*sinf(p2), 0.5f + cosf(p2), sinf(a1)*sinf(p2)} });
                
                v.push_back({ {cosf(a1)*sinf(p1), -0.5f - cosf(p1), sinf(a1)*sinf(p1)} });
                v.push_back({ {cosf(a1)*sinf(p2), -0.5f - cosf(p2), sinf(a1)*sinf(p2)} });
            }
        }
    }

    _capsuleVertexCount = (UINT)v.size();
    ID3D12Resource* res = CreateBufferResource(device, nullptr, v.data(),
        sizeof(DebugVertex) * _capsuleVertexCount,
        D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);
    _capsuleVB.Attach(res);
    _capsuleVBView.BufferLocation = _capsuleVB->GetGPUVirtualAddress();
    _capsuleVBView.StrideInBytes = sizeof(DebugVertex);
    _capsuleVBView.SizeInBytes = sizeof(DebugVertex) * _capsuleVertexCount;
}
