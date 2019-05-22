#pragma once
#define DEFAULTIP	"127.0.0.1"

class Scene;

typedef std::chrono::time_point<std::chrono::steady_clock> TIMEPOINT;

enum MSGTYPE
{
	HEARTBEAT,
	MOVE,
	ADDCLIENT,
	REMOVECLIENT
};

class Client
{
	WSADATA				wsa;
	int					ClientID;
	Scene*				pScene = nullptr;

	SOCKET				Server_Sock;
	SOCKADDR_IN			Server_Addr;
	char				Server_IP[22];

	WSAEVENT			hWsaEvent;
	WSANETWORKEVENTS	netEvents;
	thread				recvThread;

	WSABUF				send_wsabuf;
	char 				send_buffer[MAX_BUFF_SIZE];
	WSABUF				recv_wsabuf;
	char				recv_buffer[MAX_BUFF_SIZE];
	char				packet_buffer[MAX_BUFF_SIZE];
	DWORD				in_packet_size = 0;
	int					saved_packet_size = 0;


public:
	Client();
	~Client();
	void InitClient();
	void StartClient();
	void CloseClient();
	void CompletePacket();
	
	void RegisterScene(Scene* s) { pScene = s; }
	void SendPacket(char* packet);

	int	GetID() { return ClientID; }
	unsigned int		packet_count = 100;

	TIMEPOINT			packet_time_send;

};
