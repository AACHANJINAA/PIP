#include "stdafx.h"
#include "main.h"
#include "NetworkManager.h"
#include "GameFramework.h"
#include "resource1.h"
#include "InputManager.h"

namespace
{
    std::string SERVER_ADDR = "127.0.0.1";
    std::string PLAYER_NAME = "MyPlayer"; // 플레이어 이름 저장용
}


#define MAX_LOADSTRING 100

// 전역 변수:
HINSTANCE hInst;                                // 현재 인스턴스입니다.
WCHAR szTitle[MAX_LOADSTRING];                  // 제목 표시줄 텍스트입니다.
WCHAR szWindowClass[MAX_LOADSTRING];            // 기본 창 클래스 이름입니다.
HWND g_hwnd; // 전역 윈도우 핸들 디버깅용

// 이 코드 모듈에 포함된 함수의 선언을 전달합니다:
ATOM                MyRegisterClass(HINSTANCE hInstance);
BOOL                InitInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    About(HWND, UINT, WPARAM, LPARAM);
INT_PTR DialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);




int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPWSTR    lpCmdLine,
    _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);

	if (DialogBoxParam(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, DialogProc, 0) != IDOK)
    {
        return 0; // 사용자가 취소를 누르면 프로그램 종료
    }
	// 네트워크 Startup
    if (!NetworkManager::Instance()->init_network())
    {
        error_display("WSAStartup failed", WSAGetLastError());
        return FALSE;
    }

    // 전역 문자열을 초기화합니다.
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_CHESSCLIENT, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // 애플리케이션 초기화를 수행합니다:
    if (!InitInstance(hInstance, nCmdShow))
    {
        NetworkManager::Instance()->cleanup_network();
        return FALSE;
    }
	// 주소구조체 설정 및 서버 연결
    if (!NetworkManager::Instance()->connect_to_server(SERVER_ADDR, common::packet::SERVER_PORT))
    {
        NetworkManager::Instance()->cleanup_network();
        return FALSE;
    }
    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_CHESSCLIENT));


    // 최초 로그인 패킷 전송 (플레이어 이름 사용)
    NetworkManager::Instance()->SendLoginPacket(PLAYER_NAME);

    int room_to_enter = 1;
    CLOG("[Auto-Enter] Automatically requesting to enter room " << room_to_enter);
    NetworkManager::Instance()->SendEnterRoomPacket(room_to_enter);

    // 기본 메시지 루프입니다:
    MSG msg;
	ZeroMemory(&msg, sizeof(MSG));

	while (msg.message != WM_QUIT)
	{// PeekMessage는 메시지 큐를 확인하되, 없으면 바로 리턴합니다. (블로킹되지 않음)
		if (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
        }
        else
        {
            // 메시지 큐가 비어있을 때, 우리의 게임 로직을 실행합니다.
        	GameFramework::Instance()->FrameAdvance();
        }
	}
    GameFramework::Instance()->OnDestroy();

	// 네트워크 Cleanup
    NetworkManager::Instance()->disconnect();
    NetworkManager::Instance()->cleanup_network();
    return (int)msg.wParam;
}


//
//  함수: MyRegisterClass()
//
//  용도: 창 클래스를 등록합니다.
//
ATOM MyRegisterClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_CHESSCLIENT));
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    // 주 윈도우의 메뉴가 나타나지 않도록 함
    wcex.lpszMenuName = NULL;
    //wcex.lpszMenuName   = MAKEINTRESOURCEW(IDC_DWLABPROJECT05);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    return RegisterClassExW(&wcex);
}

//
//   함수: InitInstance(HINSTANCE, int)
//
//   용도: 인스턴스 핸들을 저장하고 주 창을 만듭니다.
//
//   주석:
//
//        이 함수를 통해 인스턴스 핸들을 전역 변수에 저장하고
//        주 프로그램 창을 만든 다음 표시합니다.
//
BOOL InitInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // 인스턴스 핸들을 전역 변수에 저장합니다.

    RECT rc = { 0, 0, FRAME_BUFFER_WIDTH, FRAME_BUFFER_HEIGHT };

    DWORD dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_MINIMIZEBOX | WS_BORDER | WS_SYSMENU;
    AdjustWindowRect(&rc, dwStyle, FALSE);

    HWND hMainWnd = CreateWindow(szWindowClass, szTitle, dwStyle, CW_USEDEFAULT,
        CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hInstance, NULL);
    if (!hMainWnd) return(FALSE);

    // HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
       // CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (!hMainWnd)
    {
        return FALSE;
    }

    GameFramework::Instance()->OnCreate(hInstance, hMainWnd);

    ShowWindow(hMainWnd, nCmdShow);
    UpdateWindow(hMainWnd);

#ifdef _WITH_SWAPCHAIN_FULLSCREEN_STATE
    gGameFramework.ChangeSwapChainState();
#endif

    return TRUE;
}
INT_PTR DialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_INITDIALOG:
            SetDlgItemTextA(hWnd, IDC_EDIT1, SERVER_ADDR.c_str());
            SetDlgItemTextA(hWnd, IDC_EDIT3, PLAYER_NAME.c_str());
            return (INT_PTR)TRUE;
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK)
            {
                char buffer1[256];
                char buffer2[256];
                GetDlgItemTextA(hWnd, IDC_EDIT1, buffer1, 256);
                GetDlgItemTextA(hWnd, IDC_EDIT3, buffer2, 256);
                SERVER_ADDR.assign(buffer1);
                PLAYER_NAME.assign(buffer2);
                EndDialog(hWnd, IDOK);
                return (INT_PTR)TRUE;
            }
            else if (LOWORD(wParam) == IDCANCEL)
            {
                EndDialog(hWnd, IDCANCEL);
                return (INT_PTR)TRUE;
            }
            break;
    }
    return (INT_PTR)FALSE;
}
//
//  함수: WndProc(HWND, UINT, WPARAM, LPARAM)
//
//  용도: 주 창의 메시지를 처리합니다.
//
//  WM_COMMAND  - 애플리케이션 메뉴를 처리합니다.
//  WM_PAINT    - 주 창을 그립니다.
//  WM_DESTROY  - 종료 메시지를 게시하고 반환합니다.
//
//
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_ACTIVATE:
    {
        // wParam의 하위 워드를 확인하여 활성화 상태를 판단합니다.
        switch (LOWORD(wParam))
        {
        case WA_ACTIVE:      // 창이 활성화됨 (다른 창을 클릭했다가 다시 우리 창을 클릭)
        case WA_CLICKACTIVE: // 마우스 클릭으로 창이 활성화됨
            GameFramework::Instance()->m_bIsWindowActive = true;
            break;
        case WA_INACTIVE:    // 창이 비활성화됨 (다른 창을 클릭)
            GameFramework::Instance()->m_bIsWindowActive = false;
            // 여기에 게임 일시정지, 사운드 음소거 등의 로직을 넣을 수도 있습니다.
            break;
        }
        break;
    }
    case WM_DESTROY:
        ::PostQuitMessage(0);
        break;
    default:
        return(::DefWindowProc(hWnd, message, wParam, lParam));
    }
    return 0;
}

// 정보 대화 상자의 메시지 처리기입니다.
INT_PTR CALLBACK About(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(hDlg, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
