#include "stdafx.h"
#include "ObjectManager.h"

CObjectManager* CObjectManager::m_ObjectManager = nullptr;

CObjectManager::CObjectManager()
{
	for(int i = 0 ; i < m_vecSize; ++i)
	{
		m_AllObject.push_back(new std::list<CGameObject*>{});
	}

	for (int i = 0; i < m_vecSize; ++i)
	{
		m_AllNewObject.push_back(new std::list<CGameObject*>{});
	}
}

CObjectManager::~CObjectManager()
{
	DeleteAll();
}

void CObjectManager::DeleteAll()
{
	if (m_AllObject.size()) {
		for (std::list<CGameObject*>*& Objects : m_AllObject) {
			if (Objects != nullptr) {
				for (CGameObject*& Object : *Objects) {
					if (Object != nullptr) {
						delete Object;
					}
				}
				Objects->clear();
			}
		}
	}
	m_Player = nullptr;
}

void CObjectManager::UpdateAll()
{
	int i = 0;
	if (m_AllNewObject.size()) {
		for (std::list<CGameObject*>*& Objects : m_AllNewObject) {
			if (Objects != nullptr) {
				for (CGameObject*& Object : *Objects) {
					m_AllObject[i]->emplace_back(Object);
				}
				Objects->clear();
			}
			++i;
		}
	}

	m_Player = m_NewPlayer;
}

void CObjectManager::DeleteVec(int WantVecNum)
{
	if ((int)m_vecSize <= WantVecNum || WantVecNum < 0)
	{
		return;
	}
	for (CGameObject*& Object : *m_AllObject[WantVecNum]) {
		if (Object != nullptr) {
			delete Object;
		}
		m_AllObject[WantVecNum]->clear();
	}
}

void CObjectManager::DeleteObject()
{
	if (m_AllObject.size()) {
		for (std::list<CGameObject*>*& Objects : m_AllObject) {
			if (Objects != nullptr) {
				for (auto iter = Objects->begin(); iter != Objects->end();) {
					if ((*iter)->m_Delete) {
						delete (*iter);
						(*iter) = nullptr;
						//(*iter)->Release();
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

