#pragma once

#define PIECESIZE 30

class Timer;
class Client;
class Renderer;
class Object;

class Scene
{
private:
	Renderer*	m_Renderer = nullptr;
	Timer*		g_Timer = nullptr;

	int			ChessPiece;

	GAMESTATUS	GameStatus = GAMESTATUS::STOP;

	Client*				m_Client;
	int					m_Board[BOARD_WIDTH][BOARD_HEIGHT] = {};

	list<Object*>		m_Players;
	Object*				Player = nullptr;

	ofstream	outfile;

public:
	Scene();
	~Scene();
	void releaseScene();

	void buildScene();
	void setTimer(Timer* t) { g_Timer = t; }

	void setClient(Client* cli) { m_Client = cli; }

	void keyinput(unsigned char key);
	void keyspcialinput(int key);
	void mouseinput(int button, int state, int x, int y);

	GAMESTATUS GetGamestatus() { return GameStatus; }
	void SetGamestatus(GAMESTATUS s) { GameStatus = s; }

	void update();
	void render();

	void ProcessPacket(char* p);

};