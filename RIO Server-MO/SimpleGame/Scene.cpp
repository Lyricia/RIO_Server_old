#include "stdafx.h"
#include "Server.h"
#include "Timer.h"
#include "Object.h"
#include "Scene.h"

using namespace std;

Scene::Scene()
{
}

Scene::~Scene()
{
}

void Scene::buildScene()
{
	memset(m_Board, INVALID, sizeof(int) * BOARD_WIDTH * BOARD_HEIGHT);
	m_pClientArr = m_Server->GetClientArr();
	//m_pNPCList = m_Server->GetNPClist();

	GameStatus = GAMESTATUS::RUNNING;
}

bool Scene::isCollide(int x, int y)
{
	if (x < 0 || y < 0 || x > BOARD_WIDTH || y > BOARD_HEIGHT) return false;
	if (m_Board[x][y] == INVALID)
		return false;
	else 
		return true;
}

void Scene::releaseScene()
{
	delete		g_Timer;
}

void Scene::ProcessPacket(int clientid, unsigned char * packet)
{
	int type = packet[1];
	CClient& client = m_Server->GetClient(clientid);
	int x = client.x, y = client.y;

	int oldSpaceIdx = m_Server->GetSpaceIndex(clientid);

	switch (type)
	{
	case CS_UP:
		client.y--;
		if (isCollide(client.x, client.y)) {
			client.y++; break;
		}
		else
			m_Board[x][y] = INVALID;

		if (0 > client.y)
			client.y = 0;
		break;

	case CS_DOWN:
		client.y++;
		if (isCollide(client.x, client.y)) {
			client.y--; break;
		}
		else
			m_Board[x][y] = INVALID;

		if (BOARD_HEIGHT <= client.y)
			client.y = BOARD_HEIGHT - 1;
		break;

	case CS_RIGHT:
		client.x++;
		if (isCollide(client.x, client.y)) {
			client.x--; break;
		}
		else
			m_Board[x][y] = INVALID;
		if (BOARD_WIDTH <= client.x)
			client.x = BOARD_WIDTH - 1;
		break;

	case CS_LEFT:
		client.x--;
		if (isCollide(client.x, client.y)) {
			client.x++; break;
		}
		else
			m_Board[x][y] = INVALID;

		if (0 > client.x)
			client.x = 0;
		break;

	default:
		cout << "unknown protocol from client [" << clientid << "]" << endl;
		return;
	}
	
	MoveObject(clientid, oldSpaceIdx);
}

