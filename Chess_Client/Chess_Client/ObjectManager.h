#pragma once
#include "GameObject.h"
#include "BoardCube.h"

class ObjectManager : public Singleton<ObjectManager>
{
	friend Singleton<ObjectManager>;
public:

	ObjectManager();
	~ObjectManager();

public:
	// 요청과 만들기
	void RequestObject(std::shared_ptr<GameObject> WhatYouWant);
	void MakeObject(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);

	// 모두 삭제
	void DeleteAll();
	void DeleteVec(int WantVecNum);

	void DeleteObject();

	// 방 바꾸기 -> 임시방편으로 바닥 빼고 다 지우는 걸로 해놨쪙
	void ChangeRoom();

	// 플레이어
	void SetPlayer(std::shared_ptr <GameObject> player) { m_Player = player; }
	std::shared_ptr<GameObject> GetPlayer() const { return m_Player; }

	// 어떤 셰이더를 사용하는지에 따라 렌더하기 위한 것들
	void MakeRenderMap(Camera* pCamera);
	std::map<std::type_index, std::vector<std::shared_ptr<GameObject>>>& GetRenderMap();

	// 오브젝트 넣기
	// 플레이어 = 체력, 위치(x,y,z), name, id, size
	// 다른 플레이어 = 체력, 위치(x,y,z), name, id, size


	void PushObject(std::shared_ptr<GameObject> object);
	void PushEnemyBullet(std::shared_ptr<GameObject> object);
	void PushPlayerBullet(std::shared_ptr<GameObject> object);
	void PushEnemy(std::shared_ptr<GameObject> object);
	void PushFloorObject(std::shared_ptr<GameObject> object);


	// 오브젝트 얻기
	std::array<std::list<std::shared_ptr<GameObject>>, ALLARRAYSIZE>& GetAllObject() { return m_AllObject; }

	std::list<std::shared_ptr<GameObject>>& GetObjectVec() { return m_AllObject[0]; } 
	std::list<std::shared_ptr<GameObject>>& GetEnemy() { return  m_AllObject[1]; }
	std::list<std::shared_ptr<GameObject>>& GetPlayerBulletVec() { return  m_AllObject[2]; }
	std::list<std::shared_ptr<GameObject>>& GetEnemyBulletVec() { return  m_AllObject[3]; }
	std::list<std::shared_ptr<GameObject>>& GetFloor() { return  m_AllObject[4]; }

	GameObject* Terrain{};


private:


	std::shared_ptr <GameObject> m_Player{};

	std::array<std::list<std::shared_ptr<GameObject>>, ALLARRAYSIZE> m_AllObject{}; // 현재 씬 오브젝트


	// 요청 임시 변수
	std::queue<std::shared_ptr<GameObject>> m_RequestObjects{};
	
	std::map<std::type_index, std::vector<std::shared_ptr<GameObject>>> _RenderMap{};

};

