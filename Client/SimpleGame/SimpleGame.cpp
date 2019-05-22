#include "stdafx.h"
#include "Client.h"
#include "Renderer.h"
#include "Scene.h"
#include "Timer.h"
#include "mdump.h"

Scene*		CurrentScene;
Timer*		g_Timer;
Client*		Connector;

void RenderScene(void)
{
	CurrentScene->update();

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glClearColor(1.0f, 1.0f, 1.0f, 1.0f);

	CurrentScene->render();

	glutSwapBuffers();
}

void Initialize()
{
	srand((unsigned)time(NULL));

	g_Timer = new Timer();
	g_Timer->Init();

	CurrentScene = new Scene();
	CurrentScene->setTimer(g_Timer);
	CurrentScene->buildScene();

	Connector = new Client();
	Connector->InitClient();
	CurrentScene->setClient(Connector);
	Connector->RegisterScene(CurrentScene);
}


void Idle(void)
{
	RenderScene();
}

void MouseInput(int button, int state, int x, int y)
{
	x = x - WINDOW_WIDTH*0.5;
	y = -(y - WINDOW_HEIGHT*0.5);

	CurrentScene->mouseinput(button, state, x, y);
	RenderScene();
}

void MouseMove(int x, int y)
{
	x = x - WINDOW_WIDTH*0.5;
	y = -(y - WINDOW_HEIGHT*0.5);

	CurrentScene->mouseinput(0, 0, x, y);
	RenderScene();
}

void KeyInput(unsigned char key, int x, int y)
{
	switch (key)
	{
	case 'q':
		glutLeaveMainLoop();
		break;

	case 13:
		if (CurrentScene->GetGamestatus() == GAMESTATUS::STOP)
			CurrentScene->SetGamestatus(GAMESTATUS::RUNNING);
		break;
		
	case 'r':
		CurrentScene->releaseScene();
		Initialize();
		break;

	default:
		CurrentScene->keyinput(key);
		break;
	}
	RenderScene();
}

void SpecialKeyInput(int key, int x, int y)
{
	CurrentScene->keyspcialinput(key);
	RenderScene();
}

void SceneChanger(Scene* scene) {
	CurrentScene = scene;
}

int main(int argc, char **argv)
{ 
	CMiniDump::Begin();

	// Initialize GL things
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowPosition(100, 100);
	glutInitWindowSize(WINDOW_WIDTH, WINDOW_HEIGHT);
	glutCreateWindow("Game Client Programming");
	glewInit();

	if (glewIsSupported("GL_VERSION_3_0"))
	{
		std::cout << " GLEW Version is 3.0\n ";
	}
	else
	{
		std::cout << "GLEW 3.0 not supported\n ";
	}

	Initialize();

	glutDisplayFunc(RenderScene);
	glutIdleFunc(Idle);
	glutKeyboardFunc(KeyInput);
	glutMouseFunc(MouseInput);
	glutSpecialFunc(SpecialKeyInput);
	glutPassiveMotionFunc(MouseMove);
	glutMainLoop();

	CurrentScene->releaseScene();
	CMiniDump::End();
    return 0;
}


