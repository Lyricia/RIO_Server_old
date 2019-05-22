#include "stdafx.h"
#include "Renderer.h"
#include "Object.h"


Object::Object()
{
}

void Object::releaseObject()
{
}

void Object::move(DIR dir)
{
	switch (dir)
	{
	case DIR::UP:
		if (m_Position.y-- < 2) m_Position.y++;
		break;
	case DIR::DOWN:
		if (m_Position.y++ > 7) m_Position.y--;
		break;
	case DIR::LEFT:
		if (m_Position.x-- < 2) m_Position.x++;
		break;
	case DIR::RIGHT:
		if (m_Position.x++ > 7) m_Position.x--;
		break;

	default:break;
	}
}