#include "stdafx.h"
#include "InputManager.h"

InputManager::InputManager()
{
    ZeroMemory(m_eKeyState, sizeof(m_eKeyState));
    m_ptMousePos = { 0, 0 };
    m_ptOldMousePos = { 0, 0 };
}

void InputManager::initialize(HWND hWnd)
{
    m_hWnd = hWnd;
    for (int i = 0; i < KEY_MAX; ++i)
    {
        m_eKeyState[i] = KEY_STATE::NONE;
    }
}

void InputManager::Update()
{

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
    // 상태 바꾸고 적용
    _isShowCusor = !_isShowCusor;
    ShowCursor(_isShowCusor);

    // 상태 바꿀 때 마우스 팍! 튀는 것 방지
    RECT rect;
    ::GetWindowRect(m_hWnd, &rect);

    // 현재 화면의 중앙값 구하기
    int centerX = (rect.left + rect.right) / 2;
    int centerY = (rect.top + rect.bottom) / 2;

    // 마우스 중앙으로 이동
    SetCursorPos(centerX, centerY);
}

void InputManager::MouseFixCenter()
{
    if (!_isShowCusor) // 마우스 안보일 때는 화면 중앙으로 고정시키기
    {
        RECT rect;
        ::GetWindowRect(m_hWnd, &rect);

        // 현재 화면의 중앙값 구하기
        int centerX = (rect.left + rect.right) / 2;
        int centerY = (rect.top + rect.bottom) / 2;

        // 현재 마우스 위치
        POINT currentPos;
        GetCursorPos(&currentPos);

        // 현재 마우스 위치
        m_ptMousePos.x = currentPos.x;
        m_ptMousePos.y = currentPos.y;

        // 화면 중앙 위치
        m_ptOldMousePos.x = centerX;
        m_ptOldMousePos.y = centerY;

        // 마우스 중앙으로 이동
        SetCursorPos(centerX, centerY);
    }
}
