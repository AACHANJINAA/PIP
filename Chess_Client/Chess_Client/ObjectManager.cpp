#include "stdafx.h"
#include "ObjectManager.h"
#include "MainPlayer.h"
#include "OtherPlayer.h"
#include "Shader.h"
#include "RenderComponent.h"

ObjectManager::ObjectManager()
{
}

ObjectManager::~ObjectManager()
{
	DeleteAll();
}

void ObjectManager::RequestObject(std::shared_ptr<GameObject> WhatYouWant)
{
	_requestobjects.push(WhatYouWant);
}

void ObjectManager::MakeObject(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{
	if (_requestobjects.empty())
	{
		return;
	}

	while (!_requestobjects.empty())
	{
		std::shared_ptr<GameObject> RequestObject = _requestobjects.front();

		auto RequestObject_Render = RequestObject->get_component<RenderComponent>();
		if (RequestObject_Render)
			RequestObject_Render->CreateShaderVariables(pd3dDevice, pd3dCommandList); // 상수 버퍼 생성 로직 추가

		auto RequestObject_Transform = RequestObject->get_component<TransformComponent>();
		switch (RequestObject->_meshType)
		{
		case PLAYER:
		{
			std::shared_ptr<Mesh> Chess_Mesh = std::make_shared<Mesh>(ReadObjMesh{pd3dDevice, pd3dCommandList, "Resource/Character/test_mesh.obj" });

			Chess_Mesh->ChangeColor(pd3dCommandList, 1.0f, 1.0f, 1.0f, 1.f);
			if (RequestObject_Render)
				RequestObject_Render->set_mesh(Chess_Mesh);

			if (RequestObject_Transform)
				RequestObject_Transform->set_scale(1.f, 1.f, 1.f);

			ObjectManager::Instance()->PushObject(RequestObject);
			ObjectManager::Instance()->SetPlayer(RequestObject);
		}
		break;

		case ENEMY:
		{

			std::shared_ptr<Mesh> Chess_Mesh = std::make_shared<Mesh>(ReadObjMesh{ pd3dDevice, pd3dCommandList, "Resource/Monster/test_monster.obj" });

			Chess_Mesh->ChangeColor(pd3dCommandList, 0.0f, 1.0f, 0.0f, 1.f);
			if (RequestObject_Render)
				RequestObject_Render->set_mesh(Chess_Mesh);

			if(RequestObject_Transform)
				RequestObject_Transform->set_scale(1.f, 1.f, 1.f);

			ObjectManager::Instance()->PushEnemy(RequestObject);
		}
		break;

		default:

			break;
		}
		_requestobjects.pop();
	}
	

}

void ObjectManager::DeleteAll()
{
	for (auto& objectList : _allobject)
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
	
	_allobject[WantVecNum].clear();
}

void ObjectManager::DeleteObject()
{
	for (auto& objectList : _allobject)
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
	_rendermap.clear();

	std::array<std::list<std::shared_ptr<GameObject>>, ALLARRAYSIZE>& Arr = GetAllObject();

	for (auto& objectList : Arr) {
		for (auto& object : objectList) {
			auto object_Render = object->get_component<RenderComponent>();
			if (object_Render && object_Render->is_visible(pCamera)) {
				auto maerialShader = object_Render->get_material_shader();

				std::shared_ptr<Shader> shader = nullptr;
				if (maerialShader)
				{
					shader = maerialShader->get_shader();
				}

				if (shader)
				{
					_rendermap[typeid(*shader)].push_back(object);
				}
				else
				{
					_rendermap[typeid(CObjectsShader)].push_back(object);
				}
			}
		}
	}
}

std::map<std::type_index, std::vector<std::shared_ptr<GameObject>>>& ObjectManager::GetRenderMap()
{
	return _rendermap;
}

void ObjectManager::PushObject(std::shared_ptr<GameObject> object)
{
	_allobject[0].push_back(object);
}

void ObjectManager::PushEnemy(std::shared_ptr<GameObject> object)
{
	_allobject[1].push_back(object);
}

void ObjectManager::PushPlayerBullet(std::shared_ptr<GameObject> object)
{
	_allobject[2].push_back(object);
}

void ObjectManager::PushEnemyBullet(std::shared_ptr<GameObject> object)
{
	_allobject[3].push_back(object);
}

void ObjectManager::PushFloorObject(std::shared_ptr<GameObject> object)
{
	_allobject[4].push_back(object);
}


