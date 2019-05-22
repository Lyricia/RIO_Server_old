#include "stdafx.h"
#include "Scene.h"
#include "Server.h"


inline void CRASH_ASSERT(bool isOk)
{
	if (isOk)
		return;

	int* crashVal = 0;
	*crashVal = 0xDEADBEEF;
}

Server* Server_Instance;

RIO_EXTENSION_FUNCTION_TABLE Server::mRioFunctionTable = { 0, };
RIO_CQ Server::mRioCompletionQueue[MAX_RIO_THREAD + 1] = { 0, };
//RIO_CQ Server::mRioSendCompletionQueue[MAX_RIO_THREAD + 1] = { 0, };
//RIO_CQ Server::mRioRecvCompletionQueue[MAX_RIO_THREAD + 1] = { 0, };


void HandleDiagnosticRecord(SQLHANDLE hHandle, SQLSMALLINT hType, RETCODE RetCode)
{
	SQLSMALLINT iRec = 0;
	SQLINTEGER  iError;
	WCHAR       wszMessage[1000];
	WCHAR       wszState[SQL_SQLSTATE_SIZE + 1];

	if (RetCode == SQL_INVALID_HANDLE) {
		fwprintf(stderr, L"Invalid handle!\n");
		return;
	}
	while (SQLGetDiagRecW(hType, hHandle, ++iRec, wszState, &iError, wszMessage,
		(SQLSMALLINT)(sizeof(wszMessage) / sizeof(WCHAR)), (SQLSMALLINT *)NULL) == SQL_SUCCESS) {
		// Hide data truncated..
		if (wcsncmp(wszState, L"01004", 5)) {
			fwprintf(stderr, L"[%5.5s] %s (%d)\n", wszState, wszMessage, iError);
		}
	}
}
void DisplayError(lua_State* L, int error) {
	printf("Error : %s\n", lua_tostring(L, -1));
	lua_pop(L, 1);
}

int CAPI_sendMessage(lua_State* L)
{
	char* message = (char *)lua_tostring(L, -1);
	int chatter = (int)lua_tointeger(L, -2);
	int player = (int)lua_tointeger(L, -3);
	lua_pop(L, 4);

	wchar_t wmess[MAX_STR_SIZE];
	size_t len = strlen(message), wlen;
	if (len >= MAX_STR_SIZE) {
		len = MAX_STR_SIZE - 1;
	}
	mbstowcs_s(&wlen, wmess, len, message, _TRUNCATE);
	wmess[MAX_STR_SIZE - 1] = (wchar_t)0;

	Server_Instance->SendChatPacket(player, chatter, wmess);
	return 0;
}
int CAPI_get_x(lua_State* L)
{
	int id = (int)lua_tointeger(L, -1);
	lua_pop(L, 2);
	int x;
	if (id >= NPC_START) {
		x = Server_Instance->GetNPC(id).x;
	}
	else {
		x = Server_Instance->GetClient(id).x;
	}
	lua_pushnumber(L, x);
	return 1;
}
int CAPI_get_y(lua_State* L)
{
	int id = (int)lua_tointeger(L, -1);
	lua_pop(L, 2);
	int y;
	if (id >= NPC_START) {
		y = Server_Instance->GetNPC(id).y;
	}
	else {
		y = Server_Instance->GetClient(id).y;
	}
	lua_pushnumber(L, y);
	return 1;
}

Server::Server()
{
}

Server::~Server()
{
}

