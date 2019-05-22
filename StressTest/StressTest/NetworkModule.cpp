#include <WinSock2.h>
#include <winsock.h>
#include <Windows.h>
#include <iostream>
#include <thread>
#include <vector>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include <chrono>
#include <queue>
#include <array>
#include <unordered_map>
#include "NetworkModule.h"

using namespace std;
using namespace chrono;

const static int MAX_TEST = 5000;
const static int INVALID_ID = -1;

bool bRunning = true;

#include "..\..\RIO Server\SimpleGame\protocol.h"

HANDLE g_hiocp;

enum OPTYPE { OP_SEND, OP_RECV, OP_DO_MOVE };

high_resolution_clock::time_point last_connect_time;

struct OverlappedEx {
	WSAOVERLAPPED over;
	WSABUF wsabuf;
	unsigned char IOCP_buf[MAX_BUFF_SIZE];
	OPTYPE event_type;
	int event_target;
};

struct CLIENT {
	int id;
	int x;
	int y;
	high_resolution_clock::time_point last_move_time;
	bool connect;

	SOCKET client_socket;
	OverlappedEx recv_over;
	unsigned char packet_buf[MAX_PACKET_SIZE];
	int prev_packet_data;
	int curr_packet_size;
};

array<CLIENT, MAX_USER> g_clients;
atomic_int num_connections;

vector <thread *> worker_threads;
thread test_thread;

float point_cloud[MAX_USER * 2];

// ���߿� NPC���� �߰� Ȯ�� ��
struct ALIEN {
	int id;
	int x, y;
	int visible_count;
};

void error_display(char *msg, int err_no)
{
	WCHAR *lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, err_no,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	std::cout << msg;
	std::wcout << L"����" << lpMsgBuf << std::endl;
	LocalFree(lpMsgBuf);
	//while (true);
}

void DisconnectClient(int ci)
{
	closesocket(g_clients[ci].client_socket);
	g_clients[ci].connect = false;
	cout << "Client [" << ci << "] Disconnected!\n";
}

void ProcessPacket(int ci, unsigned char packet[])
{
	switch (packet[1]) {
	case SC_POS: {
		sc_packet_pos *pos_packet = reinterpret_cast<sc_packet_pos *>(packet);
		if (INVALID_ID == g_clients[ci].id) g_clients[ci].id = ci;
		if (ci == pos_packet->id) {
			g_clients[ci].x = pos_packet->x;
			g_clients[ci].y = pos_packet->y;
		}
		} break;
	case SC_PUT_PLAYER: break;
	case SC_REMOVE_PLAYER: break;
	case SC_CHAT: break;
	default: std::cout << "Unknown Packet Type from Server : " << ci << std::endl;
		break;
	}
}

void Worker_Thread()
{
	while (true) {
		if (bRunning == false) {
			cout << "Close Workerthread\n";
			return;
		}
		DWORD io_size;
		unsigned long ci;
		OverlappedEx *over;
		BOOL ret = GetQueuedCompletionStatus(g_hiocp, &io_size, &ci,
			reinterpret_cast<LPWSAOVERLAPPED *>(&over), INFINITE);
		// std::cout << "GQCS :";
		if (FALSE == ret) {
			int err_no = WSAGetLastError();
			if (64 == err_no) DisconnectClient(ci);
			else error_display("GQCS : ", WSAGetLastError());
		}
		if (0 == io_size) {
			DisconnectClient(ci);
			continue;
		}
		if (OP_RECV == over->event_type) {
			std::cout << "RECV from Client :" << ci;
			std::cout << "  IO_SIZE : " << io_size << std::endl;
			unsigned char *buf = g_clients[ci].recv_over.IOCP_buf;
			unsigned psize = g_clients[ci].curr_packet_size;
			unsigned pr_size = g_clients[ci].prev_packet_data;
			while (io_size > 0) {
				if (0 == psize) psize = buf[0];
				if (io_size + pr_size >= psize) {
					// ���� ��Ŷ �ϼ� ����
					unsigned char packet[MAX_PACKET_SIZE];
					memcpy(packet, g_clients[ci].packet_buf, pr_size);
					memcpy(packet + pr_size, buf, psize - pr_size);
					ProcessPacket(static_cast<int>(ci), packet);
					io_size -= psize - pr_size;
					buf += psize - pr_size;
					psize = 0; pr_size = 0;
				}
				else {
					memcpy(g_clients[ci].packet_buf + pr_size, buf, io_size);
					pr_size += io_size;
					io_size = 0;
				}
			}
			g_clients[ci].curr_packet_size = psize;
			g_clients[ci].prev_packet_data = pr_size;
			DWORD recv_flag = 0;
			WSARecv(g_clients[ci].client_socket,
				&g_clients[ci].recv_over.wsabuf, 1,
				NULL, &recv_flag, &g_clients[ci].recv_over.over, NULL);
		}
		else if (OP_SEND == over->event_type) {
			if (io_size != over->wsabuf.len) {
				std::cout << "Send Incomplete Error!\n";
				closesocket(g_clients[ci].client_socket);
				g_clients[ci].connect = false;
			}
			delete over;
		}
		else if (OP_DO_MOVE == over->event_type) {
			// Not Implemented Yet
			delete over;
		}
		else {
			std::cout << "Unknown GQCS event!\n";
			while (true);
		}
	}
}

