#include "stdafx.h"
#include "ObjectManager.h"

CObjectManager* CObjectManager::m_ObjectManager = nullptr;

CObjectManager::CObjectManager()
{
	for(int i = 0 ; i < ALLARRAYSIZE; ++i)
	{
		m_AllObject[i] = new std::list<std::unique_ptr<CGameObject>>{};
	}
}

CObjectManager::~CObjectManager()
{
	DeleteAll();
	for (std::list<std::unique_ptr<CGameObject>>* iter : m_AllObject)
	{
		delete iter;
		iter = nullptr;
	}
}

void CObjectManager::DeleteAll()
{
	for (auto& vec : m_AllObject)
	{
		vec->clear();
	}

	m_Player = nullptr;
}


void CObjectManager::DeleteVec(int WantVecNum)
{
	if ((int)ALLARRAYSIZE <= WantVecNum || WantVecNum < 0)
	{
		return;
	}
	
	m_AllObject[WantVecNum]->clear();



}

void CObjectManager::DeleteObject()
{
	if (m_AllObject.size()) {
		for (std::list<std::unique_ptr<CGameObject>>*& Objects : m_AllObject) {
			if (Objects != nullptr) {
				for (auto iter = Objects->begin(); iter != Objects->end();) {
					if ((*iter).get()->m_Delete) {
						(*iter).reset(nullptr);
						iter = Objects->erase(iter);
					}
					else
					{
						++iter;
					}
				}
			}
		}
	}
}

void CObjectManager::PushObject(std::unique_ptr<CGameObject> object)
{
	m_AllObject[0]->emplace_back(std::move(object));
}

void CObjectManager::PushEnemyBullet(std::unique_ptr<CGameObject> object)
{
	m_AllObject[1]->emplace_back(std::move(object));
}

void CObjectManager::PushPlayerBullet(std::unique_ptr<CGameObject> object)
{
	m_AllObject[2]->emplace_back(std::move(object));
}

void CObjectManager::PushEnemy(std::unique_ptr<CGameObject> object)
{
	m_AllObject[3]->emplace_back(std::move(object));
}

void CObjectManager::PushFloorObejct(std::unique_ptr<CGameObject> object)
{
	m_AllObject[4]->emplace_back(std::move(object));
}


