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
	OBJTYPE				m_type;
	Vector3D<float>		m_Position;
	int					m_Team;
	float				m_Priority;

public:
	Object();
	Object(OBJTYPE type, int size, Vector3D<float> pos) : m_type(type), m_Position(pos) {};
	~Object() {};
	void releaseObject();

	Vector3D<float>	getPosition() { return m_Position; }

	void setPosition(float x, float y, float z) { m_Position = { x, y, z }; }
	void setPosition(Vector3D<float> pos) { m_Position = pos; }
	void setPosition(Vector3D<int> pos) { m_Position = {pos.x, pos.y, pos.z}; }
	void setID(int id) { m_id = id; }
	void setTeam(int team) { m_Team = team; }
	void setType(OBJTYPE type) { m_type = type; }
	void setPriority(float pri) { m_Priority = pri; }

	void move(DIR dir);
	
	OBJTYPE getType() { return m_type; }
	int getTeam() { return m_Team; }
	const int getID() { return m_id; }
	const float getPriority() { return m_Priority; }

	virtual void update(const double timeElapsed) {}
	virtual void render(Renderer* renderer, int texID = NULL) {}

};

inline void DrawSolidRectByMatrix(Vector3D<float> pos, Renderer* Renderer, int size, COLOR color, float level)
{
	Renderer->DrawSolidRect(pos.x, pos.y, pos.z, size, color.r, color.g, color.b, color.a, level);
}

inline void DrawTexturedSeqRectByMatrix(Vector3D<float> pos , Renderer* Renderer, int size, COLOR color, int x, int y, int totalx, int totaly, GLuint texID, float level)
{
	Renderer->DrawTexturedRectSeq(pos.x, pos.y, pos.z, size, color.r, color.g, color.b, color.a, texID, x, y, totalx, totaly, level);
}

inline void DrawTexturedRectByMatrix(Vector3D<float> pos, Renderer* Renderer, int size, COLOR color, GLuint texID, float level)
{
	Renderer->DrawTexturedRect(pos.x, pos.y, pos.z, size, color.r, color.g, color.b, color.a, texID, level);
}

inline void DrawSolidRectGaugeByMatrix(Vector3D<float> pos, Renderer* Renderer, int size, float width, float height, COLOR color, float gauge, float level)
{
	Renderer->DrawSolidRectGauge(pos.x, pos.y + size, pos.z, width, height, color.r, color.g, color.b, color.a, gauge, level);
}