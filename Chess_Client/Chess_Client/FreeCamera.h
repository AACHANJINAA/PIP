#pragma once
#include "Camera.h"
#include "GameObject.h"
class CFreeCamera : public CCamera
{
public:
	CFreeCamera() {};
	~CFreeCamera() {};

	void SetPlayer(CGameObject* pPlayer) { m_Player = pPlayer; }
	void SetCameraMode(size_t Mode) { m_NowMode = Mode; }
	size_t GetCameraMode() { return m_NowMode; }
	void SetOffset(float Offset) { m_Offset = Offset; }

	void UpdateAnimateCamera(float fElapsedTime); // 카메라 애니메이션

	void KeyInput(float fElapsedTime, HWND hwnd, UINT nMessageID,POINT ptOldCursorPos);

	void GetElapsedTime(float fElapsedTime) { m_ElapsTime = fElapsedTime; }

	void RotateMouseCamera(float X, float Y);

	void MoveForwardBack(float Sign); // 부호넣기
	void MoveRightLeft(float Sign); // 부호넣기
	void MoveUPDown(float Sign); // 부호넣기

private:
	// 플레이어를 가지고 있기
	CGameObject* m_Player{};


	float m_BeforeX{-500.f};
	float m_BeforeY{ -500.f };
	float m_MoveSpeed{10.f};

	float m_NowOffset{}; // 현재 플레이어와의 거리
	float m_Offset{}; // 목표 거리


	size_t m_NowMode{}; // 현재 플레이어 카메라 모드
	// 0번 : 자유, 1번 : 1플레이어, 2번 : 2플레이어
};

