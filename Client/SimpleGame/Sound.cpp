#include "stdafx.h"
#include <iostream>
#include "Sound.h"

#define MAX_SOUND 100

Sound::Sound()
{
	m_engine = createIrrKlangDevice();

	if (!m_engine)
	{
		std::cout << "Sound Initialization failed. \n";
	}
}

int Sound::CreateSound(char* filePath)
{
	int index = 0;

	for (int i = 1; i < MAX_SOUND; i++)
	{
		if (m_soundList.find(i) == m_soundList.end())
		{
			index = i;
			break;
		}
	}
	ISoundSource* tempSound = m_engine->addSoundSourceFromFile(filePath);
	if (tempSound == NULL)
	{
		std::cout << "Sound creation fails. \n";
		return -1;
	}
	m_soundList.insert(std::pair<int, ISoundSource*>(index, tempSound));

	return index;
}

void Sound::DeleteSound(int index)
{
	std::map<int, ISoundSource*>::iterator iter;

	iter = m_soundList.find(index);

	if (iter == m_soundList.end())
	{
		std::cout << "No such sound with index " << index << ". \n";
		return;
	}
	(*iter).second->drop();
	m_soundList.erase(index);
}

void Sound::PlaySound(int index, bool bLoop, float volume)
{
	std::map<int, ISoundSource*>::iterator iter;

	iter = m_soundList.find(index);

	if (iter == m_soundList.end())
	{
		std::cout << "No such sound with index " << index << ". \n";
		return;
	}
	(*iter).second->setDefaultVolume(volume);
	m_engine->play2D((*iter).second, bLoop);
}

void Sound::StopAllSound()
{
	m_engine->stopAllSounds();
}

void Sound::PauseAllSound(bool isPaused)
{
	m_engine->setAllSoundsPaused(isPaused);
}

Sound::~Sound()
{
	std::map<int, ISoundSource*>::iterator iter;

	for (iter = m_soundList.begin(); iter != m_soundList.end(); iter++)
	{
		DeleteSound((*iter).first);
	}

	m_engine->drop();
	m_soundList.clear();
}
