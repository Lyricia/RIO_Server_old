#include "stdafx.h"
#include "Server.h"
#include "Scene.h"
#include "Timer.h"
#include "mdump.h"

Scene*		CurrentScene;
Timer*		g_Timer;
Server*		MainServer;

void Initialize()
{
	srand((unsigned)time(NULL));

	g_Timer = new Timer();
	g_Timer->Init();

	CurrentScene = new Scene();
	CurrentScene->setTimer(g_Timer);
	CurrentScene->SetServer(MainServer);
	CurrentScene->buildScene();

	MainServer->RegisterScene(CurrentScene);
}

void SceneChanger(Scene* scene) {
	CurrentScene = scene;
}

int main(int argc, char **argv)
{
	CMiniDump::Begin();
	MainServer = new Server();
	MainServer->InitServer();
	Initialize();
	MainServer->StartListen();

	CurrentScene->releaseScene();

	MainServer->CloseServer();

	CMiniDump::End();
	return 0;
}

