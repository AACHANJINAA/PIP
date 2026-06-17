#pragma once
#include "Component.h"
#include "TransformComponent.h" // Required for getting transform data

// CB_CAMERA_INFO is needed by shaders, so it's good to have it accessible.
struct CB_CAMERA_INFO
{
    XMFLOAT4X4  _view;
    XMFLOAT4X4  _projection;
    XMFLOAT4    _position;
};

// 역할: 카메라의 데이터(프로젝션)와 기능(뷰 행렬 계산)을 담당하는 순수 데이터 컴포넌트.
// Component를 상속하며, Behaviour가 아니므로 자동 update가 호출되지 않습니다.
// 뷰 행렬의 갱신은 렌더링 직전 Renderer에 의해 명시적으로 제어됩니다.
class CameraComponent : public Component
{
public:
    CameraComponent(float fov = 45.f);
    virtual ~CameraComponent();

    // 역할 이전 (from CCamera::UpdateShaderVariables):
    // 셰이더에 뷰/프로젝션 행렬 업데이트
    void update_shader_variables(ID3D12GraphicsCommandList* commandList, UINT frame_index);

    // 역할 이전 (from CCamera::SetViewportsAndScissorRects):
    // 뷰포트 및 시저렉트 설정
    void set_viewports_and_scissor_rects(ID3D12GraphicsCommandList* commandList);

    // 역할 이전 (from CCamera::GenerateProjectionMatrix, CCamera::SetFOVAngle):
    // 렌즈 설정 (프로젝션 행렬 생성)
    void set_lens(float fov, float aspect, float near_plane, float far_plane);

    // 역할 이전 (from CCamera::Update):
    // GameObject의 Transform을 기준으로 뷰 행렬을 다시 계산합니다.
    void recalculate_view_matrix();

    // 해상도가 변경될 때 뷰포트, 시저 렉트, 투영 행렬을 다시 설정하는 함수
    void update_resolution(UINT width, UINT height);

    // --- Getters ---
    const XMFLOAT4X4& view_matrix() const { return _viewMatrix; }
    const XMFLOAT4X4& projection_matrix() const { return _projectionMatrix; }
    const BoundingFrustum& frustum() const { return _frustum; }

    // [추가] 카메라 쉐이크를 위한 오프셋 설정
    void set_shake_offset(const DirectX::XMFLOAT3& offset) { _shakeOffset = offset; }

    // --- Main Camera Management ---
	void set_main_camera() { _mainCamera = this; }
    static CameraComponent* get_main() { return _mainCamera; }
    ID3D12Resource* get_cb_skybox() const { return _cbSkybox.Get(); }

private:
    // 역할 이전 (from CCamera):
    // CCamera가 가지고 있던 뷰/프로젝션 행렬입니다. (컨벤션: _ + camelCase)
    XMFLOAT4X4 _viewMatrix;
    XMFLOAT4X4 _projectionMatrix;
    BoundingFrustum _frustum;

    DirectX::XMFLOAT3 _shakeOffset = { 0.0f, 0.0f, 0.0f };
    // 역할 이전 (from CCamera):
    // 뷰포트와 시저렉트 정보입니다.
    D3D12_VIEWPORT  _viewport;
    D3D12_RECT      _scissorRect;

    // 역할 이전 (from CCamera):
    // 렌즈 설정을 위한 변수들입니다.
    float _fov;
    float _aspect;
    float _near;
    float _far;

    // 역할 이전 (from CCamera):
    // 상수 버퍼 리소스 및 매핑된 포인터
    std::array<ComPtr<ID3D12Resource>, 2> _cbCamera;
    std::array<CB_CAMERA_INFO*, 2> _mappedCbCamera;

    struct CB_SKYBOX_INFO
    {
        XMFLOAT4X4 _viewNoTranslate;
        XMFLOAT4X4 _projection;
    };

    ComPtr<ID3D12Resource> _cbSkybox;
    CB_SKYBOX_INFO* _mappedCbSkybox = nullptr;
   
public:
    // --- Main Camera ---
   // 렌더러가 쉽게 접근할 수 있도록 주 카메라를 가리키는 정적 포인터
    static CameraComponent* _mainCamera;
};