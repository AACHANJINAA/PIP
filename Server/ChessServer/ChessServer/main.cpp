#include "main.h"

constexpr short SERVER_PORT = 3000; // 내가 가지고 있어야함

namespace chess::main
{
	std::unordered_map<long long, chess::packet::PositionPacket> g_positions;
	std::unordered_map<long long, chess::server::SESSION> g_users;
	
	void error_display(const char* msg, int err_no)
	{
		if (WSA_IO_PENDING == err_no)
		{
			return;
		}
		WCHAR* lpMsgBuf;
		FormatMessage(
			FORMAT_MESSAGE_ALLOCATE_BUFFER |
			FORMAT_MESSAGE_FROM_SYSTEM,
			NULL, err_no,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPTSTR)&lpMsgBuf, 0, NULL);
		std::cout << msg;
		std::wcout << L" 에러 " << lpMsgBuf << std::endl;
#ifdef _DEBUG
		while (true); // 디버깅 용 그냥 죽으면 안되니까
#endif

		LocalFree(lpMsgBuf);
	}
}
int main()
{
	std::wcout.imbue(std::locale("korean"));
	WSAData wsadata;
	WSAStartup(MAKEWORD(2, 2), &wsadata);

	SOCKET s_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, 0, 0, WSA_FLAG_OVERLAPPED);
	if (s_socket <= 0) std::cout << "ERROR" << "원인";
	else std::cout << "Socket Created.\n";

	SOCKADDR_IN addr; //운영체제마다 라이브러리 크기가 달라짐
	addr.sin_family = AF_INET;
	addr.sin_port = htons(SERVER_PORT);
	addr.sin_addr.S_un.S_addr = htonl(INADDR_ANY);
	bind(s_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(SOCKADDR_IN));
	listen(s_socket, SOMAXCONN);
	INT addr_size = sizeof(SOCKADDR_IN);

	long long client_id = 0;
	while (true)
	{
		auto s_client = WSAAccept(s_socket, reinterpret_cast<sockaddr*>(&addr), &addr_size,NULL, NULL);
		//SleepEx안해도됨
		chess::main::g_users.try_emplace(client_id, client_id, s_client);
		chess::main::g_users[client_id].send_id();
		chess::main::g_positions.try_emplace(client_id, client_id);
		
		++client_id;
	}
	
	closesocket(s_socket);
	WSACleanup();
}


