#include "stdafx.h"
#include "FreeCamera.h"
#include "ObjectManager.h"
#include "InputManager.h"
#include "GameFramework.h"

void FreeCamera::UpdateAnimateCamera(float fElapsedTime)
{
	GetElapsedTime(fElapsedTime);

	switch (m_NowMode)
	{
	case CAMERA_MODE::CAMERA_FREE:

		break;
	case CAMERA_MODE::CAMERA_THIRD_PERSON:
	{
		std::shared_ptr<GameObject>Player = ObjectManager::Instance()->GetPlayer();
		if(Player.get())
		{
			if (m_NowOffset < m_Offset)
			{
				m_NowOffset += m_Move_Offset_Time * fElapsedTime;
			}
			else
			{
				int i = 0; // DW디버깅
			}
			SetPosition(XMFLOAT3(Player.get()->position().x - (GetLookVec().x) * m_NowOffset, Player.get()->position().y - (GetLookVec().y) * m_NowOffset, Player.get()->position().z - (GetLookVec().z) * m_NowOffset)); // 카메라를 플레이어한테 붙이기
		}
	}
		break;
	case CAMERA_MODE::CAMERA_END:

		break;
	default:
		break;
	}
}

void FreeCamera::ProcessInput(float fElapsedTime)
{

	// 창이 포커싱 되어있고, 커서가 안보일 때만 회전 적용
	if (GameFramework::Instance()->m_bIsWindowActive && !InputManager::Instance()->GetIsShowCusor())
	{
		float cxDelta = InputManager::Instance()->GetMouseDelta().x;
		float cyDelta = InputManager::Instance()->GetMouseDelta().y;

		if (cxDelta != 0.0f || cyDelta != 0.0f)
		{
			Rotate(cyDelta, cxDelta, 0.f);
		}
	}

   /* if (InputManager::Instance()->IsKeyDown(VK_RBUTTON))
    {
        ::SetCapture(InputManager::Instance()->GetHWnd());
		::GetCursorPos(&m_ptOldCursorPos);
    }

    if (InputManager::Instance()->IsKeyDown(VK_RBUTTON) || InputManager::Instance()->IsKeyPress(VK_RBUTTON))
    {
        POINT ptCursorPos;
        ::GetCursorPos(&ptCursorPos); 

        float cxDelta = (float)(ptCursorPos.x - m_ptOldCursorPos.x) / 3.0f;
        float cyDelta = (float)(ptCursorPos.y - m_ptOldCursorPos.y) / 3.0f;

        if (cxDelta != 0.0f || cyDelta != 0.0f)
        {
            Rotate(cyDelta, cxDelta, 0.f);
            ::SetCursorPos(m_ptOldCursorPos.x, m_ptOldCursorPos.y);
        }
    }

    if (InputManager::Instance()->IsKeyUp(VK_RBUTTON))
    {
        ::ReleaseCapture();
    }*/

    if (InputManager::Instance()->IsKeyDown('V')) {
        m_NowMode = CAMERA_MODE::CAMERA_FREE;
    }
    if (InputManager::Instance()->IsKeyDown('B')) {
        m_NowMode = CAMERA_MODE::CAMERA_THIRD_PERSON;
    }

    if (m_NowMode == CAMERA_MODE::CAMERA_FREE)
    {
        if (InputManager::Instance()->IsKeyPress('W')) MoveForward(1);
        if (InputManager::Instance()->IsKeyPress('S')) MoveForward(-1);
        if (InputManager::Instance()->IsKeyPress('D')) MoveRight(1);
        if (InputManager::Instance()->IsKeyPress('A')) MoveRight(-1);
        if (InputManager::Instance()->IsKeyPress(VK_SPACE)) MoveUP(1);
        if (InputManager::Instance()->IsKeyPress(VK_LCONTROL)) MoveUP(-1);
    }
}


void FreeCamera::RotateMouseCamera(float NowX, float NowY)
{
	if (m_BeforeX < 0.f || m_BeforeY < 0.f)
	{
		m_BeforeX = NowX;
		m_BeforeY = NowY;
	}

	Rotate((NowY - m_BeforeY) / 5.f, (NowX - m_BeforeX) / 5.f, 0.f);

	m_BeforeX = NowX;
	m_BeforeY = NowY;
	
}

void FreeCamera::MoveForward(float Sign)
{
	Move(m_xmf3Look.x * Sign * m_ElapseTime * m_MoveSpeed, m_xmf3Look.y * Sign * m_ElapseTime * m_MoveSpeed, m_xmf3Look.z * Sign * m_ElapseTime * m_MoveSpeed);
}

void FreeCamera::MoveRight(float Sign)
{
	Move(m_xmf3Right.x * Sign * m_ElapseTime * m_MoveSpeed, m_xmf3Right.y * Sign * m_ElapseTime * m_MoveSpeed, m_xmf3Right.z * Sign * m_ElapseTime * m_MoveSpeed);
}

void FreeCamera::MoveUP(float Sign)
{
	Move(0.f, Sign * m_ElapseTime * m_MoveSpeed,0.f);
}