void Server::InitServer()
{
	cout << "Select Operation Mode\n";
	cout << "1. Normal\n";
	cout << "2. Server Test Normal\n";
	cout << "3. Server Test Hotspot\n";
	cout << "Select : ";
	std::cin >> Mode;

	Server_Instance = this;
	setlocale(LC_ALL, "korean");
	std::wcout.imbue(std::locale("korean"));

	int retval;
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return;

	// IOCP
	h_IOCP = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
	if (h_IOCP == NULL) {
		std::cout << "Error :: init IOCP" << std::endl;
		return;
	}

	//Listen_Sock = WSASocketW(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
	Listen_Sock = WSASocketW(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_REGISTERED_IO);
	if (Listen_Sock == INVALID_SOCKET) {
		std::cout << "Error :: init ListenSocket" << std::endl;
		return;
	}

	ZeroMemory(&Server_Addr, sizeof(Server_Addr));
	Server_Addr.sin_family = AF_INET;
	Server_Addr.sin_addr.s_addr = htonl(INADDR_ANY);
	Server_Addr.sin_port = htons(SERVERPORT);
	retval = ::bind(Listen_Sock, (SOCKADDR *)&Server_Addr, sizeof(Server_Addr));
	if (retval == SOCKET_ERROR) {
		std::cout << "Error :: Bind Server Addr" << std::endl;
		return;
	}

	/// RIO function table
	GUID functionTableId = WSAID_MULTIPLE_RIO;
	DWORD dwBytes = 0;

	if (WSAIoctl(Listen_Sock, SIO_GET_MULTIPLE_EXTENSION_FUNCTION_POINTER, &functionTableId, sizeof(GUID), (void**)&mRioFunctionTable, sizeof(mRioFunctionTable), &dwBytes, NULL, NULL))
		return;

	
	InitObjectList();

	// ThreadPool
	for (UINT i = 0; i < std::thread::hardware_concurrency(); i++)
	{
		WorkerThreads.emplace_back(std::thread{ [this, i]() { WorkThreadProcess(i); } });
	}

	//TimerThread = thread{ [this]() { TimerThreadProcess(); } };

	std::cout << "Server Initiated" << std::endl;
}

