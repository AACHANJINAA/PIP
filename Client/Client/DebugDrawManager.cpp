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
    // [추가] 서버 전송용 동적 라인 버퍼 생성 (약 20만개 정점 확보)
    UINT dynamicBufferSize = sizeof(DebugVertex) * 200000;
    _remoteLineVB = CreateBufferResource(device, nullptr, nullptr, dynamicBufferSize,
        D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);
    _remoteLineVBView.BufferLocation = _remoteLineVB->GetGPUVirtualAddress();
    _remoteLineVBView.StrideInBytes = sizeof(DebugVertex);
    _remoteLineVBView.SizeInBytes = dynamicBufferSize;
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

void DebugDrawManager::AddRemoteDebugShape(RemoteDebugShape&& shape)
{
	_remoteShapes.push_back(std::move(shape));
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
    if (_requests.empty() && _remoteShapes.empty()) return;
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

    if (!_remoteShapes.empty() && shapeIdx < MAX_DEBUG_SHAPES) {
        RenderRemoteShape(cmdList, frameIndex, cbSize, shapeIdx);
    }
    
}

void DebugDrawManager::LoadLocalDebugShape(const std::string& jsonPath, const std::string& targetActor,
	const std::string& targetMesh)
{
    std::ifstream file(jsonPath);
    if (!file.is_open()) return;

    nlohmann::json root;
    file >> root;

    // 1. 메쉬 라이브러리에서 targetMesh의 Convex 데이터만 추출
    std::vector<std::vector<common::Vec3>> meshConvexParts;
    if (root.contains("MeshLibrary") && root["MeshLibrary"].contains(targetMesh)) {
        const auto& colData = root["MeshLibrary"][targetMesh]["CollisionData"];
        if (colData.contains("ConvexHulls")) {
            for (const auto& hull : colData["ConvexHulls"]) {
                std::vector<common::Vec3> points;
                for (const auto& v : hull["Vertices"]) {
                    points.push_back({ v.value("X", 0.0f), v.value("Y", 0.0f), v.value("Z", 0.0f) });
                }
                meshConvexParts.push_back(points);
            }
        }

        if (colData.contains("Boxes"))
        {
            for (const auto& box : colData["Boxes"]) {
                // Box는 8개의 꼭짓점으로 변환 (단순 선 그리기를 위해)
                std::vector<common::Vec3> points;
                XMVECTOR center = ToCVec(box["Center"]);
                float ExtentX = box["ExtentX"];
				float ExtentY = box["ExtentY"];
				float ExtentZ = box["ExtentZ"];
				XMVECTOR extents = XMVectorSet(ExtentX, ExtentY, ExtentZ, 0);
				auto rot = box["Rotation"];
				auto rotQuat = ToCQuat(rot);
                // 8 corners of the box
                for (int x = -1; x <= 1; x += 2) {
                    for (int y = -1; y <= 1; y += 2) {
                        for (int z = -1; z <= 1; z += 2) {
                            XMVECTOR localPos = XMVectorSet(extents.m128_f32[0] * x, extents.m128_f32[1] * y, extents.m128_f32[2] * z, 0);
                            XMVECTOR rotatedPos = XMVector3Rotate(localPos, rotQuat);
                            XMVECTOR finalPos = XMVectorAdd(center, rotatedPos);
                            points.push_back({ finalPos.m128_f32[0], finalPos.m128_f32[1], finalPos.m128_f32[2] });
                        }
                    }
                }
				meshConvexParts.push_back(points);
			}
	        
        }
    }

    if (meshConvexParts.empty()) return;

    // 2. 인스턴스에서 targetActor 찾기
    if (root.contains("Instances")) {
        for (const auto& inst : root["Instances"]) {
            if (inst.value("ActorName", "") != targetActor) continue;

            XMVECTOR actorPos = ToCVec(inst["WorldPos"]);
            XMVECTOR actorRot = ToCQuat(inst["WorldRot"]);

            for (const auto& part : inst["Parts"]) {
                if (part.value("MeshName", "") != targetMesh) continue;

                XMVECTOR relPos = ToCVec(part["RelPos"]);
                XMVECTOR relRot = ToCQuat(part["RelRot"]);

                // 최종 월드 트랜스폼 계산 (서버와 동일한 공식)
                XMVECTOR finalPos = XMVectorAdd(actorPos, XMVector3Rotate(relPos, actorRot));
                XMVECTOR finalRot = XMQuaternionNormalize(XMQuaternionMultiply(actorRot, relRot));

                // 렌더링용 데이터 생성 (기존 _remoteShapes 구조 재활용)
                for (const auto& points : meshConvexParts) {
                    RemoteDebugShape rs;
                    XMStoreFloat3((XMFLOAT3*)&rs.pos, finalPos);
                    XMStoreFloat4((XMFLOAT4*)&rs.rot, finalRot);

                    // Convex의 정점들을 삼각형 형태로 변환 (단순 선 그리기를 위해)
                    // 실제 Convex를 정확히 그리려면 Hull 알고리즘이 필요하지만,
                    // 여기서는 모든 점을 원점과 잇는 방식으로 대략적인 형태만 확인합니다.
                    if (points.size() >= 3) {
                        for (size_t i = 1; i < points.size() - 1; ++i) {
                            rs.triangles.push_back(points[0]);
                            rs.triangles.push_back(points[i]);
                            rs.triangles.push_back(points[i + 1]);
                        }
                    }
                    _remoteShapes.push_back(std::move(rs));
                }
            }
        }
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

void DebugDrawManager::RenderRemoteShape(ID3D12GraphicsCommandList* cmdList, UINT frameIndex, UINT cbSize, int& shapeIdx)
{
    if (_remoteShapes.empty()) return;

    _remoteLineVertices.clear();

    for (const auto& shape : _remoteShapes) {
        XMMATRIX world = XMMatrixRotationQuaternion(XMLoadFloat4((XMFLOAT4*)&shape.rot)) *
            XMMatrixTranslation(shape.pos.x, shape.pos.y, shape.pos.z);

        // [수정] 정점이 3개 미만으로 남았을 경우 루프 종료 (안전 장치)
        for (size_t i = 0; i + 2 < shape.triangles.size(); i += 3)
        {
            XMVECTOR v0 = XMVector3Transform(XMLoadFloat3((XMFLOAT3*)&shape.triangles[i]), world);
            XMVECTOR v1 = XMVector3Transform(XMLoadFloat3((XMFLOAT3*)&shape.triangles[i + 1]), world);
            XMVECTOR v2 = XMVector3Transform(XMLoadFloat3((XMFLOAT3*)&shape.triangles[i + 2]), world);

            DebugVertex dv0, dv1, dv2;
            XMStoreFloat3(&dv0.pos, v0);
            XMStoreFloat3(&dv1.pos, v1);
            XMStoreFloat3(&dv2.pos, v2);

            _remoteLineVertices.push_back(dv0); _remoteLineVertices.push_back(dv1);
            _remoteLineVertices.push_back(dv1); _remoteLineVertices.push_back(dv2);
            _remoteLineVertices.push_back(dv2); _remoteLineVertices.push_back(dv0);

            if (_remoteLineVertices.size() >= 199000) break; // 버퍼 오버플로우 방지
        }
    }

    // [핵심] 모든 인스턴스의 선들을 하나의 정점 버퍼에 모아서 단 한 번의 호출로 그립니다!
    if (!_remoteLineVertices.empty()) {
        void* pData = nullptr;
        if (SUCCEEDED(_remoteLineVB->Map(0, nullptr, &pData))) {
            memcpy(pData, _remoteLineVertices.data(), sizeof(DebugVertex) * _remoteLineVertices.size());
            _remoteLineVB->Unmap(0, nullptr);
        }

        XMMATRIX identity = XMMatrixIdentity();
        XMFLOAT4X4* pMapped = (XMFLOAT4X4*)((BYTE*)_mappedWorld[frameIndex] + (cbSize * shapeIdx));
        XMStoreFloat4x4(pMapped, XMMatrixTranspose(identity));

        cmdList->SetGraphicsRootConstantBufferView(0, _cbWorld[frameIndex]->GetGPUVirtualAddress() + (cbSize *
            shapeIdx));
        cmdList->IASetVertexBuffers(0, 1, &_remoteLineVBView);
        cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
        cmdList->DrawInstanced((UINT)_remoteLineVertices.size(), 1, 0, 0);

        shapeIdx++;
    }
}
