#include "stdafx.h"
#include "CameraComponent.h"

#include "GameFramework.h"

CameraComponent* CameraComponent::_mainCamera = nullptr;

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
    // --- initialize() 로직 시작 ---
    ID3D12Device* device = GameFramework::Instance()->device().Get();
    if (!device)
    {
        CERROR("CameraComponent: Device is null.");
        return;
    }

    _cbCamera = ::CreateBufferResource(device, nullptr, nullptr, sizeof(CB_CAMERA_INFO),
        D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);

    if (!_cbCamera)
    {
        CERROR("CameraComponent: Failed to create constant buffer.");
        return;
    }

    D3D12_RANGE readRange{ 0, 0 };
    _cbCamera->Map(0, &readRange, reinterpret_cast<void**>(&_mappedCbCamera));

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
    if (_cbCamera)
    {
        _cbCamera->Unmap(0, nullptr);
        // _cbCamera는 ComPtr이므로, 소멸자가 호출될 때 자동으로 Reset()되어 리소스가 해제됩니다.
        // 따라서 명시적으로 _cbCamera.Reset()을 호출할 필요는 없습니다.
    }
    // --- release() 로직 끝 ---
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

    auto print_matrix = [](const char* name, const XMFLOAT4X4& matrix) {
        CLOG("--- " << name << " ---");
        CLOG(matrix.m[0][0] << ", " << matrix.m[0][1] << ", " << matrix.m[0][2] << ", " << matrix.m[0][3]);
        CLOG(matrix.m[1][0] << ", " << matrix.m[1][1] << ", " << matrix.m[1][2] << ", " << matrix.m[1][3]);
        CLOG(matrix.m[2][0] << ", " << matrix.m[2][1] << ", " << matrix.m[2][2] << ", " << matrix.m[2][3]);
        CLOG(matrix.m[3][0] << ", " << matrix.m[3][1] << ", " << matrix.m[3][2] << ", " << matrix.m[3][3]);
        CLOG("--------------------");
    };
    
     // 매 프레임 행렬 값을 출력합니다.
     static int frame_count = 0;
    if (frame_count < 10) { // 처음 10프레임만 출력
       print_matrix("View Matrix", _viewMatrix);
       print_matrix("Projection Matrix", _projectionMatrix);
       frame_count++;
    }    // 루트 시그니처의 1번 파라미터(b1)에 카메라 상수 버퍼를 바인딩합니다.
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
