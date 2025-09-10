#include "stdafx.h"
#include "InputManager.h"

InputManager::InputManager()
{
    ZeroMemory(m_eKeyState, sizeof(m_eKeyState));
    m_ptMousePos = { 0, 0 };
    m_ptOldMousePos = { 0, 0 };
}

void InputManager::Init(HWND hWnd)
{
    m_hWnd = hWnd;
    for (int i = 0; i < KEY_MAX; ++i)
    {
        m_eKeyState[i] = KEY_STATE::NONE;
    }
}

void InputManager::Update()
{
    m_ptOldMousePos = m_ptMousePos;

    GetCursorPos(&m_ptMousePos);
    ScreenToClient(m_hWnd, &m_ptMousePos);

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
}

POINT InputManager::GetMouseDelta()
{
    POINT delta = { m_ptMousePos.x - m_ptOldMousePos.x, m_ptMousePos.y - m_ptOldMousePos.y };
    return delta;
}