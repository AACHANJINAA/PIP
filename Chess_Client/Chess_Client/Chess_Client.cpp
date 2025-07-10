// DWLABPROJECT03.cpp : 애플리케이션에 대한 진입점을 정의합니다.
//

#include "stdafx.h"
#include "Chess_Client.h"

#include "Other_King.h"
#include "Chess_King.h"
#include "GameFramework.h"
#include "ObjectManager.h"
#include "resource1.h"


CGameFramework gGameFramework;
SOCKET c_socket;
std::wstring SERVER_ADDR_W = L"127.0.0.1";
std::wstring PLAYER_NAME_W = L"MyPlayer"; // 플레이어 이름 저장용

std::vector<char> g_recvBuffer; // 서버로부터 받은 데이터를 쌓아두는 수신 버퍼


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
void send_login_packet(const std::string& name)
{
    uint16_t name_len = static_cast<uint16_t>(name.length());
    
    uint16_t packet_type = static_cast<uint16_t>(chess::packet::PacketType::C2S_P_LOGIN);
    uint16_t total_size = sizeof(chess::packet::PacketHeader) + sizeof(name_len) + name_len;

    std::vector<char> buffer(total_size);
    char* p = buffer.data();

    memcpy(p, &total_size, sizeof(total_size)); p += sizeof(total_size);
    memcpy(p, &packet_type, sizeof(packet_type)); p += sizeof(packet_type);
    
    memcpy(p, &name_len, sizeof(name_len)); p += sizeof(name_len);
    memcpy(p, name.c_str(), name_len);

    send(c_socket, buffer.data(), total_size, 0);
}
void send_move_packet(chess::packet::MOVE_TYPE direction)
{
    uint16_t packet_type = static_cast<uint16_t>(chess::packet::PacketType::C2S_P_MOVE);
    uint16_t total_size = sizeof(chess::packet::PacketHeader) + sizeof(direction);

    std::vector<char> buffer(total_size);
    char* p = buffer.data();

    memcpy(p, &total_size, sizeof(total_size)); p += sizeof(total_size);
    memcpy(p, &packet_type, sizeof(packet_type)); p += sizeof(packet_type);
    memcpy(p, &direction, sizeof(direction));

    send(c_socket, buffer.data(), total_size, 0);
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

    // 새로 받은 데이터를 전역 수신 버퍼에 추가
    g_recvBuffer.insert(g_recvBuffer.end(), recvBuffer, recvBuffer + retval);

    // 처리 루프
    while (true)
    {
        // 1. 헤더를 읽을 만큼 데이터가 충분한가?
        if (g_recvBuffer.size() < sizeof(chess::packet::PacketHeader))
            break;

        chess::packet::PacketHeader* header = reinterpret_cast<chess::packet::PacketHeader*>(g_recvBuffer.data());

        // 2. 패킷 전체를 받을 만큼 데이터가 충분한가?
        if (g_recvBuffer.size() < header->_size)
            break;

        // 3. 패킷 종류에 따라 처리
        // 헤더 다음 위치부터 파싱 시작. char* 포인터로 순차적으로 읽는다.
        char* payload_ptr = g_recvBuffer.data() + sizeof(chess::packet::PacketHeader);
        chess::packet::PacketType type = static_cast<chess::packet::PacketType>(header->_type);

        switch (type)
        {
            case chess::packet::PacketType::S2C_P_AVATAR_INFO:
	        {
	            chess::packet::SC_PACKET_AVATAR_INFO* pkt = reinterpret_cast<chess::packet::SC_PACKET_AVATAR_INFO*>(payload_ptr);
	            auto player = std::dynamic_pointer_cast<CChess_King>(CObjectManager::GetManager()->GetPlayer());
	            if (player == nullptr)
	            {
	                player = std::make_shared<CChess_King>();
	            }
	            player->SetID(pkt->_id);
	            player->SetPos(pkt->_x, pkt->_y);
	            player->m_Mesh_Type = I_WANT_CHESS_PLAYER;

	            CObjectManager::GetManager()->RequestObject(player);
                CObjectManager::GetManager()->SetPlayer(player);
	            /*g_myPlayer.hp = pkt->_hp;
	            g_myPlayer.exp = pkt->_exp;
	            g_myPlayer.level = pkt->_level;*/ //아직은 안씀
	            break;
	        }
            case chess::packet::PacketType::S2C_P_ENTER:
            {
                // [수정] 서버가 보낸 순서대로 데이터를 하나씩 읽습니다.
                char* p = payload_ptr;

                int64_t new_id;
                memcpy(&new_id, p, sizeof(new_id)); p += sizeof(new_id);

                char obj_type;
                memcpy(&obj_type, p, sizeof(obj_type)); p += sizeof(obj_type);

                short x, y;
                memcpy(&x, p, sizeof(x)); p += sizeof(x);
                memcpy(&y, p, sizeof(y)); p += sizeof(y);

                uint16_t name_len;
                memcpy(&name_len, p, sizeof(name_len)); p += sizeof(name_len);

                std::string name(p, name_len);

                {
                    //상대방 생성
                    auto Other = std::make_shared<COther_King>(x, y);
                    Other->SetID(new_id);
					Other->SetName(name);
					Other->m_Mesh_Type = I_WANT_CHESS_ENEMY; // 아오 대원시치

                    CObjectManager::GetManager()->RequestObject(Other);
                }

                break;
            }
            case chess::packet::PacketType::S2C_P_MOVE:
            {
                chess::packet::SC_PACKET_MOVE* pkt = reinterpret_cast<chess::packet::SC_PACKET_MOVE*>(payload_ptr);
				auto player = std::dynamic_pointer_cast<CChess_King>(CObjectManager::GetManager()->GetPlayer());
                if (pkt->_id == player->GetID())
                {
                    player->SetPos(pkt->_x, pkt->_y);
                }
                else
                {
					auto other_players = CObjectManager::GetManager()->GetEnemy();
                    auto it = std::find_if(other_players.begin(), other_players.end(), [pkt](const std::shared_ptr<CGameObject>& other)
                    {
                    	return pkt->_id == static_cast<COther_King*>(other.get())->GetID();
                    });
                    if (it != other_players.end())
                    {
						dynamic_cast<COther_King*>(it->get())->SetPos(pkt->_x, pkt->_y);
                    }
                }
                break;
            }
            case chess::packet::PacketType::S2C_P_LEAVE:
            {
                chess::packet::SC_PACKET_LEAVE* pkt = reinterpret_cast<chess::packet::SC_PACKET_LEAVE*>(payload_ptr);
                auto other_players = CObjectManager::GetManager()->GetEnemy();
                auto it = std::find_if(other_players.begin(), other_players.end(), [pkt](const std::shared_ptr<CGameObject>& other) 
                {
                    return pkt->_id == static_cast<COther_King*>(other.get())->GetID();
                });
                if (it != other_players.end())
                {
                    (*it)->m_Delete = true;
                }
                break;
            }
        }

        // 4. 처리한 패킷만큼 버퍼에서 제거
        g_recvBuffer.erase(g_recvBuffer.begin(), g_recvBuffer.begin() + header->_size);
    }

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

    MSG msg;

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

    WSAEVENT hEvent = WSACreateEvent();
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
    }
    // 최초 로그인 패킷 전송 (플레이어 이름 사용)
    std::string player_name_mb(PLAYER_NAME_W.begin(), PLAYER_NAME_W.end());
    send_login_packet(player_name_mb);
    

    // 기본 메시지 루프입니다:
    while (true)
    {
        DWORD result = MsgWaitForMultipleObjects(1, &hEvent, FALSE, INFINITE, QS_ALLINPUT);
        if (result == WAIT_OBJECT_0)
        {
            // 소켓 이벤트 발생
            WSANETWORKEVENTS ne;
            if (WSAEnumNetworkEvents(c_socket, hEvent, &ne) == SOCKET_ERROR)
            {
                error_display("WSAEnumNetworkEvents", WSAGetLastError());
                break;
            }
            if (ne.lNetworkEvents & FD_READ)
            {
                recv_and_process_packets();
            }
            if (ne.lNetworkEvents & FD_CLOSE)
            {
                closesocket(c_socket);
                PostQuitMessage(0);
                break;
            }
        }
        else if (result == WAIT_OBJECT_0 + 1)
        {
            if (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) // 메세지 큐에 메세지가 있으면 TRUE를 반환하고 기존방식으로 한다, 없으면 FALSE를 반환하고 내 gameFramework의 FrameAdvance함수를 실행한다.
            {
                if (msg.message == WM_QUIT)
                    break;
                if (!::TranslateAccelerator(msg.hwnd, hAccelTable, &msg))
                {
                    ::TranslateMessage(&msg);
                    ::DispatchMessage(&msg);
                }
            }
            else
            {
                gGameFramework.FrameAdvance();
            }
        }
        
    }
    gGameFramework.OnDestroy();


    WSACloseEvent(hEvent);

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
