#pragma once


class Timer
{
	std::chrono::system_clock::time_point	m_CurrentTime;
	std::chrono::duration<double>			m_TimeElapsed;

	double					m_fps;

public:
	Timer() {};
	~Timer() {};
	
	void Init();
	void getTimeset();
	double getTimeElapsed() { return m_TimeElapsed.count(); }
	int getFPS() { return (int)m_fps; }
};