void Adjust_Number_Of_Client()
{
	if (num_connections >= MAX_USER) return;
	if (high_resolution_clock::now() < last_connect_time + 100ms) return;

	g_clients[num_connections].client_socket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);

	SOCKADDR_IN ServerAddr;
	ZeroMemory(&ServerAddr, sizeof(SOCKADDR_IN));
	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_port = htons(4000);
	ServerAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

	int Result = WSAConnect(g_clients[num_connections].client_socket, (sockaddr *)&ServerAddr, sizeof(ServerAddr), NULL, NULL, NULL, NULL);

	g_clients[num_connections].curr_packet_size = 0;
	g_clients[num_connections].prev_packet_data = 0;
	ZeroMemory(&g_clients[num_connections].recv_over, sizeof(g_clients[num_connections].recv_over));
	g_clients[num_connections].recv_over.event_type = OP_RECV;
	g_clients[num_connections].recv_over.wsabuf.buf =
		reinterpret_cast<CHAR *>(g_clients[num_connections].recv_over.IOCP_buf);
	g_clients[num_connections].recv_over.wsabuf.len = sizeof(g_clients[num_connections].recv_over.IOCP_buf);

	DWORD recv_flag = 0;
	CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_clients[num_connections].client_socket), g_hiocp, num_connections, 0);
	WSARecv(g_clients[num_connections].client_socket, &g_clients[num_connections].recv_over.wsabuf, 1,
		NULL, &recv_flag, &g_clients[num_connections].recv_over.over, NULL);
	g_clients[num_connections].connect = true;
	num_connections++;
}

void SendPacket(int cl, void *packet)
{
	int psize = reinterpret_cast<unsigned char *>(packet)[0];
	int ptype = reinterpret_cast<unsigned char *>(packet)[1];
	OverlappedEx *over = new OverlappedEx;
	over->event_type = OP_SEND;
	memcpy(over->IOCP_buf, packet, psize);
	ZeroMemory(&over->over, sizeof(over->over));
	over->wsabuf.buf = reinterpret_cast<CHAR *>(over->IOCP_buf);
	over->wsabuf.len = psize;
	int ret = WSASend(g_clients[cl].client_socket, &over->wsabuf, 1, NULL, 0,
		&over->over, NULL);
	if (0 != ret) {
		int err_no = WSAGetLastError();
		if (WSA_IO_PENDING != err_no)
			error_display("Error in SendPacket:", err_no);
	}
	std::cout << "Send Packet [" << ptype << "] To Client : " << cl << std::endl;
}

void Test_Thread()
{
	while (true) {
		if (bRunning == false)
			return;
		Sleep(10);
		Adjust_Number_Of_Client();

		for (int i = 0; i < num_connections; ++i) {
			if (false == g_clients[i].connect) continue;
			if (g_clients[i].last_move_time + 1s > high_resolution_clock::now()) continue;
			g_clients[i].last_move_time = high_resolution_clock::now();

			cs_packet_move my_packet;
			my_packet.size = sizeof(cs_packet_move);

			switch (rand() % 4) {
			case 0: my_packet.type = CS_UP; break;
			case 1: my_packet.type = CS_DOWN; break;
			case 2: my_packet.type = CS_LEFT; break;
			case 3: my_packet.type = CS_RIGHT; break;
			}
			SendPacket(i, &my_packet);
		}
	}
}

void TrunOffServer()
{
	bRunning = false;
	for(int i=0; i<6; ++i)
		PostQueuedCompletionStatus(g_hiocp, 0, 0, nullptr);
}

void InitializeNetwork()
{
	for (int i = 0; i < MAX_USER; ++i) {
		g_clients[i].connect = false;
		g_clients[i].id = INVALID_ID;
	}
	num_connections = 0;
	last_connect_time = high_resolution_clock::now();

	WSADATA	wsadata;
	WSAStartup(MAKEWORD(2, 2), &wsadata);

	g_hiocp = CreateIoCompletionPort(INVALID_HANDLE_VALUE, 0, NULL, 0);

	for (int i = 0; i < 6; ++i)
		worker_threads.push_back(new std::thread{ Worker_Thread });

	Sleep(1000);
	test_thread = thread{ Test_Thread };
}

void ShutdownNetwork()
{
	test_thread.join();
	for (auto pth : worker_threads) {
		pth->join();
		delete pth;
	}
}

void Do_Network()
{
	return;
}

void GetPointCloud(int *size, float **points)
{
	for (int i = 0; i < num_connections; ++i) {
		point_cloud[i * 2] = g_clients[i].x;
		point_cloud[i * 2 + 1] = g_clients[i].y;
	}
	*size = num_connections;
	*points = point_cloud;
}

