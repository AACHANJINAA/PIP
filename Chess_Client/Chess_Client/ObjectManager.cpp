#include "stdafx.h"
#include "ObjectManager.h"
#include "MainPlayer.h"
#include "OtherPlayer.h"
#include "Shader.h"

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
		RequestObject->CreateShaderVariables(pd3dDevice, pd3dCommandList); // 상수 버퍼 생성 로직 추가
		auto RequestObject_Transform = RequestObject->get_component<TransformComponent>();
		switch (RequestObject->_meshType)
		{
		case PLAYER:
		{

			Mesh* Chess_Mesh = new ReadObjMesh{ pd3dDevice, pd3dCommandList, "Resource/Character/test_mesh.obj" };

			// 색 설정
			Chess_Mesh->ChangeColor(pd3dCommandList, 1.0f, 1.0f, 1.0f, 1.f);
			RequestObject->set_mesh(Chess_Mesh);
			if (RequestObject_Transform)
				RequestObject_Transform->set_scale(1.f, 1.f, 1.f);

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
			RequestObject->set_mesh(Chess_Mesh);

			// 이동 거리 설정
			if(RequestObject_Transform)
				RequestObject_Transform->set_scale(1.f, 1.f, 1.f);

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
			return pObject->_shouldDelete;
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

void ObjectManager::MakeRenderMap(Camera* pCamera)
{
	_RenderMap.clear();

	std::array<std::list<std::shared_ptr<GameObject>>, ALLARRAYSIZE>& Arr = GetAllObject();

	for (auto& objectList : Arr) {
		for (auto& object : objectList) {
			if (object && object->is_visible(pCamera)) {
				// 오브젝트의 머터리얼에서 셰이더를 키로 사용하여 렌더 큐에 추가
				if (nullptr == object->_materialShader)
				{
					_RenderMap[typeid(CObjectsShader)].push_back(object);
				}
				else
				{
					_RenderMap[typeid(*(object->_materialShader->_shader))].push_back(object);
				}
			}
		}
	}
}

std::map<std::type_index, std::vector<std::shared_ptr<GameObject>>>& ObjectManager::GetRenderMap()
{
	return _RenderMap;
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


