#include <iostream>
#include <thread>
#include <WinSock2.h>
#include <Windows.h>
#include <conio.h>
#include <vector>
#include <set>
#include <mutex>
#include <chrono>
#include <queue>
#include <atomic>

#include "../../IOCPServer/SimpleGame/protocol.h"

#pragma comment (lib, "Ws2_32.lib")
#pragma warning (disable:4996)

using namespace std;
using namespace chrono;

const char *FS_GAME_SERVER_ADDRESS = "127.0.0.1";
const char *ServerName = "nhjung Local Test Server";
const int FS_GAME_SERVER_PORT = SERVERPORT;

atomic <int> g_room_serial = 1;
atomic <int> g_normal_end_client = 0;
atomic <int> g_total_move_packet_sent = 0;
atomic <int> g_total_move_packet_received = 0;

int		g_total_packet_sent[1000];
int		g_total_packet_received[1000];
atomic <int> g_unique_index = 0;

bool	g_shutdown = false;


const auto NUM_TEST = 500;

const auto NUM_THREADS = 6;
const auto X_START_POS = 4;
const auto Y_START_POS = 4;

const int OP_SEND = 0;
const int OP_RECV = 1;

__declspec(thread) int tls_my_thread_id;

struct network_info {
	WSAOVERLAPPED overlapped;
	SOCKET s;
	int operation_type;
	WSABUF wsabuf;
	char IOCPbuf[MAX_BUFF_SIZE];
	char PacketBuf[MAX_PACKET_SIZE];
	int prev_data_size;
	int curr_packet_size;
};

struct fVector3 {
	float x;
	float y;
	float z;
};

struct player {
	fVector3 pos;
	fVector3 rot;
	bool connected;
	bool game_finished;
	sockaddr_in	connection;
	network_info overlapped_ex;
};

int g_inbound_packet_num[NUM_THREADS][MAX_USER];
int g_outbound_Packet_num[NUM_THREADS][MAX_USER];

player players[NUM_TEST];

HANDLE ghIOCP;

void error_display(char *msg, int err_no)
{
	WCHAR *lpMsgBuf;
	FormatMessage(
		FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM,
		NULL, err_no,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
		(LPTSTR)&lpMsgBuf, 0, NULL);
	printf("%s", msg);
	wprintf(L"에러[%s]\n", lpMsgBuf);
	LocalFree(lpMsgBuf);
}


void Init_Player(int id)
{
	ZeroMemory(&players[id], sizeof(player));
	players[id].overlapped_ex.operation_type = OP_RECV;
	players[id].overlapped_ex.wsabuf.buf = players[id].overlapped_ex.IOCPbuf;
	players[id].overlapped_ex.wsabuf.len = MAX_BUFF_SIZE;
	players[id].connected = false;
	players[id].game_finished = false;
}

void ShutDown_Server()
{
	WSACleanup();
}


void Init_Server()
{
	WSADATA wsadata;

	_wsetlocale(LC_ALL, L"korean");
	WSAStartup(MAKEWORD(2, 2), &wsadata);

	for (int i = 0; i < NUM_TEST; ++i) {
		Init_Player(i);
		players[i].connected = false;
		players[i].game_finished = false;
	}
}

void Send_Packet(void *packet, unsigned id)
{
	{
		BYTE *P = reinterpret_cast<BYTE *>(packet);
		switch (P[1]) {
		case CS_UP:
		case CS_DOWN:
		case CS_LEFT:
		case CS_RIGHT:
			// printf("SC_GAME_OBJECT_MOVE packet sent\n");
			break;
		default:
			printf("Unrecognized packet was sent\n");
			break;
		}

	}
	network_info *over_ex = new network_info;
	int packet_size = reinterpret_cast<char *>(packet)[0];
	memcpy(over_ex->IOCPbuf, packet, packet_size);
	over_ex->operation_type = OP_SEND;
	ZeroMemory(&over_ex->overlapped, sizeof(WSAOVERLAPPED));
	over_ex->wsabuf.buf = over_ex->IOCPbuf;
	over_ex->wsabuf.len = packet_size;
	unsigned long IOsize;
	WSASend(players[id].overlapped_ex.s, &over_ex->wsabuf, 1,
		&IOsize, NULL, &over_ex->overlapped, NULL);
}

