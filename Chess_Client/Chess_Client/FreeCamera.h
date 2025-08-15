#pragma once
#include "Camera.h"
#include "GameObject.h"

enum class CAMERA_MODE : uint8_t
{
	CAMERA_FREE = 1 ,
	CAMERA_THIRD_PERSON = 2 ,
	CAMERA_END = 3 // 끝 번호임 의미X
};

inline CAMERA_MODE& operator++(CAMERA_MODE& mod)
{
	if (mod == CAMERA_MODE::CAMERA_END)
	{
		mod = CAMERA_MODE::CAMERA_FREE;
	}
	else
	{
		mod = static_cast<CAMERA_MODE>(static_cast<int>(mod) + 1);
	}
	return mod;
}


class FreeCamera : public Camera
{
public:
	FreeCamera() = default;
	~FreeCamera() = default;

	void SetPlayer(GameObject* pPlayer) { m_Player = pPlayer; }
	void SetCameraMode(CAMERA_MODE Mode) { m_NowMode = Mode; }
	CAMERA_MODE GetCameraMode() { return m_NowMode; }
	void SetOffset(float Offset) { m_Offset = Offset; }

	void UpdateAnimateCamera(float fElapsedTime); // 카메라 애니메이션

	void KeyInput(float fElapsedTime, HWND hwnd, UINT nMessageID,POINT ptOldCursorPos);

	void GetElapsedTime(float fElapsedTime) { m_ElapseTime = fElapsedTime; }

	void RotateMouseCamera(float X, float Y);

	void MoveForward(float Sign); // 부호넣기
	void MoveRight(float Sign); // 부호넣기
	void MoveUP(float Sign); // 부호넣기

private:
	// 플레이어를 가지고 있기
	GameObject* m_Player{};

	float m_BeforeX{-500.f};
	float m_BeforeY{ -500.f };
	float m_MoveSpeed{10.f};

	float m_NowOffset{0.5f}; // 현재 플레이어와의 거리
	float m_Offset{}; // 목표 거리

	float m_Move_Offset_Time{3.f}; // 목표 거리까지 가는 시간


	CAMERA_MODE m_NowMode{}; // 현재 플레이어 카메라 모드
	// 0번 : 자유, 1번 : 1플레이어, 2번 : 2플레이어
};

