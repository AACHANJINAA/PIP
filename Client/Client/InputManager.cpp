#include "stdafx.h"
#include "InputManager.h"

#include "GameFramework.h"

InputManager::InputManager()
{
    ZeroMemory(m_eKeyState, sizeof(m_eKeyState));
    m_ptMousePos = { 0, 0 };
    m_ptOldMousePos = { 0, 0 };
}

void InputManager::initialize(HWND hWnd)
{
    m_hWnd = hWnd;
    for (auto& i : m_eKeyState)
	{
		i = KEY_STATE::NONE;
    }
}

void InputManager::Update()
{
    // 이 확인 로직을 추가합니다.
    if (!GameFramework::instance()->m_bIsWindowActive)
    {
        // 선택적으로, 포커스를 다시 얻을 때 키가 "고정"되는 것을 방지하기 위해 키 상태 재설정 합니다.
        for (auto& i : m_eKeyState)
		{
			i = KEY_STATE::NONE;
        }
        return; // 창이 활성화되지 않은 경우 입력 처리를 건너뜁니다.
    }
    if (!_isShowCusor) // 안보일 때
    {

    }
    else
    {
        m_ptOldMousePos = m_ptMousePos;

        GetCursorPos(&m_ptMousePos);
        ScreenToClient(m_hWnd, &m_ptMousePos);
    }
    

    for (int i = 0; i < KEY_MAX; ++i)
    {
        if (GetAsyncKeyState(i) & 0x8000)
        {
            if (m_eKeyState[i] == KEY_STATE::DOWN || m_eKeyState[i] == KEY_STATE::PRESS)
                m_eKeyState[i] = KEY_STATE::PRESS;
            else 
                m_eKeyState[i] = KEY_STATE::DOWN;
        }
        else 
        {
            if (m_eKeyState[i] == KEY_STATE::DOWN || m_eKeyState[i] == KEY_STATE::PRESS)
                m_eKeyState[i] = KEY_STATE::UP;
            else 
                m_eKeyState[i] = KEY_STATE::NONE;
        }
    }

    MouseFixCenter();
}

POINT InputManager::GetMouseDelta()
{
    POINT delta = { m_ptMousePos.x - m_ptOldMousePos.x, m_ptMousePos.y - m_ptOldMousePos.y };
    return delta;
}

void InputManager::ChangeShowCusor()
{
    // 커서 표시 토글
    _isShowCusor = !_isShowCusor;
    ShowCursor(_isShowCusor);

    RECT rect;
    ::GetClientRect(m_hWnd, &rect);

    POINT centerPt = { (rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2 };
    ::ClientToScreen(m_hWnd, &centerPt);

    int centerX = centerPt.x;
    int centerY = centerPt.y;

    // 마우스 중앙 이동
    SetCursorPos(centerX, centerY);

    // 상태 변경 시 m_ptOldMousePos와 m_ptMousePos를 현재 중앙으로 강제 갱신
    m_ptOldMousePos.x = centerX;
    m_ptOldMousePos.y = centerY;
    m_ptMousePos.x = centerX;
    m_ptMousePos.y = centerY;
}

void InputManager::MouseFixCenter()
{
    if (!_isShowCusor) // 커서가 숨겨진 상태일 때만
    {
        // 1. 현재 마우스 위치 얻기
        POINT currentPos;
        GetCursorPos(&currentPos);

        // 2. 실제 마우스 이동량 계산 (마지막 스냅 위치 대비)
        int deltaX = currentPos.x - m_ptOldMousePos.x;
        int deltaY = currentPos.y - m_ptOldMousePos.y;

        // 3. 새로운 클라이언트 영역 중앙 계산
        RECT rect;
        ::GetClientRect(m_hWnd, &rect);
        POINT centerPt = { (rect.left + rect.right) / 2, (rect.top + rect.bottom) / 2 };
        ::ClientToScreen(m_hWnd, &centerPt);

        int centerX = centerPt.x;
        int centerY = centerPt.y;

        // 4. 새로운 중앙으로 마우스 스냅
        SetCursorPos(centerX, centerY);

        // 5. 다음 프레임 비교를 위해 m_ptOldMousePos를 방금 스냅한 위치로 업데이트
        m_ptOldMousePos.x = centerX;
        m_ptOldMousePos.y = centerY;

        // 6. GetMouseDelta()가 정상적인 delta를 반환하도록 m_ptMousePos 조작
        // GetMouseDelta() return 값이 (m_ptMousePos - m_ptOldMousePos) 이므로
        m_ptMousePos.x = centerX + deltaX;
        m_ptMousePos.y = centerY + deltaY;
    }
}