void DisConnect(int id)
{
	players[id].connected = false;
	players[id].game_finished = true;
	closesocket(players[id].overlapped_ex.s);
}

void Process_Packet(char *packet, int id)
{
	switch (packet[1]) {
	case SC_POS: {
		sc_packet_pos *P = reinterpret_cast<sc_packet_pos *>(packet);
		players[P->id].pos.x = P->x;
		players[P->id].pos.y = P->y;
		g_total_move_packet_received++;
		g_total_packet_received[id]++;
	}
							  break;
	case SC_REMOVE_PLAYER: {
		if (false == players[id].game_finished) g_normal_end_client++;
		players[id].game_finished = true;
		DisConnect(id);
	}
					  break;
	default: {
		cout << "Unknown Packet Type " << packet[1] << endl;
		//while (true);
		//exit(-1); 
	}
	}
}

void Worker_Thread(int id)
{
	unsigned long IOsize;
	unsigned long long key;
	network_info *over_ex;

	tls_my_thread_id = id;

	while (true) {

		if (true == g_shutdown) break;
		BOOL succ = GetQueuedCompletionStatus(ghIOCP, &IOsize, &key,
			reinterpret_cast<LPOVERLAPPED *>(&over_ex),
			1000);
		if (FALSE == succ) {
			if (nullptr == over_ex) continue;  // TimeOut we should check server shutdown
			else {
				int error_no = WSAGetLastError();
				if ((error_no == ERROR_NETNAME_DELETED) || (error_no == ERROR_OPERATION_ABORTED)) {
					// 서버에서 강제 종료.
					DisConnect(static_cast<int>(key));
					continue;
				}
				//error_display("GQCS:", WSAGetLastError());
			}
		}
		// ERROR 처리
		// 접속 종료 처리
		if (0 == IOsize) {
			DisConnect(static_cast<int>(key));
			continue;
		}
		if (over_ex->operation_type == OP_RECV) {
			// 페킷조립 및 실행
			unsigned data_to_process = IOsize;
			unsigned char * buf = reinterpret_cast<unsigned char *>(over_ex->IOCPbuf);
			while (0 < data_to_process) {
				if (0 == over_ex->curr_packet_size) {
					over_ex->curr_packet_size = buf[0];
					if (buf[0] > 200) {
						printf("Invalid Packet Size [%d]", buf[0]);
						printf("Terminating Server!\n");
						while (true);
						//exit(-1);
					}
				}
				unsigned need_to_build = over_ex->curr_packet_size - over_ex->prev_data_size;
				if (need_to_build <= data_to_process) {
					// 패킷 조립
					memcpy(over_ex->PacketBuf + over_ex->prev_data_size, buf, need_to_build);
					Process_Packet(over_ex->PacketBuf, static_cast<int>(key));
					over_ex->curr_packet_size = 0;
					over_ex->prev_data_size = 0;
					data_to_process -= need_to_build;
					buf += need_to_build;
				}
				else {
					// 훗날을 기약
					memcpy(over_ex->PacketBuf + over_ex->prev_data_size, buf, data_to_process);
					over_ex->prev_data_size += data_to_process;
					data_to_process = 0;
					buf += data_to_process;
				}
			}
			// 다시 RECV
			unsigned long recv_flag = 0;
			WSARecv(over_ex->s, &over_ex->wsabuf, 1, NULL, &recv_flag, &over_ex->overlapped, NULL);
		}
		else if (over_ex->operation_type == OP_SEND) {
			if (over_ex->wsabuf.buf[0] != IOsize) {
				printf("Incompleted Send Call: closing connection for [%lld]\n", key);
				players[key].connected = false;
				closesocket(over_ex->s);
			}
			delete over_ex;
		}
		else {
			cout << "Unknown Operation Type Detected in worker thread!\n";
			while (true);
			//exit(-1);
		}
	}
}

