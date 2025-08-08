#include "stdafx.h"
#include "SceneManager.h"

CSceneManager* CSceneManager::m_SceneManager = nullptr;

CSceneManager::CSceneManager()
{

}

CSceneManager::~CSceneManager()
{

}

void CSceneManager::ChangeScene()
{
	if (m_WantScene != SCENE_NUM::SCENE_NONE)
	{
		switch (m_WantScene)
		{
		case SCENE_NUM::SCENE_NONE:
			break;
		case SCENE_NUM::SCENE_CHESS:
			break;
		case SCENE_NUM::SCENE_OTHER:
			break;
		default:
			break;
		}
	}
}
