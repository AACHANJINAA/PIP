#include "stdafx.h"
#include "DebugDrawManager.h"

#include "Renderer.h"
const int MAX_DEBUG_SHAPES = 100;
void DebugDrawManager::Initialize(ID3D12Device* device)
{
	CreateUnitBox(device);
	CreateUnitSphere(device);

	UINT cbSize = (sizeof(XMFLOAT4X4) + 255) & ~255; // 256 바이트 정렬
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

	// [추가] 안전 장치: 버퍼가 없으면 그냥 나갑니다.
	if (frameIndex >= 2 || !_cbWorld[frameIndex]) return;

	//static float logTimer = 0;
	//logTimer += 0.016f; // 대략적인 deltaTime
	//if (logTimer > 1.0f) {
	//	CLOG("[DEBUG_DRAW] Rendering " << _requests.size() << " shapes. FrameIndex=" << frameIndex);
	//	logTimer = 0;
	//}

	auto camera = CameraComponent::get_main();
	auto renderer = Renderer::instance();
	ID3D12PipelineState* pso = renderer->get_pso("debug");
	ID3D12RootSignature* rootSig = renderer->get_root_signature("debug"); // 전용 시그니처 가져오기

	cmdList->SetGraphicsRootSignature(rootSig);
	cmdList->SetPipelineState(pso);
	camera->update_shader_variables(cmdList, frameIndex); // b1 바인딩

	UINT cbSize = (sizeof(XMFLOAT4X4) + 255) & ~255;
	int shapeIdx = 0;

	for (const auto& req : _requests) {
		if (shapeIdx >= MAX_DEBUG_SHAPES) break;

		XMMATRIX world;
		XMMATRIX rot = XMMatrixRotationQuaternion(XMLoadFloat4(&req.rotation));
		XMMATRIX trans = XMMatrixTranslation(req.position.x, req.position.y, req.position.z);

		if (req.shapeType == common::packet::DebugShapeType::SPHERE) {
			// [수정] 구체는 extents.x(반경)를 모든 축에 동일하게 적용 (균등 스케일링)
			float radius = req.extents.x;
			world = XMMatrixScaling(radius, radius, radius) * trans;
		}
		else if (req.shapeType == common::packet::DebugShapeType::BOX) {
			// 박스는 x, y, z 각각의 반폭(extents)을 적용
			world = XMMatrixScaling(req.extents.x, req.extents.y, req.extents.z) * rot * trans;
		}
		else if (req.shapeType == common::packet::DebugShapeType::CAPSULE) {
			// 캡슐은 보통 x=반경, y=절반높이(Half-Height)를 사용합니다.
			// 현재는 박스 메쉬를 재활용하거나 전용 로직 필요 (일단 구체처럼 처리 가능)
			world = XMMatrixScaling(req.extents.x, req.extents.y, req.extents.x) * rot * trans;
		}

		// 상수 버퍼 업데이트 및 그리기
		XMFLOAT4X4* pMapped = (XMFLOAT4X4*)((BYTE*)_mappedWorld[frameIndex] + (cbSize * shapeIdx));
		XMStoreFloat4x4(pMapped, XMMatrixTranspose(world));

		D3D12_GPU_VIRTUAL_ADDRESS cbAddr = _cbWorld[frameIndex]->GetGPUVirtualAddress() + (cbSize * shapeIdx);
		cmdList->SetGraphicsRootConstantBufferView(0, cbAddr);

		if (req.shapeType == common::packet::DebugShapeType::BOX) {
			cmdList->IASetVertexBuffers(0, 1, &_boxVBView);
			cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
			cmdList->DrawInstanced(_boxVertexCount, 1, 0, 0);
		}
		else { // SPHERE 및 CAPSULE
			cmdList->IASetVertexBuffers(0, 1, &_sphereVBView);
			cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
			cmdList->DrawInstanced(_sphereVertexCount, 1, 0, 0);
		}
		shapeIdx++;
	}
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
	const int slices = 32; // 선을 더 촘촘하게 (32개면 충분히 둥금)

	for (int i = 0; i < slices; ++i) {
		float a1 = (float)i / slices * DirectX::XM_2PI;
		float a2 = (float)(i + 1) / slices * DirectX::XM_2PI;

		// 1. XY 평면 원 (정면에서 보이는 원)
		v.push_back({ {cosf(a1), sinf(a1), 0.0f} });
		v.push_back({ {cosf(a2), sinf(a2), 0.0f} });

		// 2. XZ 평면 원 (위에서 내려다보는 원)
		v.push_back({ {cosf(a1), 0.0f, sinf(a1)} });
		v.push_back({ {cosf(a2), 0.0f, sinf(a2)} });

		// 3. YZ 평면 원 (옆에서 보는 원)
		v.push_back({ {0.0f, cosf(a1), sinf(a1)} });
		v.push_back({ {0.0f, cosf(a2), sinf(a2)} });
	}

	_sphereVertexCount = (UINT)v.size(); // 약 192개 정점

	// 제공해주신 CreateBufferResource로 버퍼 생성
	ID3D12Resource* res = CreateBufferResource(device, nullptr, v.data(),
		sizeof(DebugVertex) * _sphereVertexCount, // 사이즈 타입 체크!
		D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);

	_sphereVB.Attach(res);

	// View 정보 업데이트 (중요!)
	_sphereVBView.BufferLocation = _sphereVB->GetGPUVirtualAddress();
	_sphereVBView.StrideInBytes = sizeof(DebugVertex);
	_sphereVBView.SizeInBytes = sizeof(DebugVertex) * _sphereVertexCount;

}