void Connect_Thread()
{
	SOCKADDR_IN clntAdr;

	clntAdr.sin_family = AF_INET;
	clntAdr.sin_addr.s_addr = inet_addr("127.0.0.1");
	clntAdr.sin_port = htons(SERVERPORT);

	for (auto i = 0; i < NUM_TEST; ++i)
	{
		SOCKET new_socket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, NULL, 0, WSA_FLAG_OVERLAPPED);
		HANDLE iocp = CreateIoCompletionPort((HANDLE)new_socket, ghIOCP, i, 0);

		if ((WSAConnect(new_socket, (SOCKADDR*)&clntAdr, sizeof(clntAdr), NULL, NULL, NULL, NULL)) == SOCKET_ERROR) {
			printf("WSAConnect() Error!!\n");
			players[i].game_finished = true;
			continue;
		}

		Init_Player(i);

		players[i].overlapped_ex.s = new_socket;

		unsigned long recv_flag = 0;
		int err = WSARecv(new_socket,
			&players[i].overlapped_ex.wsabuf, 1,
			NULL, &recv_flag,
			reinterpret_cast<LPOVERLAPPED>(&players[i].overlapped_ex), NULL);
		if (SOCKET_ERROR == err) {
			int err_code = WSAGetLastError();
			if (WSA_IO_PENDING != err_code) {
				cout << "WSA RECV error" << endl;
				while (true);
				//error_display("CONNECT(WSARecv):", WSAGetLastError());
				exit(-1);
			}
		}
		if (99 == (i % 100)) printf("Client[%d] Connected\n", i + 1);

		players[i].connected = true;
	}
}

int main()
{
	vector <thread *> worker_threads;

	//	SetUnhandledExceptionFilter(unhandled_handler);

	Init_Server();
	ghIOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 0);
	for (int i = 0; i < NUM_THREADS; ++i)
		worker_threads.push_back(new thread{ Worker_Thread, i });
	auto accept_thread = thread{ Connect_Thread };

	auto last_process_time = chrono::high_resolution_clock::now();

	bool all_client_finished = false;


	do {
		Sleep(1);
		auto now = chrono::high_resolution_clock::now();
		auto duration = now - last_process_time;
		long long msec = chrono::duration_cast<milliseconds>(duration).count();
		if (msec < 100) continue;
		last_process_time = now;

		all_client_finished = true;

		for (auto i = 0; i < NUM_TEST; ++i)
		{
			if (true == players[i].game_finished) continue;
			all_client_finished = false;
			if (false == players[i].connected) continue;

			g_total_packet_sent[i]++;
			g_total_move_packet_sent++;

			cs_packet_move p;
			p.size = sizeof(cs_packet_move);
			switch (rand() % 4)
			{
			case 0: p.type = CS_UP; break;
			case 1: p.type = CS_DOWN; break;
			case 2: p.type = CS_LEFT; break;
			case 3: p.type = CS_RIGHT; break;
			}
			Send_Packet(&p, i);
		}
	} while (false == all_client_finished);
	g_shutdown = true;

	printf("TOTAL Client Started:%d\n", NUM_TEST);
	printf("TOTAL Game Playe SUCCESS:%d\n", static_cast<int>(g_normal_end_client));
	printf("TOTAL Move Packet Sent:%d\n", static_cast<int>(g_total_move_packet_sent));
	printf("TOTAL Move Packet Received:%d\n", static_cast<int>(g_total_move_packet_received));

	for (int i = 0; i < 30; ++i) {
		cout << "Client[" << i << "] sent [" << g_total_packet_sent[i] << "], received [" << g_total_packet_received[i] << "]\n";
	}

	int zero_packet = 0;
	int under500_packet = 0;
	int under1000_packet = 0;
	for (int i = 0; i < NUM_TEST; ++i)
		if (0 == g_total_packet_sent[i]) zero_packet++;
		else if (500 > g_total_packet_sent[i]) under500_packet++;
		else if (1000 > g_total_packet_sent[i]) under1000_packet++;

	cout << "Number of Clients that failed to send Packet[" << zero_packet << "]\n";
	cout << "Number of Clients that sent under 500 Packets[" << under500_packet << "]\n";
	cout << "Number of Clients that sent under 1000 Packets[" << under1000_packet << "]\n";

	printf("-------------------------Pess Any Key to Finish-----------------");
	while (true)
	{
		if (_kbhit()) break;
		Sleep(100);
	}


	for (auto t : worker_threads) t->join();
	accept_thread.join();
	ShutDown_Server();
}