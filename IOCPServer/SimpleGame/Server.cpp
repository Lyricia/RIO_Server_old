#include "stdafx.h"
#include "Scene.h"
#include "Server.h"

Server* Server_Instance;

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

	Listen_Sock = WSASocketW(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
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

	InitObjectList();
	InitDB();

	// ThreadPool
	for (UINT i = 0; i < std::thread::hardware_concurrency(); i++)
	{
		WorkerThreads.emplace_back(std::thread{ [this]() { WorkThreadProcess(); } });
	}

	TimerThread = thread{ [this]() { TimerThreadProcess(); } };

	DBThread = thread{ [this]() { DBThreadProcess(); } };

	std::cout << "Server Initiated" << std::endl;
}

void Server::InitDB()
{
	SQLRETURN retcode;
	retcode = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &h_env);
	retcode = SQLSetEnvAttr(h_env, SQL_ATTR_ODBC_VERSION, (void*)SQL_OV_ODBC3, 0);
	if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
		retcode = SQLAllocHandle(SQL_HANDLE_DBC, h_env, &h_dbc);

		if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
			SQLSetConnectAttr(h_dbc, SQL_LOGIN_TIMEOUT, (SQLPOINTER)5, 0);

			retcode = SQLConnectW(h_dbc, (SQLWCHAR*)L"2018_ODBC", SQL_NTS, (SQLWCHAR*)NULL, 0, NULL, 0);

			if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
				retcode = SQLAllocHandle(SQL_HANDLE_STMT, h_dbc, &h_stmt);

				DoTest();
			}
		}
	}

	cout << "Database Connected\n";
}

