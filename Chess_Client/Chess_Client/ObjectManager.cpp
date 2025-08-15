#include "stdafx.h"
#include "ObjectManager.h"
#include "MainPlayer.h"
#include "OtherPlayer.h"

ObjectManager::ObjectManager()
{
}

ObjectManager::~ObjectManager()
{
	DeleteAll();
}

void ObjectManager::RequestObject(std::shared_ptr<GameObject> WhatYouWant)
{
	m_RequestObjects.push(WhatYouWant);
}

void ObjectManager::MakeObject(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
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
		case PLAYER:
		{
			Mesh* Chess_Mesh = new ReadObjMesh{ pd3dDevice, pd3dCommandList, "Resource/Character/test_mesh.obj" };

			// 색 설정
			Chess_Mesh->ChangeColor(pd3dCommandList, 1.0f, 1.0f, 1.0f, 1.f);
			RequestObject->SetMesh(Chess_Mesh);
			RequestObject->SetScale(1.f, 1.f, 1.f);

			// 매니저에 넣기
			ObjectManager::Instance()->PushObject(RequestObject);
			ObjectManager::Instance()->SetPlayer(RequestObject);
		}
		break;

		case ENEMY:
		{
			Mesh* Chess_Mesh = new ReadObjMesh{ pd3dDevice, pd3dCommandList, "Resource/Monster/test_monster.obj" };

			// 색 설정
			Chess_Mesh->ChangeColor(pd3dCommandList, 0.0f, 1.0f, 0.0f, 1.f);
			RequestObject->SetMesh(Chess_Mesh);

			// 이동 거리 설정
			RequestObject->SetScale(1.f, 1.f, 1.f);

			// 매니저에 넣기
			ObjectManager::Instance()->PushEnemy(RequestObject);
		}
		break;

		default:

			break;
		}
		m_RequestObjects.pop();
	}
	

}

void ObjectManager::DeleteAll()
{
	for (auto& objectList : m_AllObject)
	{
		objectList.clear();
	}

	m_Player = nullptr;
}


void ObjectManager::DeleteVec(int WantVecNum)
{
	if (WantVecNum < 0 || WantVecNum >= ALLARRAYSIZE)
	{
		return;
	}
	
	m_AllObject[WantVecNum].clear();
}

void ObjectManager::DeleteObject()
{
	for (auto& objectList : m_AllObject)
	{
		objectList.remove_if([](const std::shared_ptr<GameObject>& pObject) {
			return pObject->m_Delete;
		});
	}
}

void ObjectManager::ChangeRoom()
{
	m_Player = nullptr; 
	DeleteVec(0);
	DeleteVec(1);
	//DeleteVec(2);
	//DeleteVec(3);
}

void ObjectManager::PushObject(std::shared_ptr<GameObject> object)
{
	m_AllObject[0].push_back(object);
}

void ObjectManager::PushEnemy(std::shared_ptr<GameObject> object)
{
	m_AllObject[1].push_back(object);
}

void ObjectManager::PushPlayerBullet(std::shared_ptr<GameObject> object)
{
	m_AllObject[2].push_back(object);
}

void ObjectManager::PushEnemyBullet(std::shared_ptr<GameObject> object)
{
	m_AllObject[3].push_back(object);
}

void ObjectManager::PushFloorObject(std::shared_ptr<GameObject> object)
{
	m_AllObject[4].push_back(object);
}


