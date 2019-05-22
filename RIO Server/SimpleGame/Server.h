#pragma once

class Scene;



enum MSGTYPE
{
	HEARTBEAT,
	MOVE,
	REMOVECLIENT
};

enum enumOperation {
	op_Send, op_Recv, op_Move,
	db_login, db_logout,
	npc_player_move
};

enum ServerOperationMode {
	MODE_NORMAL = 1,
	MODE_TEST_NORMAL,
	MODE_TEST_HOTSPOT
};

class CObject{
public:
	int				ID = NULL;
};

class CNPC :public CObject {
public:
	bool			inUse;
	bool			bActive;

	SHORT			x, y;
	lua_State*		L;
};

class BufferPiece {
public:
	char* ptr = nullptr;
	int idx = 0;
};

constexpr int NUMPIECE = 4096;
constexpr size_t BUFPIECESIZE = 16;

class BufferManager {
public:
	char* startPtr;
	BufferPiece PieceList[NUMPIECE];
	bool PieceinUse[NUMPIECE];

	bool CAS(bool* addr, bool expected, bool new_val)
	{
		return atomic_compare_exchange_strong(reinterpret_cast<volatile std::atomic_bool*>(addr), &expected, new_val);
	}

	int GetFreeBufferPieceIdx(char*& bufferptr) {
		int i = 0;
		while (true) {
			if (CAS(&PieceinUse[i], false, true)) {
				bufferptr = startPtr + i * BUFPIECESIZE;
				return i;
			}
			i++;
			if (i > NUMPIECE)
				i = 0; 
		}
	}

	void ReleaseUsedBufferPiece(int idx) {
		while (CAS(&PieceinUse[idx], true, false)) {}
		//PieceinUse[idx] = false;
	}
};

class CClient : public CNPC {
public:
	SOCKET			Client_Sock;

	std::unordered_set<int>	viewlist;
	std::mutex				viewlist_mutex;

	RIO_RQ			mRequestQueue;

	//char*			mRioSendBufferPointer;
	//char*			mRioRecvBufferPointer;
	//RIO_BUFFERID	mRioSendBufferId;
	//RIO_BUFFERID	mRioRecvBufferId;

	std::mutex		mRioRQMutex;

	char*			mRioBufferPointer;
	char*			mRioSendBufferPtr;
	RIO_BUFFERID	mRioBufferId;

	BufferManager	mBufferMng;

	int				packetsize;
	int				prev_packetsize;
	unsigned char	prev_packet[MAX_PACKET_SIZE];

	unsigned int	packet_num_arrive = 0;

	bool PostSend(void* packet);
	bool PostRecv();

	~CClient();
};

struct sEvent 
{
	long long		startTime = 0;
	enumOperation	operation;
	UINT			id;
	UINT			IOCPKey = -1;
	void*			data = nullptr;
};

struct cmp{
    bool operator()(sEvent t, sEvent u){
        return t.startTime > u.startTime;
    }
};

struct DBUserData {
	UINT Key;
	SQLINTEGER 
		nID, 
		nCHAR_LEVEL, 
		nPosX, 
		nPosY, 
		nHP,
		nExp;
};

class Server
{
private:
	int					Mode;

	WSADATA				wsa;
	SOCKET				Listen_Sock;
	SOCKADDR_IN			Server_Addr;
	HANDLE				h_IOCP;

	SQLHENV				h_env;
	SQLHDBC				h_dbc;
	SQLHSTMT			h_stmt = 0;

	CNPC				NPCList[NUM_OF_NPC];
	CClient				ClientArr[MAX_USER];

	std::thread			AccessThread;
	std::thread			TimerThread;
	std::list<thread>	WorkerThreads;

	std::priority_queue<sEvent, vector<sEvent>, cmp>	TimerEventQueue;
	std::mutex					TimerEventMutex;

	UINT			ClientCounter = 0;

	Scene*			m_pScene = NULL;

	std::mutex					m_SpaceMutex[SPACE_X * SPACE_Y];
	std::unordered_set<int>		m_Space[SPACE_X * SPACE_Y];

public:
	Server();
	~Server();

	void InitServer();
	void InitObjectList();

	void StartListen();
	void CloseServer();
	void RegisterScene(Scene* scene) { m_pScene = scene; }
	void CreateConnection(UINT clientkey);

	void SendPacket(int clientkey, void* packet);
	void SendPutObject(int client, int objid);
	void SendRemoveObject(int client, int objid);
	void SendChatPacket(int to, int from, WCHAR * message);

	CClient& GetClient(int id) { return ClientArr[id]; }
	CClient* GetClientArr() { return ClientArr; }
	CNPC& GetNPC(int id) { return NPCList[id]; }
	CNPC* GetNPClist() { return NPCList; }
	HANDLE GetIOCP() { return h_IOCP; }


	void AddTimerEvent(UINT id, enumOperation op, long long time);

	long long GetTime();

	bool CanSee(int a, int b);
	void DisConnectClient(int key);
	void MoveNPC(int key);

	std::unordered_set<int>& GetSpace(int idx) { return m_Space[idx];}
	std::mutex& GetSpaceMutex(int idx) { return m_SpaceMutex[idx]; }
	int GetSpaceIndex(int id) {
		CNPC* obj = nullptr;
		if (id >= NPC_START)		obj = &NPCList[id];
		else if (id < NPC_START)	obj = &ClientArr[id];
		int res =  (obj->x / SPACESIZE) + (obj->y / SPACESIZE) * SPACE_X;
		if (res > 1600) {
			int a = 0;
		}
		return res;
	}

	bool ChkInSpace(int clientid, int targetid);

	void WorkThreadProcess(int threadid);
	void TimerThreadProcess();

	static RIO_EXTENSION_FUNCTION_TABLE mRioFunctionTable;
	//static RIO_CQ mRioSendCompletionQueue[MAX_RIO_THREAD + 1];
	//static RIO_CQ mRioRecvCompletionQueue[MAX_RIO_THREAD + 1];
	static RIO_CQ mRioCompletionQueue[MAX_RIO_THREAD + 1];
};

struct RioIoContext : public RIO_BUF
{
	RioIoContext(CClient* client, enumOperation ioType) : mClientSession(client), mIoType(ioType) {}

	CClient* mClientSession;
	enumOperation	mIoType;
	int	mSendBufIdx = -1;
};

#define RIO	Server::mRioFunctionTable

