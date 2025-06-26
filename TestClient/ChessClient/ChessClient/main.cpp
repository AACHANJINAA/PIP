// =================================================================
// Client.cpp
// =================================================================
#include <iostream>
#include <string>
#include <vector>
#include <winsock2.h>
#include <WS2tcpip.h>
#include <unordered_map>
#include "resource.h" // Dialog 리소스

#pragma comment(lib, "ws2_32.lib")

// -----------------------------------------------------------------
// 1. 새로운 패킷 정의 (서버의 Packet.h와 거의 동일)
// -----------------------------------------------------------------
#include "../../../Server/ChessServer/ChessServer/Packet.h"

// -----------------------------------------------------------------
// 전역 변수 및 함수 프로토타입
// -----------------------------------------------------------------

// 창 관련
constexpr std::wstring_view Class_Name = L"ChessClient";
constexpr int CELL_SIZE = 50;
HWND g_hwnd;

// 서버 접속 관련
std::wstring SERVER_ADDR_W = L"127.0.0.1";
std::wstring PLAYER_NAME_W = L"MyPlayer"; // 플레이어 이름 저장용
constexpr int SERVER_PORT = 3000;
SOCKET c_socket;

// 게임 데이터
struct Player
{
    int64_t id = -1;
    short x = 0;
    short y = 0;
    std::string name;
};

Player g_myPlayer;
std::unordered_map<int64_t, Player> g_otherPlayers;
std::vector<char> g_recvBuffer; // 서버로부터 받은 데이터를 쌓아두는 수신 버퍼

// 함수 프로토타입
void draw_board(HDC);
void error_display(const char*, int);
void send_login_packet(const std::string& name);
void send_move_packet(chess::packet::MOVE_TYPE direction);
void recv_and_process_packets();
LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK DialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

// -----------------------------------------------------------------
// WinMain - 프로그램 시작점
// -----------------------------------------------------------------
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    // ... (DialogBox, RegisterClass, CreateWindowEx 등은 기존과 동일) ...
    if (DialogBox(hInstance, MAKEINTRESOURCE(IDD_DIALOG1), NULL, DialogProc) == IDCANCEL)
    {
        return 0;
    }
    WNDCLASS wc{};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = Class_Name.data();
    RegisterClass(&wc);
    HWND hWnd = CreateWindowEx(0, Class_Name.data(), L"ChessClient", WS_OVERLAPPEDWINDOW,
                               CW_USEDEFAULT, CW_USEDEFAULT, 420, 440, nullptr, nullptr, hInstance, nullptr);
    if (hWnd == NULL) return 0;
    g_hwnd = hWnd;
    ShowWindow(hWnd, nCmdShow);
    // ...

    // 서버 접속
    WSAData wsadata;
    WSAStartup(MAKEWORD(2, 2), &wsadata);
    c_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    SOCKADDR_IN addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(SERVER_PORT);
    std::string server_addr_mb;
    server_addr_mb.assign(SERVER_ADDR_W.begin(), SERVER_ADDR_W.end());
    inet_pton(AF_INET, server_addr_mb.c_str(), &addr.sin_addr);

    if (connect(c_socket, (SOCKADDR*)&addr, sizeof(addr)) == SOCKET_ERROR)
    {
        error_display("connect", WSAGetLastError());
        return 0;
    }

    // 이벤트 객체 생성 및 소켓에 이벤트 연결
    WSAEVENT hEvent = WSACreateEvent();
    if (hEvent == WSA_INVALID_EVENT) {
        error_display("WSACreateEvent", WSAGetLastError());
        return 0;
    }
    if (WSAEventSelect(c_socket, hEvent, FD_READ | FD_CLOSE) == SOCKET_ERROR) {
        error_display("WSAEventSelect", WSAGetLastError());
        WSACloseEvent(hEvent);
        return 0;
    }

    // 최초 로그인 패킷 전송 (플레이어 이름 사용)
    std::string player_name_mb(PLAYER_NAME_W.begin(), PLAYER_NAME_W.end());
    send_login_packet(player_name_mb);

    MSG msg{};
    while (true) {
        DWORD result = MsgWaitForMultipleObjects(1, &hEvent, FALSE, INFINITE, QS_ALLINPUT);
        if (result == WAIT_OBJECT_0) {
            // 소켓 이벤트 발생
            WSANETWORKEVENTS ne;
            if (WSAEnumNetworkEvents(c_socket, hEvent, &ne) == SOCKET_ERROR) {
                error_display("WSAEnumNetworkEvents", WSAGetLastError());
                break;
            }
            if (ne.lNetworkEvents & FD_READ) {
                recv_and_process_packets();
            }
            if (ne.lNetworkEvents & FD_CLOSE) {
                closesocket(c_socket);
                PostQuitMessage(0);
                break;
            }
        } else if (result == WAIT_OBJECT_0 + 1) {
            // 윈도우 메시지 처리
            if (GetMessage(&msg, NULL, 0, 0)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            } else {
                break;
            }
        }
    }
    WSACloseEvent(hEvent);

    closesocket(c_socket);
    WSACleanup();
    return 0;
}

