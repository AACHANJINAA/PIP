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

    BOOL _isShowCusor = TRUE;

public:
    static InputManager* Instance()
    {
        static InputManager instance;
        return &instance;
    }

    void initialize(HWND hWnd);
    void Update();

    bool IsKeyDown(int key) { return m_eKeyState[key] == KEY_STATE::DOWN; }
    bool IsKeyUp(int key) { return m_eKeyState[key] == KEY_STATE::UP; }
    bool IsKeyPress(int key) { return m_eKeyState[key] == KEY_STATE::PRESS; }


    // 마우스 관련 처리 함수
    POINT GetMousePos() { return m_ptMousePos; }
    POINT GetMouseDelta(); // 마우스 움직인 값 받기
    HWND GetHWnd() { return m_hWnd; }

    // 커서 보이는지? 상태 확인
    BOOL GetIsShowCusor() { return _isShowCusor; }
    // 커서 보이고 끄기
    void ChangeShowCusor();


private: // 나중에 바깥에서 필요하면 public로 옮겨도 상관없음
    // 현재는 내부적으로 마우스를 안보일 때만 중앙에 고정시킬 예정

    void MouseFixCenter(); // 마우스 고정 해야할 때 ex)FPS 모드

    float _mouseSensitivity = 0.3f; // 마우스 감도

};