void Server::InitObjectList()
{
	//SYSTEM_INFO systemInfo;
	//GetSystemInfo(&systemInfo);
	//const unsigned __int64 granularity = systemInfo.dwAllocationGranularity; ///< maybe 64K
	//CRASH_ASSERT(SESSION_BUFFER_SIZE % granularity == 0);

	for (auto & client : ClientArr) {
		client.viewlist.clear();
		client.inUse = false;
		client.packetsize = 0;
		client.prev_packetsize = 0;
		client.ID = -1;
		//client.mRioSendBufferPointer = reinterpret_cast<char*>(VirtualAllocEx(GetCurrentProcess(), 0, SESSION_BUFFER_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
		//client.mRioRecvBufferPointer = reinterpret_cast<char*>(VirtualAllocEx(GetCurrentProcess(), 0, SESSION_BUFFER_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
		client.mRioBufferPointer = reinterpret_cast<char*>(VirtualAllocEx(GetCurrentProcess(), 0, SESSION_BUFFER_SIZE, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
		client.mRioSendBufferPtr = client.mRioBufferPointer + RioConfig::BUFFER_OFFSET;

		//client.mRioSendBufferId = RIO.RIORegisterBuffer(client.mRioSendBufferPointer, SESSION_BUFFER_SIZE);
		//client.mRioRecvBufferId = RIO.RIORegisterBuffer(client.mRioRecvBufferPointer, SESSION_BUFFER_SIZE);
		client.mRioBufferId = RIO.RIORegisterBuffer(client.mRioBufferPointer, SESSION_BUFFER_SIZE);

		client.mBufferMng.startPtr = client.mRioSendBufferPtr;

		if (Mode == MODE_TEST_NORMAL) {
			client.x = rand() % (BOARD_WIDTH);
			client.y = rand() % (BOARD_HEIGHT);
		}
		else {
			client.x = rand() % 10 + 20;
			client.y = rand() % 10 + 20;
		}
	}
	 
	for (int i = NPC_START; i < NUM_OF_NPC; ++i) {
		NPCList[i].x = rand() % (BOARD_WIDTH);
		NPCList[i].y = rand() % (BOARD_HEIGHT);
		NPCList[i].inUse = true;
		NPCList[i].bActive = false;
		
		lua_State* L = luaL_newstate();
		luaL_openlibs(L);
		int error = luaL_loadfile(L, "Lua/Monster.lua");
		error = lua_pcall(L, 0, 0, 0);
		lua_getglobal(L, "set_myid");
		lua_pushnumber(L, i);
		error = lua_pcall(L, 1, 0, 0);
		
		lua_pop(L, 1);
		lua_register(L, "API_sendMessage", CAPI_sendMessage);
		lua_register(L, "API_get_x", CAPI_get_x);
		lua_register(L, "API_get_y", CAPI_get_y);

		NPCList[i].L = L;
		m_Space[GetSpaceIndex(i)].insert(i);
	}
}

void Server::StartListen()
{
	Sleep(100);
	int retval = listen(Listen_Sock, SOMAXCONN);
	std::cout << "Waiting..." << std::endl;

	while (true) {
		SOCKET ClientAcceptSocket = accept(Listen_Sock, NULL, NULL);
		if (ClientAcceptSocket == INVALID_SOCKET) {
			std::cout << "Error :: Client Accept" << std::endl;
			return;
		}

		int ClientKey = -1;

		///////////////////////////////////////////////////////////////////////////////////////////////
		for (int i = 0; i < MAX_USER; ++i) {
			if (ClientArr[i].inUse == false) {
				ClientKey = i;
				break;
			}
		}

		if (-1 == ClientKey) {
			cout << "Max User Accepted" << endl;
			continue;
		}

			
		auto& client = ClientArr[ClientKey];
		client.ID = ClientKey;
		client.Client_Sock = ClientAcceptSocket;

		/// make socket non-blocking
		u_long arg = 1;
		ioctlsocket(client.Client_Sock, FIONBIO, &arg);

		/// turn off nagle
		int opt = 1;
		setsockopt(client.Client_Sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&opt, sizeof(int));

		/*
typedef RIO_RQ (PASCAL FAR * LPFN_RIOCREATEREQUESTQUEUE)(
    _In_ SOCKET Socket,
    _In_ ULONG MaxOutstandingReceive,
    _In_ ULONG MaxReceiveDataBuffers,
    _In_ ULONG MaxOutstandingSend,
    _In_ ULONG MaxSendDataBuffers,
    _In_ RIO_CQ ReceiveCQ,
    _In_ RIO_CQ SendCQ,
    _In_ PVOID SocketContext
);
		*/
		// 현재는 Recv, Send 둘다 한큐에서 받는 중
		client.mRequestQueue = RIO.RIOCreateRequestQueue(
			ClientAcceptSocket, 
			MAX_RECV_RQ_SIZE_PER_SOCKET, 
			1, 
			MAX_SEND_RQ_SIZE_PER_SOCKET, 
			1,
			mRioCompletionQueue[client.ID % MAX_RIO_THREAD],
			mRioCompletionQueue[client.ID % MAX_RIO_THREAD],
			NULL);
		if (client.mRequestQueue == RIO_INVALID_RQ)
		{
			printf_s("[DEBUG] RIOCreateRequestQueue Error: %d\n", GetLastError());
			while (true);
		}

		DWORD flag = 0;

		client.inUse = true;

		client.PostRecv();

		sc_packet_loginok p;
		p.size = sizeof(sc_packet_loginok);
		p.type = SC_LOGINOK;
		p.id = client.ID;
		p.x = client.x;
		p.y = client.y;
		p.level = 0;
		p.hp = 0;
		p.exp = 0;
		SendPacket(client.ID, &p);
		CreateConnection(client.ID);
		//////////////////////////////////////////////////////////////////////////////////////////////
	}
}

void Server::CloseServer()
{
	for (auto& t : WorkerThreads)
		t.join();
	TimerThread.join();

	closesocket(Listen_Sock);

	WSACleanup();
}

void Server::SendPacket(int clientkey, void * packet)
{
	ClientArr[clientkey].PostSend(packet);
}

void Server::SendPutObject(int client, int objid)
{
	sc_packet_put_player p;
	p.id = objid;
	p.size = sizeof(p);
	p.type = SC_PUT_PLAYER;

	CNPC* obj = nullptr;
	if (objid >= NPC_START)		obj = &NPCList[objid];
	else if (objid < NPC_START)	obj = &ClientArr[objid];
	p.x = obj->x;
	p.y = obj->y;

	SendPacket(client, &p);
}

void Server::SendRemoveObject(int client, int objid)
{
	sc_packet_remove_player p;
	p.id = objid;
	p.size = sizeof(p);
	p.type = SC_REMOVE_PLAYER;

	SendPacket(client, &p);
}

void Server::SendChatPacket(int to, int from, WCHAR * message)
{
	sc_packet_chat p;
	p.id = from;
	p.size = sizeof(p);
	p.type = SC_CHAT;
	wcsncpy_s(p.message, message, MAX_STR_SIZE);

	SendPacket(to, &p);
}

bool Server::ChkInSpace(int clientid, int targetid)
{
	int clientspaceid = GetSpaceIndex(clientid);
	int targetspaceid = GetSpaceIndex(targetid);
	if ((clientspaceid + SPACE_X - 1	<= targetspaceid)	&& (targetspaceid <= clientspaceid + SPACE_X + 1) ||
		(clientspaceid - 1				<= targetspaceid)	&& (targetspaceid <= clientspaceid + 1) ||
		(clientspaceid - SPACE_X - 1	<= targetspaceid)	&& (targetspaceid <= clientspaceid - SPACE_X + 1))
		return true;

	return false;
}

void Server::WorkThreadProcess(int threadinput)
{
	thread_local int threadid = threadinput;
	//mRioSendCompletionQueue[threadid] = RIO.RIOCreateCompletionQueue(MAX_CQ_SIZE_PER_RIO_THREAD, 0);
	//mRioRecvCompletionQueue[threadid] = RIO.RIOCreateCompletionQueue(MAX_CQ_SIZE_PER_RIO_THREAD, 0);
	mRioCompletionQueue[threadid] = RIO.RIOCreateCompletionQueue(MAX_CQ_SIZE_PER_RIO_THREAD, 0);
	if (mRioCompletionQueue[threadid] == RIO_INVALID_CQ)
	{
		CRASH_ASSERT(false);
		while (true);
	}
	RIORESULT sendresults[MAX_RIO_RESULT];
	RIORESULT recvresults[MAX_RIO_RESULT];
	
	while (1)
	{
		memset(sendresults, 0, sizeof(sendresults));
		memset(recvresults, 0, sizeof(recvresults));

		// Dequeue를 하는 시점에 버퍼 락이 걸리는가? Lock이 안걸린다면 계속 쌓이는 데이터는 어떻게 해소할 것 인가?
		//ULONG numSendResults = RIO.RIODequeueCompletion(mRioSendCompletionQueue[threadid], sendresults, MAX_RIO_RESULT);
		//ULONG numRecvResults = RIO.RIODequeueCompletion(mRioRecvCompletionQueue[threadid], recvresults, MAX_RIO_RESULT);
		ULONG numResults = RIO.RIODequeueCompletion(mRioCompletionQueue[threadid], recvresults, MAX_RIO_RESULT);
		if (0 == numResults)
		{
			Sleep(1); ///< for low cpu-usage
			continue;
		}
		else if (RIO_CORRUPT_CQ == numResults)
		{
			printf_s("[DEBUG] RIO CORRUPT CQ \n");
			while (true);
		}

		for (ULONG i = 0; i < numResults; ++i)
		{
			RioIoContext* context = reinterpret_cast<RioIoContext*>(recvresults[i].RequestContext);
			ULONG transferred = recvresults[i].BytesTransferred;

			if (transferred == 0 && context->mClientSession->ID < NPC_START && (context->mIoType == op_Recv || context->mIoType == op_Send))
			{
				DisConnectClient(context->mClientSession->ID);
				continue;
			}

			switch (context->mIoType) 
			{
			case enumOperation::op_Recv:
			{
				CClient* client = context->mClientSession;
				
				int r_size = transferred;
				char* ptr = client->mRioBufferPointer;

				while (0 < r_size)
				{
					if (client->packetsize == 0)
						client->packetsize = ptr[0];

					int remainsize = client->packetsize - client->prev_packetsize;

					if (remainsize <= r_size) {

						memcpy(client->prev_packet + client->prev_packetsize, ptr, remainsize);

						if (m_pScene == nullptr) {
							cout << "null scene" << endl;
						}
						m_pScene->ProcessPacket(client->ID, client->prev_packet);

						r_size -= remainsize;
						ptr += remainsize;
						client->packetsize = 0;
						client->prev_packetsize = 0;
					}
					else {
						memcpy(client->prev_packet + client->prev_packetsize, ptr, r_size);
						client->prev_packetsize += r_size;
					}

				}

				client->PostRecv();
				break;
			}
			case enumOperation::op_Send:
			{
				auto& client = context->mClientSession;
				client->mBufferMng.ReleaseUsedBufferPiece(context->mSendBufIdx);
				break;
			}

			default:
				break;
			}

			delete context;
		}
	}
}

void Server::CreateConnection(UINT ClientKey)
{
	sc_packet_put_player p;
	p.id = ClientKey;
	p.size = sizeof(sc_packet_put_player);
	p.type = SC_PUT_PLAYER;

	p.x = ClientArr[ClientKey].x;
	p.y = ClientArr[ClientKey].y;


	// to all players
	for (int i = 0; i < MAX_USER; ++i)
	{
		if (true == ClientArr[i].inUse) {
			if (!CanSee(i, ClientKey))		continue;

			ClientArr[i].viewlist_mutex.lock();
			ClientArr[i].viewlist.insert(ClientKey);
			ClientArr[i].viewlist_mutex.unlock();
			SendPacket(i, &p);
		}
	}
	// to me
	for (int i = 0; i < MAX_USER; ++i)
	{
		if (i != ClientKey && true == ClientArr[i].inUse)
		{
			if (!CanSee(ClientKey, i))		continue;

			ClientArr[ClientKey].viewlist_mutex.lock();
			ClientArr[ClientKey].viewlist.insert(i);
			ClientArr[ClientKey].viewlist_mutex.unlock();

			p.id = i;
			p.x = ClientArr[i].x;
			p.y = ClientArr[i].y;

			SendPacket(ClientKey, &p);
		}
	}

	for (int i = NPC_START; i < NUM_OF_NPC; ++i)
	{
		if (!ChkInSpace(ClientKey, i))	continue;
		if (!CanSee(ClientKey, i))		continue;

		ClientArr[ClientKey].viewlist_mutex.lock();
		ClientArr[ClientKey].viewlist.insert(i);
		ClientArr[ClientKey].viewlist_mutex.unlock();

		if (NPCList[i].bActive == false) {
			NPCList[i].bActive = true;
			AddTimerEvent(i, enumOperation::op_Move, MOVE_TIME);
		}

		p.id = i;
		p.x = NPCList[i].x;
		p.y = NPCList[i].y;

		SendPacket(ClientKey, &p);
	}

	m_SpaceMutex[GetSpaceIndex(ClientKey)].lock();
	m_Space[GetSpaceIndex(ClientKey)].insert(ClientKey);
	m_SpaceMutex[GetSpaceIndex(ClientKey)].unlock();
}

void Server::TimerThreadProcess()
{
	while (true) {
		Sleep(1);
		while (true) {
			TimerEventMutex.lock();
			if (TimerEventQueue.empty()) {
				TimerEventMutex.unlock();
				break;
			}
			
			sEvent e = TimerEventQueue.top();
			if (e.startTime > GetTime()) {
				TimerEventMutex.unlock();
				break;
			}
			
			TimerEventQueue.pop();
			TimerEventMutex.unlock();

			if (e.operation == enumOperation::op_Move)
			{
				//stOverlappedEx* o = new stOverlappedEx();
				//o->eOperation = e.operation;
				//
				//PostQueuedCompletionStatus(h_IOCP, 0, e.id, reinterpret_cast<LPOVERLAPPED>(o));
			}
		}
	}
}

void Server::AddTimerEvent(UINT id, enumOperation op, long long time)
{
	sEvent e;
	e.id = id;
	e.operation = op;
	e.startTime = GetTime() + time;

	TimerEventMutex.lock();
	TimerEventQueue.push(e);
	TimerEventMutex.unlock();
}

long long Server::GetTime()
{
	const long long _Freq = _Query_perf_frequency();	// doesn't change after system boot
	const long long _Ctr = _Query_perf_counter();
	const long long _Whole = (_Ctr / _Freq) * 1000;
	const long long _Part = (_Ctr % _Freq) * 1000 / _Freq;
	return _Whole + _Part;
}

bool Server::CanSee(int a, int b)
{
	CNPC *oa = nullptr;
	CNPC *ob = nullptr;
	if (a < NPC_START)			oa = &ClientArr[a];
	else if (a >= NPC_START)	oa = &NPCList[a];
	if (b < NPC_START)			ob = &ClientArr[b];
	else if (b >= NPC_START)	ob = &NPCList[b];

	int distance =
		(oa->x - ob->x) * (oa->x - ob->x) +
		(oa->y - ob->y) * (oa->y - ob->y);

	return distance <= VIEW_RADIUS * VIEW_RADIUS;
}

void Server::DisConnectClient(int key)
{
	closesocket(ClientArr[key].Client_Sock);

	sc_packet_remove_player p;
	p.id = key;
	p.size = sizeof(sc_packet_remove_player);
	p.type = SC_REMOVE_PLAYER;

	ClientArr[key].viewlist_mutex.lock();
	std::unordered_set<int> oldviewlist = ClientArr[key].viewlist;
	ClientArr[key].viewlist.clear();
	ClientArr[key].viewlist_mutex.unlock();

	for (int id : oldviewlist)
	{
		if (id >= NPC_START) continue;

		ClientArr[id].viewlist_mutex.lock();
		if (ClientArr[id].inUse == true)
		{
			if (ClientArr[id].viewlist.count(key) != 0) {
				ClientArr[id].viewlist.erase(key);
				ClientArr[id].viewlist_mutex.unlock();

				SendPacket(id, &p);
			}
			else
				ClientArr[id].viewlist_mutex.unlock();
		}
		else
			ClientArr[id].viewlist_mutex.unlock();
	}

	int spaceIdx = GetSpaceIndex(key);
	m_SpaceMutex[spaceIdx].lock();
	m_Space[spaceIdx].erase(key);
	m_SpaceMutex[spaceIdx].unlock();
	m_pScene->RemovePlayerOnBoard(ClientArr[key].x, ClientArr[key].y);

	std::cout << "Client " << ClientArr[key].ID << " Disconnected" << std::endl;
	ClientArr[key].inUse = false;
	ClientArr[key].bActive = false;
}

void Server::MoveNPC(int key)
{
	////////////////////////////////////////////////////////////////////////////////

	if (NPCList[key].bActive == false)
		return;

	int oldSpaceIdx = GetSpaceIndex(key);

	switch (rand() % 4) {
	case 0:
		if (NPCList[key].x == BOARD_WIDTH - 1)	break;
		NPCList[key].x++;					break;
	case 1:
		if (NPCList[key].x == 0)			break;
		NPCList[key].x--;					break;
	case 2:
		if (NPCList[key].y == BOARD_HEIGHT - 1)	break;
		NPCList[key].y++;					break;
	case 3:
		if (NPCList[key].y == 0)				break;
		NPCList[key].y--;					break;
	}
	int NewSpaceIdx = GetSpaceIndex(key);
	if (oldSpaceIdx != NewSpaceIdx) {
		m_SpaceMutex[oldSpaceIdx].lock();
		m_Space[oldSpaceIdx].erase(key);
		m_SpaceMutex[oldSpaceIdx].unlock();

		m_SpaceMutex[NewSpaceIdx].lock();
		m_Space[NewSpaceIdx].insert(key);
		m_SpaceMutex[NewSpaceIdx].unlock();
	}

	std::unordered_set<int> new_view_list;
	for (int playerid = 0; playerid < MAX_USER; ++playerid) {
		if (ClientArr[playerid].inUse == false) continue;
		if (CanSee(key, playerid) == false) {
			ClientArr[playerid].viewlist_mutex.lock();
			if (ClientArr[playerid].viewlist.count(key) != 0) {
				ClientArr[playerid].viewlist.erase(key);
				ClientArr[playerid].viewlist_mutex.unlock();
				SendRemoveObject(playerid, key);
			}
			else
				ClientArr[playerid].viewlist_mutex.unlock();

			continue;
		}
		new_view_list.insert(playerid);
	}

	// NPC가 볼 수 있는 Player
	for (auto id : new_view_list) {
		ClientArr[id].viewlist_mutex.lock();
		if (ClientArr[id].viewlist.count(key) == 0) {
			ClientArr[id].viewlist.insert(key);
			ClientArr[id].viewlist_mutex.unlock();

			SendPutObject(id, key);
		}
		else {
			sc_packet_pos sp;
			sp.id = key;
			sp.size = sizeof(sc_packet_pos);
			sp.type = SC_POS;
			sp.x = NPCList[key].x;
			sp.y = NPCList[key].y;
			ClientArr[id].viewlist_mutex.unlock();

			SendPacket(id, &sp);
		}
	}

	if (NPCList[key].bActive == false)
		return;

	AddTimerEvent(key, op_Move, MOVE_TIME);
}


////////////////////////////////////////////////////////////////////////////////////////////////


bool CClient::PostSend(void* packet)
{
	unsigned char* p = reinterpret_cast<unsigned char*>(packet);
	char* PiecePtr = nullptr;
	int PieceIdx = -1;

	int packetlen = p[0];
	if (packetlen > BUFPIECESIZE)
		p[-1] = 0;

	PieceIdx = mBufferMng.GetFreeBufferPieceIdx(PiecePtr);
	memcpy(PiecePtr, packet, packetlen);

	RioIoContext* sendContext = new RioIoContext(this, enumOperation::op_Send);

	sendContext->BufferId = mRioBufferId;
	sendContext->Length = packetlen;
	sendContext->Offset = RioConfig::BUFFER_OFFSET + PieceIdx * BUFPIECESIZE;
	sendContext->mSendBufIdx = PieceIdx;

	DWORD sendbytes = 0;
	DWORD flags = 0;

	mRioRQMutex.lock();
	/// start async send
	if (!RIO.RIOSend(mRequestQueue, (PRIO_BUF)sendContext, 1, flags, sendContext))
	{
		printf_s("[DEBUG] RIOSend error: %d\n", GetLastError());
		return false;
	}
	mRioRQMutex.unlock();

	//cout << "send on" << endl;
	return true;
}

bool CClient::PostRecv()
{
	RioIoContext* recvContext = new RioIoContext(this, enumOperation::op_Recv);

	recvContext->BufferId = mRioBufferId;
	recvContext->Length = RioConfig::SESSION_BUFFER_SIZE / 2 ;
	recvContext->Offset = 0;

	DWORD recvbytes = 0;
	DWORD flags = 0;

	mRioRQMutex.lock();
	/// start async recv
	if (!RIO.RIOReceive(mRequestQueue, (PRIO_BUF)recvContext, 1, flags, recvContext))
	{
		printf_s("[DEBUG] RIOReceive error: %d\n", GetLastError());
		return false;
	}
	mRioRQMutex.unlock();
	//cout << "recv on" << endl;
	return true;
}

CClient::~CClient()
{
	//RIO.RIODeregisterBuffer(mRioRecvBufferId);
	//RIO.RIODeregisterBuffer(mRioSendBufferId);
	RIO.RIODeregisterBuffer(mRioBufferId);
	//VirtualFreeEx(GetCurrentProcess(), mRioSendBufferPointer, 0, MEM_RELEASE);
	//VirtualFreeEx(GetCurrentProcess(), mRioRecvBufferPointer, 0, MEM_RELEASE);
	VirtualFreeEx(GetCurrentProcess(), mRioBufferPointer, 0, MEM_RELEASE);
}