// -----------------------------------------------------------------
// 2. 패킷 전송 로직 (새로운 방식)
// -----------------------------------------------------------------

// 로그인 패킷을 조립하고 전송하는 함수
void send_login_packet(const std::string& name)
{
    // Payload: [이름 길이(2바이트)] + [이름 데이터(N바이트)]
    uint16_t name_len = static_cast<uint16_t>(name.length());

    // Header
    uint16_t packet_type = static_cast<uint16_t>(chess::packet::PacketType::C2S_P_LOGIN);
    uint16_t total_size = sizeof(chess::packet::PacketHeader) + sizeof(name_len) + name_len;

    // 최종 패킷 조립
    std::vector<char> buffer(total_size);
    char* p = buffer.data();

    // 헤더 쓰기
    memcpy(p, &total_size, sizeof(total_size)); p += sizeof(total_size);
    memcpy(p, &packet_type, sizeof(packet_type)); p += sizeof(packet_type);

    // 페이로드 쓰기
    memcpy(p, &name_len, sizeof(name_len)); p += sizeof(name_len);
    memcpy(p, name.c_str(), name_len);

    send(c_socket, buffer.data(), total_size, 0);
}

// 이동 패킷을 조립하고 전송하는 함수
void send_move_packet(chess::packet::MOVE_TYPE direction)
{
    // Payload: [이동 방향(1바이트)]
    // Header
    uint16_t packet_type = static_cast<uint16_t>(chess::packet::PacketType::C2S_P_MOVE);
    uint16_t total_size = sizeof(chess::packet::PacketHeader) + sizeof(direction);

    // 최종 패킷 조립
    std::vector<char> buffer(total_size);
    char* p = buffer.data();

    memcpy(p, &total_size, sizeof(total_size)); p += sizeof(total_size);
    memcpy(p, &packet_type, sizeof(packet_type)); p += sizeof(packet_type);
    memcpy(p, &direction, sizeof(direction));

    send(c_socket, buffer.data(), total_size, 0);
}

// -----------------------------------------------------------------
// 3. 패킷 수신 및 처리 로직 (전면 개편)
// -----------------------------------------------------------------

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
        // 헤더 다음 위치부터 파싱 시작
        char* payload_ptr = g_recvBuffer.data() + sizeof(chess::packet::PacketHeader);
        chess::packet::PacketType type = static_cast<chess::packet::PacketType>(header->_type);

        switch (type)
        {
            case chess::packet::PacketType::S2C_P_AVATAR_INFO:
            {
                chess::packet::SC_PACKET_AVATAR_INFO* pkt = reinterpret_cast<chess::packet::SC_PACKET_AVATAR_INFO*>(payload_ptr);
                g_myPlayer.id = pkt->_id;
                g_myPlayer.x = pkt->_x;
                g_myPlayer.y = pkt->_y;
                // 이름은 LOGIN 패킷 보낼 때 이미 알고 있으므로 여기서는 생략
                break;
            }
            case chess::packet::PacketType::S2C_P_ENTER:
            {
                chess::packet::SC_PACKET_ENTER* pkt = reinterpret_cast<chess::packet::SC_PACKET_ENTER*>(payload_ptr);
                Player newPlayer;
                newPlayer.id = pkt->_id;
                newPlayer.x = pkt->_x;
                newPlayer.y = pkt->_y;

                // 가변 길이 이름 읽기
                char* name_ptr = payload_ptr + sizeof(chess::packet::SC_PACKET_ENTER);
                uint16_t name_len = *reinterpret_cast<uint16_t*>(name_ptr);
                newPlayer.name.assign(name_ptr + sizeof(uint16_t), name_len);

                g_otherPlayers[newPlayer.id] = newPlayer;
                break;
            }
            case chess::packet::PacketType::S2C_P_MOVE:
            {
                chess::packet::SC_PACKET_MOVE* pkt = reinterpret_cast<chess::packet::SC_PACKET_MOVE*>(payload_ptr);
                if (pkt->_id == g_myPlayer.id)
                {
                    g_myPlayer.x = pkt->_x;
                    g_myPlayer.y = pkt->_y;
                }
                else
                {
                    auto it = g_otherPlayers.find(pkt->_id);
                    if (it != g_otherPlayers.end())
                    {
                        it->second.x = pkt->_x;
                        it->second.y = pkt->_y;
                    }
                }
                break;
            }
            case chess::packet::PacketType::S2C_P_LEAVE:
            {
                chess::packet::SC_PACKET_LEAVE* pkt = reinterpret_cast<chess::packet::SC_PACKET_LEAVE*>(payload_ptr);
                g_otherPlayers.erase(pkt->_id);
                break;
            }
        }

        // 4. 처리한 패킷만큼 버퍼에서 제거
        g_recvBuffer.erase(g_recvBuffer.begin(), g_recvBuffer.begin() + header->_size);
    }

    // 화면 갱신
    InvalidateRect(g_hwnd, nullptr, TRUE);
}


