#include "stdafx.h"
#include "ObjectManager.h"

CObjectManager* CObjectManager::m_ObjectManager = nullptr;

CObjectManager::CObjectManager()
{
}

CObjectManager::~CObjectManager()
{
	DeleteAll();
}

void CObjectManager::DeleteAll()
{
	for (auto& objectList : m_AllObject)
	{
		objectList.clear();
	}

	m_Player = nullptr;
}


void CObjectManager::DeleteVec(int WantVecNum)
{
	if (WantVecNum < 0 || WantVecNum >= ALLARRAYSIZE)
	{
		return;
	}
	
	m_AllObject[WantVecNum].clear();
}

void CObjectManager::DeleteObject()
{
	for (auto& objectList : m_AllObject)
	{
		objectList.remove_if([](const std::shared_ptr<CGameObject>& pObject) {
			return pObject->m_Delete;
		});
	}
}

void CObjectManager::PushObject(std::shared_ptr<CGameObject> object)
{
	m_AllObject[0].push_back(object);
}

void CObjectManager::PushEnemyBullet(std::shared_ptr<CGameObject> object)
{
	m_AllObject[1].push_back(object);
}

void CObjectManager::PushPlayerBullet(std::shared_ptr<CGameObject> object)
{
	m_AllObject[2].push_back(object);
}

void CObjectManager::PushEnemy(std::shared_ptr<CGameObject> object)
{
	m_AllObject[3].push_back(object);
}

void CObjectManager::PushFloorObejct(std::shared_ptr<CGameObject> object)
{
	m_AllObject[4].push_back(object);
}


