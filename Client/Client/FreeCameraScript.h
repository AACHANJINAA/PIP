#pragma once
#include "ScriptComponent.h"

// Forward declaration
class CameraComponent;

// 역할: 사용자의 입력을 받아 카메라(가 부착된 GameObject)를 이동시키고 회전시키는 스크립트.
//       자신이 부착된 GameObject에 CameraComponent가 있도록 보장합니다.
class FreeCameraScript : public ScriptComponent
{
public:
    // 이 스크립트는 CameraComponent가 반드시 필요하다고 시스템에 알립니다.
    using required_components = std::tuple<CameraComponent>;

    FreeCameraScript();
    virtual ~FreeCameraScript() = default;

    // 이 스크립트가 활성화될 때, 필요한 CameraComponent를 확인하고 설정합니다.
    virtual void awake() override;

    // 매 프레임 입력을 처리합니다.
    virtual void update(float delta_time) override;


    virtual void late_update(float delta_time) override;

private:
    // 역할 이전 (from FreeCamera):
    // 마우스 입력을 처리하여 카메라를 회전시킵니다.
    void process_mouse_input(float delta_time);

    // 역할 이전 (from FreeCamera):
    // 키보드 입력을 처리하여 카메라를 이동시킵니다.
    void process_keyboard_input(float delta_time);

    // 카메라의 이동 및 회전 속도
    float _moveSpeed;
    float _rotationSpeed;

	// 3인칭 에서의 offset 거리
	float _thirdPersonOffsetDistance_top = 0.0f;
    float _thirdPersonOffsetDistance_back = -10.0f;

    // 자주 접근하게 될 CameraComponent에 대한 캐시된 포인터
    CameraComponent* _cameraComponent;
};