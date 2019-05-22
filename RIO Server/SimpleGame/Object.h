#pragma once

struct COLOR {
	float r;
	float g;
	float b;
	float a;
};


class Object
{
protected:
	int					m_id;
	Vector3D<float>		m_Position;
	int					m_Team;
	
public:
	Object();
	~Object() {};
	void releaseObject();

	Vector3D<float>	getPosition() { return m_Position; }

	void setPosition(float x, float y, float z) { m_Position = { x, y, z }; }
	void setPosition(Vector3D<float> pos) { m_Position = pos; }
	void setPosition(Vector3D<int> pos) { m_Position = Vec3i_to_Vec3f(pos); };
	void setID(int id) { m_id = id; }
	void setTeam(int team) { m_Team = team; }

	int getTeam() { return m_Team; }
	const int getID() { return m_id; }

	virtual void update(const double timeElapsed) {}
};