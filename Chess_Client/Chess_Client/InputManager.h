#pragma once

#include "stdafx.h"

#define KEY_MAX 256

class InputManager
{
private:
    InputManager();
    ~InputManager() = default;

    enum class KEY_STATE {
        NONE,
        DOWN,
        UP,
        PRESS
    };

    KEY_STATE m_eKeyState[KEY_MAX];
    POINT m_ptMousePos;
    POINT m_ptOldMousePos;
    HWND m_hWnd = nullptr;

public:
    static InputManager* Instance()
    {
        static InputManager instance;
        return &instance;
    }

    void Init(HWND hWnd);
    void Update();

    bool IsKeyDown(int key) { return m_eKeyState[key] == KEY_STATE::DOWN; }
    bool IsKeyUp(int key) { return m_eKeyState[key] == KEY_STATE::UP; }
    bool IsKeyPress(int key) { return m_eKeyState[key] == KEY_STATE::PRESS; }

    POINT GetMousePos() { return m_ptMousePos; }
    POINT GetMouseDelta();
    HWND GetHWnd() { return m_hWnd; }
};