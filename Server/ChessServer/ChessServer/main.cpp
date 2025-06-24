#include "main.h"

constexpr short SERVER_PORT = 3000; // 내가 가지고 있어야함

namespace chess
{
	std::unordered_map<long long, chess::packet::PositionPacket> g_positions;
	std::unordered_map<long long, chess::server::SESSION> g_users;

}

using namespace chess;

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
		g_users.try_emplace(client_id, client_id, s_client);
		g_users[client_id].send_id();
		g_positions.try_emplace(client_id, client_id);
		
		++client_id;
	}
	
	closesocket(s_socket);
	WSACleanup();
}