// -----------------------------------------------------------------
// WindowProc 및 기타 함수들 (기존 코드에서 호출부만 수정)
// -----------------------------------------------------------------

LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);
            draw_board(hdc);
            EndPaint(hWnd, &ps);
        }
        break;
        case WM_KEYDOWN:
        {
            switch (wParam)
            {
                case VK_UP:    send_move_packet(chess::packet::MOVE_TYPE::MOVE_UP); break;
                case VK_DOWN:  send_move_packet(chess::packet::MOVE_TYPE::MOVE_DOWN); break;
                case VK_LEFT:  send_move_packet(chess::packet::MOVE_TYPE::MOVE_LEFT); break;
                case VK_RIGHT: send_move_packet(chess::packet::MOVE_TYPE::MOVE_RIGHT); break;
            }
        }
        break;
        case WM_DESTROY:
            // 서버는 클라이언트의 접속 종료를 자동으로 감지하므로, 별도 패킷은 불필요.
            PostQuitMessage(0);
            break;
        default:
            return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    return 0;
}

// ... (DialogProc, draw_board, error_display 등은 기존과 거의 동일) ...
INT_PTR DialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_INITDIALOG:
            SetDlgItemText(hWnd, IDC_EDIT1, SERVER_ADDR_W.c_str());
            SetDlgItemText(hWnd, IDC_EDIT2, PLAYER_NAME_W.c_str());
            return (INT_PTR)TRUE;
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK)
            {
                wchar_t buffer1[256];
                wchar_t buffer2[256];
                GetDlgItemText(hWnd, IDC_EDIT1, buffer1, 256);
                GetDlgItemText(hWnd, IDC_EDIT2, buffer2, 256);
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

void draw_board(HDC hdc)
{
    HBRUSH hWhiteBrush = CreateSolidBrush(RGB(240, 217, 181)); // 밝은 칸
    HBRUSH hBlackBrush = CreateSolidBrush(RGB(181, 136, 99)); // 어두운 칸
    HBRUSH hBlueBrush = CreateSolidBrush(RGB(100, 149, 237)); // 다른 플레이어
    HBRUSH hRedBrush = CreateSolidBrush(RGB(255, 99, 71));    // 내 플레이어

    for (int i = 0; i < 8; ++i)
    {
        for (int j = 0; j < 8; ++j)
        {
            RECT rect = { j * CELL_SIZE, i * CELL_SIZE, (j + 1) * CELL_SIZE, (i + 1) * CELL_SIZE };
            HBRUSH currentBrush = ((i + j) % 2 == 0) ? hWhiteBrush : hBlackBrush;
            FillRect(hdc, &rect, currentBrush);
        }
    }

    // 다른 플레이어 그리기
    SelectObject(hdc, hBlueBrush);
    for (const auto& pair : g_otherPlayers)
    {
        const auto& player = pair.second;
        Ellipse(hdc, player.x * CELL_SIZE, player.y * CELL_SIZE, (player.x + 1) * CELL_SIZE, (player.y + 1) * CELL_SIZE);
    }

    // 내 플레이어 그리기
    if (g_myPlayer.id != -1)
    {
        SelectObject(hdc, hRedBrush);
        Ellipse(hdc, g_myPlayer.x * CELL_SIZE, g_myPlayer.y * CELL_SIZE, (g_myPlayer.x + 1) * CELL_SIZE, (g_myPlayer.y + 1) * CELL_SIZE);
    }

    DeleteObject(hWhiteBrush);
    DeleteObject(hBlackBrush);
    DeleteObject(hBlueBrush);
    DeleteObject(hRedBrush);
}

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
