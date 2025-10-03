#include "stdafx.h"
#include "CameraComponent.h"

#include "GameFramework.h"

CameraComponent::CameraComponent() :
	_fov(90.0f),
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
}
CameraComponent::~CameraComponent()
{
    // 이 컴포넌트가 파괴될 때, 자신이 메인 카메라였다면 정적 포인터를 null로 설정합니다.
    if (_mainCamera == this)
    {
        _mainCamera = nullptr;
    }
}

void CameraComponent::initialize()
{
    ID3D12Device* device = GameFramework::Instance()->device().Get();
    if (!device)
    {
        CERROR("CameraComponent::initialize: Device is null.");
        return;
    }

    // 상수 버퍼 생성
    _cbCamera = ::CreateBufferResource(device, nullptr, nullptr, sizeof(CB_CAMERA_INFO),
        D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);

    // 상수 버퍼 매핑
    D3D12_RANGE readRange{ 0, 0 };
    _cbCamera->Map(0, &readRange, reinterpret_cast<void**>(&_mappedCbCamera));

    // 렌즈 초기 설정으로 프로젝션 행렬 생성
    set_lens(_fov, _aspect, _near, _far);
}
void CameraComponent::release()
{
    if (_cbCamera)
    {
        _cbCamera->Unmap(0, nullptr);
        _cbCamera.Reset(); // ComPtr이므로 Reset()으로 리소스 해제
    }
}
void CameraComponent::update_shader_variables(ID3D12GraphicsCommandList* commandList)
{
    if (!_mappedCbCamera) return;

    // 뷰와 프로젝션 행렬을 셰이더가 사용할 수 있도록 Transpose하여 상수 버퍼에 복사합니다.
    XMStoreFloat4x4(&_mappedCbCamera->_view, XMMatrixTranspose(XMLoadFloat4x4(&_viewMatrix)));
    XMStoreFloat4x4(&_mappedCbCamera->_projection,
        XMMatrixTranspose(XMLoadFloat4x4(&_projectionMatrix)));

    // 카메라의 월드 위치를 상수 버퍼에 복사합니다.
    if (game_object() && game_object()->transform())
    {
        XMFLOAT3 pos = game_object()->transform()->position();
        _mappedCbCamera->_position = XMFLOAT4(pos.x, pos.y, pos.z, 1.0f);
    }

    // 루트 시그니처의 1번 파라미터(b1)에 카메라 상수 버퍼를 바인딩합니다.
    D3D12_GPU_VIRTUAL_ADDRESS cbGpuAddress = _cbCamera->GetGPUVirtualAddress();
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

    XMVECTOR pos = XMLoadFloat3(&transform->position());
    XMVECTOR look = XMLoadFloat3(&transform->forward());
    XMVECTOR up = XMLoadFloat3(&transform->up());

    XMStoreFloat4x4(&_viewMatrix, XMMatrixLookToLH(pos, look, up));

    // [추가] 뷰 행렬이 변경되었으므로 프러스텀도 업데이트합니다.
    XMMATRIX view = XMLoadFloat4x4(&_viewMatrix);
    XMMATRIX proj = XMLoadFloat4x4(&_projectionMatrix);
    BoundingFrustum::CreateFromMatrix(_frustum, proj);
    XMMATRIX viewInverse = XMMatrixInverse(nullptr, view);
    _frustum.Transform(_frustum, viewInverse);
}
