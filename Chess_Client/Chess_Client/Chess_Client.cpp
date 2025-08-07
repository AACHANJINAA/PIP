// DWLABPROJECT03.cpp : 애플리케이션에 대한 진입점을 정의합니다.
//

#include "stdafx.h"
#include "Chess_Client.h"
#include "ClientPacketManager.h"
#include "GameFramework.h"
#include "resource1.h"


CGameFramework gGameFramework;
SOCKET c_socket;
std::wstring SERVER_ADDR_W = L"127.0.0.1";
std::wstring PLAYER_NAME_W = L"MyPlayer"; // 플레이어 이름 저장용

// std::vector<char> g_recvBuffer; // 서버로부터 받은 데이터를 쌓아두는 수신 버퍼 (ClientPacketManager 내부로 이동)

//TODO: 창에서 포커스가 벗어났을때 키입력 받지않도록 설정 요망


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

void error_display(const char* msg, int err_no)
{
    WCHAR* lpMsgBuf;
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
        NULL, err_no,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPTSTR)&lpMsgBuf, 0, NULL);
    MessageBox(g_hwnd, lpMsgBuf, (LPCWSTR)msg, MB_OK);
    LocalFree(lpMsgBuf);
}

void recv_and_process_packets()
{
    char recvBuffer[4096];
    int retval = recv(c_socket, recvBuffer, sizeof(recvBuffer), 0);
    if (retval == SOCKET_ERROR)
    {
        if (WSAGetLastError() == WSAEWOULDBLOCK) return;
        error_display("recv", WSAGetLastError());
        closesocket(c_socket);
        PostQuitMessage(0);
        return;
    }
    if (retval == 0) return; // 정상 종료

    // ClientPacketManager를 통해 데이터 처리
    ClientPacketManager::Instance()->ProcessReceivedData(recvBuffer, retval);
}

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

	WSAData wsadata;
    WSAStartup(MAKEWORD(2, 2), &wsadata);
    c_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    // 전역 문자열을 초기화합니다.
    LoadStringW(hInstance, IDS_APP_TITLE, szTitle, MAX_LOADSTRING);
    LoadStringW(hInstance, IDC_CHESSCLIENT, szWindowClass, MAX_LOADSTRING);
    MyRegisterClass(hInstance);

    // 애플리케이션 초기화를 수행합니다:
    if (!InitInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }

    HACCEL hAccelTable = LoadAccelerators(hInstance, MAKEINTRESOURCE(IDC_CHESSCLIENT));

    SOCKADDR_IN addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(chess::packet::SERVER_PORT);
    std::string server_addr_mb;
    server_addr_mb.assign(SERVER_ADDR_W.begin(), SERVER_ADDR_W.end());
    inet_pton(AF_INET, server_addr_mb.c_str(), &addr.sin_addr);

    if (connect(c_socket, (SOCKADDR*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        error_display("connect", WSAGetLastError());
        return 0;
    }

    u_long on = 1;
    if (ioctlsocket(c_socket, FIONBIO, &on) == SOCKET_ERROR)
    {
        error_display("ioctlsocket", WSAGetLastError());
        closesocket(c_socket);
        return 0;
    }

    // ClientPacketManager 초기화
    ClientPacketManager::Instance()->Initialize(c_socket);


    /*WSAEVENT hEvent = WSACreateEvent();
    if (hEvent == WSA_INVALID_EVENT)
    {
        error_display("WSACreateEvent", WSAGetLastError());
        return 0;
    }
    if (WSAEventSelect(c_socket, hEvent, FD_READ | FD_CLOSE) == SOCKET_ERROR)
    {
        error_display("WSAEventSelect", WSAGetLastError());
        WSACloseEvent(hEvent);
        return 0;
    }*/
    // 최초 로그인 패킷 전송 (플레이어 이름 사용)
    std::string player_name_mb(PLAYER_NAME_W.begin(), PLAYER_NAME_W.end());
    ClientPacketManager::Instance()->SendLoginPacket(player_name_mb); // 호출 변경
    

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
        	gGameFramework.FrameAdvance();
        }
	}
    gGameFramework.OnDestroy();


    closesocket(c_socket);
    WSACleanup();
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

    gGameFramework.OnCreate(hInstance, hMainWnd);


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
            SetDlgItemText(hWnd, IDC_EDIT1, SERVER_ADDR_W.c_str());
            SetDlgItemText(hWnd, IDC_EDIT3, PLAYER_NAME_W.c_str());
            return (INT_PTR)TRUE;
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK)
            {
                wchar_t buffer1[256];
                wchar_t buffer2[256];
                GetDlgItemText(hWnd, IDC_EDIT1, buffer1, 256);
                GetDlgItemText(hWnd, IDC_EDIT3, buffer2, 256);
                SERVER_ADDR_W = buffer1;
                PLAYER_NAME_W = buffer2;
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
    case WM_SIZE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MOUSEMOVE:
    case WM_KEYDOWN:
    case WM_KEYUP:
        gGameFramework.OnProcessingWindowMessage(hWnd, message, wParam, lParam);
        break;
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