void Server::InitObjectList()
{
	for (auto & client : ClientArr) {
		client.OverlappedEx.eOperation = op_Recv;
		client.OverlappedEx.wsaBuf.buf = client.OverlappedEx.io_Buf;
		client.OverlappedEx.wsaBuf.len = sizeof(client.OverlappedEx.io_Buf);
		client.viewlist.clear();
		client.inUse = false;
		client.packetsize = 0;
		client.prev_packetsize = 0;
		client.ID = -1;
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
		NPCList[i].y = rand() % (BOARD_HEIGHT );
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

#pragma optimize( "", off )
void Server::StartListen()
{
	Sleep(3000);
	int retval = listen(Listen_Sock, SOMAXCONN);
	std::cout << "Waiting..." << std::endl;

	while (1) {
		SOCKADDR_IN ClientAddr;
		ZeroMemory(&ClientAddr, sizeof(SOCKADDR_IN));
		ClientAddr.sin_family = AF_INET;
		ClientAddr.sin_port = htons(SERVERPORT);
		ClientAddr.sin_addr.s_addr = INADDR_ANY;
		int addrlen = sizeof(SOCKADDR_IN);

		SOCKET ClientAcceptSocket = WSAAccept(Listen_Sock, (SOCKADDR*)&ClientAddr, &addrlen, NULL, NULL);
		if (ClientAcceptSocket == INVALID_SOCKET) {
			std::cout << "Error :: Client Accept" << std::endl;
			return;
		}
		else {
			//std::cout << "Client Accepted" << std::endl;
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

		ClientArr[ClientKey].Client_Sock = ClientAcceptSocket;
		ZeroMemory(&ClientArr[ClientKey].OverlappedEx.wsaOverlapped, sizeof(WSAOVERLAPPED));


		CreateIoCompletionPort(
			reinterpret_cast<HANDLE*>(ClientAcceptSocket),
			h_IOCP,
			ClientKey,
			0
		);

		DWORD flag = 0;

		ClientArr[ClientKey].inUse = true;

		WSARecv(
			ClientArr[ClientKey].Client_Sock,
			&ClientArr[ClientKey].OverlappedEx.wsaBuf,
			1,
			NULL,
			&flag,
			&ClientArr[ClientKey].OverlappedEx.wsaOverlapped,
			NULL
		);
	
		//////////////////////////////////////////////////////////////////////////////////////////////
	}
}
#pragma optimize( "", on ) 

void Server::CloseServer()
{
	for (auto& t : WorkerThreads)
		t.join();
	TimerThread.join();
	DBThread.join();

	SQLFreeHandle(SQL_HANDLE_STMT, h_stmt);
	SQLDisconnect(h_dbc);
	SQLFreeHandle(SQL_HANDLE_DBC, h_dbc);
	SQLFreeHandle(SQL_HANDLE_ENV, h_env);

	closesocket(Listen_Sock);

	WSACleanup();
}

void Server::SendPacket(int clientkey, void * packet)
{
	stOverlappedEx* o = new stOverlappedEx();
	unsigned char* p = reinterpret_cast<unsigned char*>(packet);
	memcpy(o->io_Buf, packet, p[0]);
	o->eOperation = op_Send;
	o->wsaBuf.buf = o->io_Buf;
	o->wsaBuf.len = p[0];
	ZeroMemory(&o->wsaOverlapped, sizeof(WSAOVERLAPPED));

	WSASend(ClientArr[clientkey].Client_Sock, &o->wsaBuf, 1, NULL, 0, &o->wsaOverlapped, NULL);
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

void Server::WorkThreadProcess()
{
	unsigned long datasize;
	unsigned long long key;
	stOverlappedEx* oEx;

	while (1)
	{
		bool isSuccess = GetQueuedCompletionStatus(
			h_IOCP,
			&datasize,
			&key,
			reinterpret_cast<LPOVERLAPPED*>(&oEx),
			INFINITE
		);

		if (isSuccess == 0)
		{
			DisConnectClient((int)key);
			continue;
		}
		else if (datasize == 0 && key < NPC_START && (oEx->eOperation == op_Recv || oEx->eOperation == op_Send))
		{
			DisConnectClient((int)key);
			continue;
		}

		switch (oEx->eOperation) {
		case enumOperation::op_Recv:
		{
			int r_size = datasize;
			char* ptr = oEx->io_Buf;

			while (0 < r_size)
			{
				if (ClientArr[key].packetsize == 0)
					ClientArr[key].packetsize = ptr[0];

				int remainsize = ClientArr[key].packetsize - ClientArr[key].prev_packetsize;

				if (remainsize <= r_size) {

					memcpy(ClientArr[key].prev_packet + ClientArr[key].prev_packetsize, ptr, remainsize);

					m_pScene->ProcessPacket((int)key, ClientArr[key].prev_packet);

					r_size -= remainsize;
					ptr += remainsize;
					ClientArr[key].packetsize = 0;
					ClientArr[key].prev_packetsize = 0;
				}
				else {
					memcpy(ClientArr[key].prev_packet + ClientArr[key].prev_packetsize, ptr, r_size);
					ClientArr[key].prev_packetsize += r_size;
				}

			}
			unsigned long rFlag = 0;
			ZeroMemory(&oEx->wsaOverlapped, sizeof(OVERLAPPED));
			WSARecv(ClientArr[key].Client_Sock, &oEx->wsaBuf, 1, NULL, &rFlag, &oEx->wsaOverlapped, NULL);
			break;
		}
		case enumOperation::op_Move:
		{
			if(NPCList[key].bActive == true)
				MoveNPC((int)key);
			delete oEx;
			break;
		}

		case enumOperation::db_login:
		{
			DBUserData* data = (DBUserData*)oEx->io_Buf;
			if (data->nID != -1) {
				sc_packet_loginok p;
				p.size = sizeof(sc_packet_loginok);
				p.type = SC_LOGINOK;
				p.id = data->nID;
				p.x = data->nPosX;
				p.y = data->nPosY;
				p.level = data->nCHAR_LEVEL;
				p.hp = data->nHP;
				p.exp = data->nExp;
				SendPacket(data->Key, &p);

				ClientArr[data->Key].ID = data->nID;
				ClientArr[data->Key].x = data->nPosX;
				ClientArr[data->Key].y = data->nPosY;
				ClientArr[data->Key].bActive = true;
			}
			else {
				sc_packet_loginfail p;
				p.size = sizeof(sc_packet_loginfail);
				p.type = SC_LOGINFAIL;
				SendPacket(data->Key, &p);
				break;
			}
			
			CreateConnection(data->Key);

			cout << "Client " << ClientArr[data->Key].ID << " Connected\n";
			delete oEx;
			break;
		}
		case enumOperation::db_logout:
		{
			delete oEx;
			break;
		}

		case enumOperation::npc_player_move:
		{
			if (Mode == MODE_NORMAL) {
				int player = oEx->EventTarget;
				lua_getglobal(NPCList[key].L, "event_player_move");
				lua_pushnumber(NPCList[key].L, player);
				lua_pcall(NPCList[key].L, 1, 0, 0);
			}
			delete oEx;
			break;
		}

		default:
			delete oEx;
			break;
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
				stOverlappedEx* o = new stOverlappedEx();
				o->eOperation = e.operation;
				
				PostQueuedCompletionStatus(h_IOCP, 0, e.id, reinterpret_cast<LPOVERLAPPED>(o));
			}
		}
	}
}

void Server::DBThreadProcess()
{
	while (true) {
		Sleep(1);
		while (true) {
			DBEventMutex.lock();
			if (DBEventQueue.empty()) {
				DBEventMutex.unlock();
				break;
			}

			sEvent e = DBEventQueue.front();

			DBEventQueue.pop();
			DBEventMutex.unlock();

			if (e.operation == enumOperation::db_login)
			{
				stOverlappedEx* o = new stOverlappedEx();
				o->eOperation = e.operation;
				DBUserData* data = (DBUserData*)o->io_Buf;
				data->Key = e.IOCPKey;
				SQLLEN cbID = 0, cbCHAR_LEVEL = 0, cbPosX = 0, cbPosY = 0, cbhp = 0, cbexp = 0;

				SQLRETURN retcode = SQLAllocHandle(SQL_HANDLE_STMT, h_dbc, &h_stmt);
				retcode = SQLExecDirectW(h_stmt, (SQLWCHAR *)(L"EXEC LoginProc " + std::to_wstring(e.id)).c_str(), SQL_NTS);
				if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
					retcode = SQLBindCol(h_stmt, 1, SQL_C_LONG, &data->nID, sizeof(SQLINTEGER), &cbID);
					retcode = SQLBindCol(h_stmt, 2, SQL_C_LONG, &data->nCHAR_LEVEL, sizeof(SQLINTEGER), &cbCHAR_LEVEL);
					retcode = SQLBindCol(h_stmt, 3, SQL_C_LONG, &data->nPosX, sizeof(SQLINTEGER), &cbPosX);
					retcode = SQLBindCol(h_stmt, 4, SQL_C_LONG, &data->nPosY, sizeof(SQLINTEGER), &cbPosY);
					retcode = SQLBindCol(h_stmt, 5, SQL_C_LONG, &data->nHP, sizeof(SQLINTEGER), &cbhp);
					retcode = SQLBindCol(h_stmt, 6, SQL_C_LONG, &data->nExp, sizeof(SQLINTEGER), &cbexp);

					retcode = SQLFetch(h_stmt);
					if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) {
						data->nID = -1;
						cout << "Error2 " << retcode << " \n";
					}
					if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {
						wprintf(L"%d\t %d\t %d\t %d\n", data->nID, data->nCHAR_LEVEL, data->nPosX, data->nPosY);
					}
					else {
						data->nID = -1;
						cout << "NO DB. LogIn Failed\n";
					}
				}
				else {
					HandleDiagnosticRecord(h_stmt, SQL_HANDLE_STMT, retcode);
				}

				SQLCancel(h_stmt);
				PostQueuedCompletionStatus(h_IOCP, 0, e.id, reinterpret_cast<LPOVERLAPPED>(o));
			}
			else if (e.operation == enumOperation::db_logout)
			{
				stOverlappedEx* o = new stOverlappedEx();
				o->eOperation = e.operation;
				DBUserData* cl = (DBUserData*)e.data;

				SQLRETURN retcode = SQLAllocHandle(SQL_HANDLE_STMT, h_dbc, &h_stmt);

				std::wstring query = 
					L"EXEC LogoutProc " + 
					std::to_wstring(cl->nID) + L", " +
					std::to_wstring(cl->nPosX) + L", " +
					std::to_wstring(cl->nPosY);
				
				retcode = SQLExecDirectW(h_stmt, (SQLWCHAR *)query.c_str(), SQL_NTS);
				cout << "Client " << cl->nID << " LogOut\n";

				SQLCancel(h_stmt);
				PostQueuedCompletionStatus(h_IOCP, 0, e.id, reinterpret_cast<LPOVERLAPPED>(o));

				delete e.data;
			}
		}
	}

}

void Server::DoTest()
{
	SQLINTEGER nID, nCHAR_LEVEL, nPosX, nPosY;
	SQLLEN cbName = 0, cbID = 0, cbCHAR_LEVEL = 0, cbPosX = 0, cbPosY = 0;
	
	SQLRETURN retcode = SQLExecDirectW(h_stmt, (SQLWCHAR *)(L"EXEC loginProc " + std::to_wstring(1)).c_str(), SQL_NTS);
	if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO) {

		// Bind columns 1, 2, and 3  
		retcode = SQLBindCol(h_stmt, 1, SQL_C_LONG, &nID, 100, &cbID);
		retcode = SQLBindCol(h_stmt, 2, SQL_C_LONG, &nCHAR_LEVEL, 100, &cbCHAR_LEVEL);
		retcode = SQLBindCol(h_stmt, 3, SQL_C_LONG, &nPosX, 100, &cbPosX);
		retcode = SQLBindCol(h_stmt, 4, SQL_C_LONG, &nPosY, 100, &cbPosY);

		// Fetch and print each row of data. On an error, display a message and exit.  
		for (int i = 0; ; i++) {
			retcode = SQLFetch(h_stmt);
			if (retcode == SQL_ERROR || retcode == SQL_SUCCESS_WITH_INFO) {
				cout << "Error2 " << retcode << " \n";
			}
			if (retcode == SQL_SUCCESS || retcode == SQL_SUCCESS_WITH_INFO)
				wprintf(L"%d\t: %d\t %d\t %d\t %d\n", i + 1, nID, nCHAR_LEVEL, nPosX, nPosY);
			else {
				cout << "DB End\n";
				break;
			}
		}
	}
	else {
		HandleDiagnosticRecord(h_stmt, SQL_HANDLE_STMT, retcode);
	}
	SQLCancel(h_stmt);
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

void Server::AddDBEvent(UINT IOCPKey, UINT id, enumOperation op)
{
	sEvent e;
	e.id = id;
	e.operation = op;
	e.IOCPKey = IOCPKey;
	DBUserData* data = new DBUserData();

	data->nPosX = ClientArr[IOCPKey].x;
	data->nPosY = ClientArr[IOCPKey].y;
	data->nID = ClientArr[IOCPKey].ID;
	e.data = data;

	DBEventMutex.lock();
	DBEventQueue.push(e);
	DBEventMutex.unlock();
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

	if (ClientArr[key].bActive)
		AddDBEvent(key, ClientArr[key].ID, db_logout);
	else
		return;

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