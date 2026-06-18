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


	// 시네마틱 카메라 모드 설정 및 확인 함수
    // 시네마틱 모드로 넘어가면 카메라 조작 및 플레이어, UI 렌더를 제한
	void set_sinamatic_camera_mode(bool enable) { _isSinamaticCameraMode = enable; }
	bool is_sinamatic_camera_mode() const { return _isSinamaticCameraMode; }

	// [추가] 카메라 쉐이크를 위한 Trauma 추가 함수
	void add_trauma(float amount) {
		_trauma += amount;
		if (_trauma > 1.0f) _trauma = 1.0f;
	}

	// [추가] 줌 오프셋 제어 함수 (스킬 사용 시 줌아웃 효과)
	void set_dynamic_zoom_offset(float offset) { _dynamicZoomOffset = offset; }
	float get_dynamic_zoom_offset() const { return _dynamicZoomOffset; }

private:

	void free_camera_update(float delta_time);
	void player_camera_update(float delta_time);
	void player_camera_conflict_update(float delta_time); // 플레이어 카메라 모드일 때 충돌처리 및 최종 카메라 위치 계산하는 함수

	float _trauma = 0.0f; // 카메라 흔들림 정도 (0.0 ~ 1.0)
	float _maxShakeOffset = 0.5f; // 최대 흔들림 폭

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
    float _thirdPersonOffsetDistance_back = -5.0f;

    // 자주 접근하게 될 CameraComponent에 대한 캐시된 포인터
    CameraComponent* _cameraComponent;

    bool _isFreeCameraMode = false;

	bool _isSinamaticCameraMode = false; // 시네마틱 카메라 모드 여부

	float _dynamicZoomOffset = 0.0f; // 스킬 사용 시의 추가 줌 오프셋
};