#include "stdafx.h"
#include "Scene.h"
#include "Client.h"

Client::Client()
{
}

Client::~Client()
{
}

void Client::InitClient()
{
	int retval = 0;
	std::cout << "Enter ServerIP (default : 0) : ";
	std::cin >> Server_IP;
	if (!strcmp(Server_IP, "0"))
		strcpy_s(Server_IP, sizeof(DEFAULTIP), DEFAULTIP);

	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		std::cout << "Error :: init WinSock" << std::endl;
		exit(1);
	}

	Server_Sock = socket(AF_INET, SOCK_STREAM, 0);
	if (Server_Sock == INVALID_SOCKET) {
		std::cout << "Error :: init ServerSocket" << std::endl;
		exit(1);
	}

	ZeroMemory(&Server_Addr, sizeof(Server_Addr));
	Server_Addr.sin_family = AF_INET;
	inet_pton(AF_INET, Server_IP, &Server_Addr.sin_addr);
	Server_Addr.sin_port = htons(SERVERPORT);

	retval = WSAConnect(Server_Sock, (sockaddr *)&Server_Addr, sizeof(Server_Addr), NULL, NULL, NULL, NULL);
	if (retval == SOCKET_ERROR) {
		std::cout << "Error :: Connect Server" << std::endl;
		exit(1);
	}

	send_wsabuf.buf = send_buffer;
	send_wsabuf.len = MAX_BUFF_SIZE;
	recv_wsabuf.buf = recv_buffer;
	recv_wsabuf.len = MAX_BUFF_SIZE;

	hWsaEvent = WSACreateEvent();
	WSAEventSelect(Server_Sock, hWsaEvent, FD_CLOSE | FD_READ);
	recvThread = thread{ [&]() { CompletePacket(); } };

	//std::cout << "Enter ID : ";
	//std::cin >> ClientID;
	//
	//cs_packet_login p;
	//p.size = sizeof(cs_packet_login);
	//p.type = CS_LOGIN;
	//p.id = ClientID;
	//SendPacket((char*)&p);
}

void Client::StartClient()
{
}

void Client::CloseClient()
{
	closesocket(Server_Sock);
	printf("Client Disconnected :: IP : %s, PORT : %d\n", Server_IP, ntohs(Server_Addr.sin_port));

	closesocket(Server_Sock);

	WSACleanup();
}

void Client::CompletePacket()
{
	DWORD iobyte = 0, ioflag = 0;
	int ret;
	while (true) {
		ret = WSAWaitForMultipleEvents(1, &hWsaEvent, FALSE, WSA_INFINITE, FALSE);
		if (ret == WSA_WAIT_FAILED || ret == WSA_WAIT_TIMEOUT) 
			continue;

		WSAEnumNetworkEvents(Server_Sock, hWsaEvent, &netEvents);

		if (netEvents.lNetworkEvents & FD_READ) 
		{
			if (netEvents.iErrorCode[FD_READ_BIT] != 0)
			{
				int err_code = WSAGetLastError();
				printf("Recv Error [%d]\n", err_code);
				continue;
			}

			ret = WSARecv(Server_Sock, &recv_wsabuf, 1, &iobyte, &ioflag, NULL, NULL);
			if (ret) {
				int err_code = WSAGetLastError();
				printf("Recv Error [%d]\n", err_code);
				continue;
			}

			BYTE *ptr = reinterpret_cast<BYTE *>(recv_buffer);

			while (0 != iobyte)
			{
				if (0 == in_packet_size)
					in_packet_size = ptr[0];

				if (iobyte + saved_packet_size >= in_packet_size)
				{
					memcpy(packet_buffer + saved_packet_size, ptr, in_packet_size - saved_packet_size);

					if (pScene != nullptr)
						pScene->ProcessPacket(packet_buffer);

					ptr += in_packet_size - saved_packet_size;
					iobyte -= in_packet_size - saved_packet_size;
					in_packet_size = 0;
					saved_packet_size = 0;
				}
				else
				{
					memcpy(packet_buffer + saved_packet_size, ptr, iobyte);
					saved_packet_size += iobyte;
					iobyte = 0;
				}
			}
		}
		if (netEvents.lNetworkEvents & FD_CLOSE)
		{
			int err_code = WSAGetLastError();
			printf("Recv Error [%d]\n", err_code);
			break;
		}
	}
}

void Client::SendPacket(char* packet)
{
	DWORD iobyte = 0;

	memcpy(send_buffer, packet, packet[0]);
	send_wsabuf.len = packet[0];
	int ret = WSASend(Server_Sock, &send_wsabuf, 1, &iobyte, 0, NULL, NULL);
	if (ret) {
		int error_code = WSAGetLastError();
		printf("Error while sending packet [%d]", error_code);
	}
}
