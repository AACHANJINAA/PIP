#include "stdafx.h"
#include "FreeCamera.h"

void CFreeCamera::UpdateAnimateCamera(float fElapsedTime)
{
	GetElapsedTime(fElapsedTime);
	if (m_NowMode != 2)
	{
		m_NowOffset = 0;
	}


	if (m_NowMode == 0)
	{
		//KeyInput(fElapsedTime,hWnd,nMessageID);
	}
	if (m_NowMode == 1)
	{
		SetPosition(XMFLOAT3(m_Player->GetPosition().x, m_Player->GetPosition().y + 5.f, m_Player->GetPosition().z)); // 카메라를 플레이어한테 붙이기
	}
	if (m_NowMode == 2)
	{
		if(m_NowOffset < m_Offset)
		{
			m_NowOffset += 20.f * fElapsedTime;
		}
		else
		{
			int i = 0; // DW디버깅
		}
		SetPosition(XMFLOAT3(m_Player->GetPosition().x - (GetLookVec().x) * m_NowOffset, m_Player->GetPosition().y - (GetLookVec().y) * m_NowOffset, m_Player->GetPosition().z - (GetLookVec().z) * m_NowOffset)); // 카메라를 플레이어한테 붙이기
	}
}

void CFreeCamera::KeyInput(float fElapsedTime, HWND hwnd, UINT nMessageID, POINT ptOldCursorPos)
{

	float cxDelta = 0.0f, cyDelta = 0.0f;
	POINT ptCursorPos;
	POINT ScreenPos;

	/*마우스를 캡쳐했으면 마우스가 얼마만큼 이동하였는 가를 계산한다. 마우스 왼쪽 또는 오른쪽 버튼이 눌러질 때의
	메시지(WM_LBUTTONDOWN, WM_RBUTTONDOWN)를 처리할 때 마우스를 캡쳐하였다. 그러므로 마우스가 캡쳐된
	것은 마우스 버튼이 눌려진 상태를 의미한다. 마우스 버튼이 눌려진 상태에서 마우스를 좌우 또는 상하로 움직이면 플
	레이어를 x-축 또는 y-축으로 회전한다.*/
	if (::GetCapture() == hwnd)
	{
		//마우스 커서를 화면에서 없앤다(보이지 않게 한다).
		//::SetCursor(NULL);

		//현재 마우스 커서의 위치를 가져온다. 
		::GetCursorPos(&ptCursorPos);

		switch (nMessageID)
		{
		case WM_LBUTTONDOWN:
		{

		}
		break;

		case WM_RBUTTONDOWN:

			ScreenPos = ptCursorPos;
			ScreenToClient(hwnd, &ScreenPos);

			break;
		}

		//마우스 버튼이 눌린 상태에서 마우스가 움직인 양을 구한다.
		cxDelta = (float)(ptCursorPos.x - ptOldCursorPos.x) / 3.0f;
		cyDelta = (float)(ptCursorPos.y - ptOldCursorPos.y) / 3.0f;

		Rotate(cyDelta, cxDelta, 0.f);

		//마우스 커서의 위치를 마우스가 눌려졌던 위치로 설정한다. 
		::SetCursorPos(ptOldCursorPos.x, ptOldCursorPos.y);
	}

	if (GetAsyncKeyState('W') & 0x8000) {
		MoveForwardBack(1);
	}
	if (GetAsyncKeyState('S') & 0x8000) {
		MoveForwardBack(-1);
	}
	if (GetAsyncKeyState('D') & 0x8000) {
		MoveRightLeft(1);
	}
	if (GetAsyncKeyState('A') & 0x8000) {
		MoveRightLeft(-1);
	}

	if (GetAsyncKeyState(VK_SPACE) & 0x8000) {
		MoveUPDown(1);
	}

	if (GetAsyncKeyState(VK_LCONTROL) & 0x8000) {
		MoveUPDown(-1);
	}
}


void CFreeCamera::RotateMouseCamera(float NowX, float NowY)
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

void CFreeCamera::MoveForwardBack(float Sign)
{
	Move(m_xmf3Look.x * Sign * m_ElapsTime * m_MoveSpeed, m_xmf3Look.y * Sign * m_ElapsTime * m_MoveSpeed, m_xmf3Look.z * Sign * m_ElapsTime * m_MoveSpeed);
}

void CFreeCamera::MoveRightLeft(float Sign)
{
	Move(m_xmf3Right.x * Sign * m_ElapsTime * m_MoveSpeed, m_xmf3Right.y * Sign * m_ElapsTime * m_MoveSpeed, m_xmf3Right.z * Sign * m_ElapsTime * m_MoveSpeed);
}

void CFreeCamera::MoveUPDown(float Sign)
{
	Move(0.f, Sign * m_ElapsTime * m_MoveSpeed,0.f);
}
