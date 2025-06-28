#include "stdafx.h"
#include "ObjectManager.h"
#include "Chess_King.h"
#include "Other_King.h"

CObjectManager* CObjectManager::m_ObjectManager = nullptr;

CObjectManager::CObjectManager()
{
}

CObjectManager::~CObjectManager()
{
	DeleteAll();
}

void CObjectManager::RequestObject(std::shared_ptr<CGameObject> WhatYouWant)
{
	m_RequestObjects.push(WhatYouWant);
}

void CObjectManager::MakeObject(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
	if (m_RequestObjects.empty())
	{
		return;
	}

	while (!m_RequestObjects.empty())
	{
		auto RequestObject = m_RequestObjects.front();
		switch (RequestObject->m_Mesh_Type)
		{
		case I_WANT_CHESS_PLAYER:
		{
			CMesh* Chess_Mesh = new CReadObjMesh{ pd3dDevice, pd3dCommandList, "Resource/Chess_King.obj" };

			// 색 설정
			Chess_Mesh->ChangeColor(pd3dCommandList, 1.0f, 1.0f, 1.0f, 1.f);
			RequestObject->SetMesh(Chess_Mesh);

			// 이동 거리 설정
			RequestObject->SetScale(1.f, 1.f, 1.f);

			// 매니저에 넣기
			CObjectManager::GetManager()->PushObject(RequestObject);
		}
		break;

		case I_WANT_CHESS_ENEMY:
		{
			CMesh* Chess_Mesh = new CReadObjMesh{ pd3dDevice, pd3dCommandList, "Resource/Chess_King.obj" };

			// 색 설정
			Chess_Mesh->ChangeColor(pd3dCommandList, 0.0f, 0.0f, 0.0f, 1.f);
			RequestObject->SetMesh(Chess_Mesh);

			// 이동 거리 설정
			RequestObject->SetScale(1.f, 1.f, 1.f);

			// 매니저에 넣기
			CObjectManager::GetManager()->PushObject(RequestObject);
		}
		break;

		default:

			break;
		}
		m_RequestObjects.pop();
	}
	

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


