#include "stdafx.h"
#include "CameraComponent.h"

#include "GameFramework.h"

CameraComponent* CameraComponent::_mainCamera = nullptr;

CameraComponent::CameraComponent(float fov) :
	_fov(fov),
	_aspect(static_cast<float>(FRAME_BUFFER_WIDTH) / static_cast<float>(FRAME_BUFFER_HEIGHT)),
	_near(0.1f), _far(5000.0f)
{
	// 역할 이전 (from CCamera constructor):
	// 행렬들을 단위 행렬로 초기화합니다.
	XMStoreFloat4x4(&_viewMatrix, XMMatrixIdentity());
	XMStoreFloat4x4(&_projectionMatrix, XMMatrixIdentity());

	// 뷰포트와 시저렉트를 기본값으로 설정합니다.
	_viewport = { 0, 0, static_cast<float>(FRAME_BUFFER_WIDTH), static_cast<float>(FRAME_BUFFER_HEIGHT),
		0.0f, 1.0f };
	_scissorRect = { 0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT };

	// 이 컴포넌트가 생성될 때, 메인 카메라가 없다면 자신을 메인 카메라로 설정합니다.
	if (!_mainCamera)
	{
		_mainCamera = this;
	}
	// --- initialize() 로직 시작 ---
	ID3D12Device* device = GameFramework::instance()->device().Get();
	if (!device)
	{
		CERROR("CameraComponent: Device is null.");
		return;
	}
	for (UINT i = 0; i < _cbCamera.size(); ++i)
	{	
		_cbCamera[i] = ::CreateBufferResource(device, nullptr, nullptr, sizeof(CB_CAMERA_INFO),
		D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);

		if (!_cbCamera[i])
		{
			CERROR("CameraComponent: Failed to create constant buffer.");
			return;
		}
		D3D12_RANGE read_range_cb{ 0, 0 };

		_cbCamera[i]->Map(0, &read_range_cb, reinterpret_cast<void**>(&_mappedCbCamera[i]));
	}

	_cbSkybox = ::CreateBufferResource(device, nullptr, nullptr, sizeof(CB_SKYBOX_INFO), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);

	D3D12_RANGE read_range_sb{ 0, 0 };
	_cbSkybox->Map(0, &read_range_sb, reinterpret_cast<void**>(&_mappedCbSkybox));

	set_lens(_fov, _aspect, _near, _far);
	// --- initialize() 로직 끝 ---
}
CameraComponent::~CameraComponent()
{
	if (_mainCamera == this)
	{
		_mainCamera = nullptr;
	}

	// --- release() 로직 시작 ---
	for (UINT i = 0; i < _cbCamera.size(); ++i)
	{
		if (_cbCamera[i])
		{
			_cbCamera[i]->Unmap(0, nullptr);
		}
	}
	if (_cbSkybox)
	{
		_cbSkybox->Unmap(0, nullptr);
	}


	// --- release() 로직 끝 ---
}

void CameraComponent::update_shader_variables(ID3D12GraphicsCommandList* commandList, UINT frame_index)
{
	if (!_mappedCbCamera[frame_index]) return;

	// 뷰와 프로젝션 행렬을 셰이더가 사용할 수 있도록 Transpose하여 상수 버퍼에 복사합니다.
	XMStoreFloat4x4(&_mappedCbCamera[frame_index]->_view, XMMatrixTranspose(XMLoadFloat4x4(&_viewMatrix)));
	XMStoreFloat4x4(&_mappedCbCamera[frame_index]->_projection,
	XMMatrixTranspose(XMLoadFloat4x4(&_projectionMatrix)));

	// 카메라의 월드 위치를 상수 버퍼에 복사합니다.
	if (game_object() && game_object()->transform())
	{
		XMFLOAT3 pos = game_object()->transform()->position();
		_mappedCbCamera[frame_index]->_position = XMFLOAT4(pos.x, pos.y, pos.z, 1.0f);
	}

	D3D12_GPU_VIRTUAL_ADDRESS cbGpuAddress = _cbCamera[frame_index]	->GetGPUVirtualAddress();
	commandList->SetGraphicsRootConstantBufferView(1, cbGpuAddress);
}
void CameraComponent::set_viewports_and_scissor_rects(ID3D12GraphicsCommandList* commandList)
{
	commandList->RSSetViewports(1, &_viewport);
	commandList->RSSetScissorRects(1, &_scissorRect);
}
void CameraComponent::set_lens(float fov, float aspect, float near_plane, float far_plane)
{
	_fov = fov;
	_aspect = aspect;
	_near = near_plane;
	_far = far_plane;

	// 원근 투영 행렬을 생성합니다.
	_projectionMatrix = Matrix4x4::PerspectiveFovLH(XMConvertToRadians(_fov), _aspect, _near, _far);
}
void CameraComponent::recalculate_view_matrix()
{
	TransformComponent* transform = this->game_object()->transform().get();
	if (!transform) return;

	f3 f3pos = transform->position();
	f3 f3look = transform->forward();
	f3 f3up = transform->up();

	XMVECTOR pos = XMLoadFloat3(&f3pos);
	XMVECTOR look = XMLoadFloat3(&f3look);
	XMVECTOR up = XMLoadFloat3(&f3up);

	XMStoreFloat4x4(&_viewMatrix, XMMatrixLookToLH(pos, look, up));
	// 스카이박스용 뷰 행렬 (이동 성분 제거) 계산 및 상수 버퍼에 복사

	if (_mappedCbSkybox)
	{
		XMMATRIX view_no_translate = XMLoadFloat4x4(&_viewMatrix);
		view_no_translate.r[3] = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f); // Translation 부분을 0으로 설정
	    XMStoreFloat4x4(&_mappedCbSkybox->_viewNoTranslate, XMMatrixTranspose(view_no_translate));

		XMMATRIX proj = XMLoadFloat4x4(&_projectionMatrix);
		XMStoreFloat4x4(&_mappedCbSkybox->_projection, XMMatrixTranspose(proj));
	}

	// [추가] 뷰 행렬이 변경되었으므로 프러스텀도 업데이트합니다.
	XMMATRIX view = XMLoadFloat4x4(&_viewMatrix);
	XMMATRIX proj = XMLoadFloat4x4(&_projectionMatrix);
	BoundingFrustum::CreateFromMatrix(_frustum, proj);
	XMMATRIX viewInverse = XMMatrixInverse(nullptr, view);
	_frustum.Transform(_frustum, viewInverse);
}

void CameraComponent::update_resolution(UINT width, UINT height)
{
	if (width == 0 || height == 0) return;

	// 1. 뷰포트와 시저 렉트를 새 해상도에 맞게 갱신
	_viewport.Width = static_cast<float>(width);
	_viewport.Height = static_cast<float>(height);
	_scissorRect.right = static_cast<LONG>(width);
	_scissorRect.bottom = static_cast<LONG>(height);

	// 2. 종횡비(Aspect Ratio)를 다시 계산하여 투영 행렬(Projection Matrix) 갱신
	float newAspect = static_cast<float>(width) / static_cast<float>(height);
	set_lens(_fov, newAspect, _near, _far);
}