void Scene::MoveObject(int clientid, int oldSpaceIdx)
{
	CClient& client = m_Server->GetClient(clientid);
	int roomidx = client.RoomIndex;

	//int newSpaceIdx = m_Server->GetSpaceIndex(clientid);
	//if (oldSpaceIdx != newSpaceIdx) {
	//	m_Server->GetSpaceMutex(roomidx, oldSpaceIdx).lock();
	//	m_Server->GetSpace(roomidx, oldSpaceIdx).erase(clientid);
	//	m_Server->GetSpaceMutex(roomidx, oldSpaceIdx).unlock();
	//
	//	m_Server->GetSpaceMutex(roomidx, newSpaceIdx).lock();
	//	m_Server->GetSpace(roomidx, newSpaceIdx).insert(clientid);
	//	m_Server->GetSpaceMutex(roomidx, newSpaceIdx).unlock();
	//}

	m_Board[client.x][client.y] = clientid;

	sc_packet_pos sp;
	sp.id = clientid;
	sp.size = sizeof(sc_packet_pos);
	sp.type = SC_POS;
	sp.x = client.x;
	sp.y = client.y;

	// 새로 viewList에 들어오는 객체 처리
	unordered_set<int> new_view_list;
	for (int i = 0; i < MAX_USER; ++i) {
		if (i == clientid) continue;
		if (m_pClientArr[i].inUse == false) continue;
		//if (m_Server->CanSee(clientid, i) == false) continue;
		new_view_list.insert(i);
	}

	//int idx = 0;
	//for (int i = -1; i <= 1; ++i) {
	//	for (int j = -1; j <= 1; ++j) {
	//		idx = newSpaceIdx + i + (j * SPACE_X);
	//		if (idx < 0 || idx >= SPACE_X * SPACE_Y) continue;
	//
	//		m_Server->GetSpaceMutex(roomidx, idx).lock();
	//		auto& Space_objlist = m_Server->GetSpace(roomidx, idx);
	//		m_Server->GetSpaceMutex(roomidx, idx).unlock();
	//
	//		for (auto objidx : Space_objlist)
	//		{
	//			if (objidx == clientid) continue;
	//			if (objidx < NPC_START && m_pClientArr[objidx].inUse == false) continue;
	//			if (m_Server->CanSee(clientid, objidx) == false) continue;
	//			// 시야 내에 있는 플레이어가 이동했다는 이벤트 발생
	//			if (objidx >= NPC_START) {
	//				//RIO.RIONotify(m_Server->mRioCompletionQueue[]);
	//				//stOverlappedEx *e = new stOverlappedEx();
	//				//e->eOperation = npc_player_move;
	//				//e->EventTarget = clientid;
	//				//PostQueuedCompletionStatus(m_Server->GetIOCP(), 1, objidx, &e->wsaOverlapped);
	//			}
	//			new_view_list.insert(objidx);
	//		}
	//	}
	//}

	// viewList에 계속 남아있는 객체 처리
	for (auto id : new_view_list) {
		m_pClientArr[clientid].viewlist_mutex.lock();
		if (m_pClientArr[clientid].viewlist.count(id) == 0) {
			m_pClientArr[clientid].viewlist.insert(id);
			m_pClientArr[clientid].viewlist_mutex.unlock();

			m_Server->SendPutObject(clientid, id);
		}
		else {
			m_pClientArr[clientid].viewlist_mutex.unlock();
		}

		if (id >= NPC_START)
		{
			//if (m_pNPCList[id].bActive == false) {
			//	m_pNPCList[id].bActive = true;
			//	m_Server->AddTimerEvent(id, enumOperation::op_Move, MOVE_TIME);
			//}
		}
		else
		{
			m_pClientArr[id].viewlist_mutex.lock();
			if (m_pClientArr[id].viewlist.count(clientid) == 0) {
				m_pClientArr[id].viewlist.insert(clientid);
				m_pClientArr[id].viewlist_mutex.unlock();

				m_Server->SendPutObject(id, clientid);
			}
			else {
				m_pClientArr[id].viewlist_mutex.unlock();
				m_Server->SendPacket(id, &sp);
			}
		}
	}

	// 빠져나간 객체
	m_pClientArr[clientid].viewlist_mutex.lock();
	unordered_set<int> oldviewlist = m_pClientArr[clientid].viewlist;
	m_pClientArr[clientid].viewlist_mutex.unlock();
	for (auto id : oldviewlist) {
		if (clientid == id)
			continue;
		if (new_view_list.count(id) == 0) {
			m_pClientArr[clientid].viewlist_mutex.lock();
			m_pClientArr[clientid].viewlist.erase(id);
			m_pClientArr[clientid].viewlist_mutex.unlock();

			m_Server->SendRemoveObject(clientid, id);

			if (id >= NPC_START) 
			{
				//if (m_pNPCList[id].bActive == true) {
				//	m_pNPCList[id].bActive = false;
				//}
			}
			else
			{
				m_pClientArr[id].viewlist_mutex.lock();
				if (m_pClientArr[id].viewlist.count(clientid) != 0) {
					m_pClientArr[id].viewlist.erase(clientid);
					m_pClientArr[id].viewlist_mutex.unlock();

					m_Server->SendRemoveObject(id, clientid);
				}
				else
					m_pClientArr[id].viewlist_mutex.unlock();
			}
		}
	}

	m_Server->SendPacket(clientid, &sp);
}

void Scene::update()
{
	g_Timer->getTimeset();
	double timeElapsed = g_Timer->getTimeElapsed();

	if (GameStatus == GAMESTATUS::RUNNING) 
	{
	}
	else if (GameStatus == GAMESTATUS::PAUSE)
	{
		g_Timer->getTimeset();
		double timeElapsed = g_Timer->getTimeElapsed();
	}

}

