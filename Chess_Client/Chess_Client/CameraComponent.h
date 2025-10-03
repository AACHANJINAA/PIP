#include "Component.h"
class TransformComponent;

// 역할: 카메라의 데이터(프로젝션)와 기능(뷰 행렬 계산)을 담당하는 순수 데이터 컴포넌트.
// Component를 상속하며, Behaviour가 아니므로 자동 update가 호출되지 않습니다.
// 뷰 행렬의 갱신은 렌더링 직전 Renderer에 의해 명시적으로 제어됩니다.
class CameraComponent : public Component
{
private:
    // 역할 이전 (from CCamera):
    // CCamera가 가지고 있던 뷰/프로젝션 행렬입니다. (컨벤션: _ + camelCase)
    XMFLOAT4X4 _viewMatrix;
    XMFLOAT4X4 _projectionMatrix;

    // 역할 이전 (from CCamera):
    // 뷰포트와 시저렉트 정보입니다.
    D3D12_VIEWPORT _viewport;
    D3D12_RECT _scissorRect;

    // 역할 이전 (from CCamera):
    // 렌즈 설정을 위한 변수들입니다.
    float _fov;
    float _aspect;
    float _near;
    float _far;

public:
    CameraComponent();
    virtual ~CameraComponent() = default;

    // 역할: Renderer가 호출할 함수.
    // 이 컴포넌트가 부착된 GameObject의 Transform을 기준으로 뷰 행렬을 다시 계산합니다.
    // Behaviour의 update()와 달리, 렌더링 시점에만 호출되어 동기화를 보장합니다.
    void recalculate_view_matrix();

    // 역할 이전 (from CCamera::SetLens, 컨벤션: set_ prefix):
    // 프로젝션 행렬(원근 투영)을 설정하는 기능은 그대로 가져왔습니다.
    void set_lens(float fov, float aspect, float near_plane, float far_plane);

    // 컨벤션: Getter는 'get_' 접두사 없이, 변수명처럼, const 키워드 사용
    const XMFLOAT4X4& view_matrix() const { return _viewMatrix; }
    const XMFLOAT4X4& projection_matrix() const { return _projectionMatrix; }
};