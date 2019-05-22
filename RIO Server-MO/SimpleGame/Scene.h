#pragma once

#define INVALID -1

class Timer;
class Server;
class Object;

class CClient;
class CNPC;

class Scene
{
private:
	Timer*		g_Timer = nullptr;

	int			ChessBoard;
	int			ChessPiece;

	GAMESTATUS	GameStatus = GAMESTATUS::STOP;

	Object*		m_Target = nullptr;
	int			m_Board[BOARD_WIDTH][BOARD_HEIGHT] = {-1};

	Server*			m_Server;

	CClient*		m_pClientArr;
	//CNPC*			m_pNPCList;

public:
	Scene();
	~Scene();
	void releaseScene();

	void SetServer(Server* serv) { m_Server = serv; }

	void buildScene();
	bool isCollide(int x, int y);
	void setTimer(Timer* t) { g_Timer = t; }

	GAMESTATUS GetGamestatus() { return GameStatus; }
	void SetGamestatus(GAMESTATUS s) { GameStatus = s; }

	void ProcessPacket(int id, unsigned char* packet);
	void MoveObject(int clientid, int oldSpaceIdx);

	void RemovePlayerOnBoard(const int x, const int y) { m_Board[x][y] = INVALID; }
	void update();
};