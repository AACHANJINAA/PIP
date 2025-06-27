#pragma once
#include "GameObject.h"

class CObjectManager
{
public:
	CObjectManager();
	~CObjectManager();

	static CObjectManager* GetManager() {
		if (m_ObjectManager == nullptr)
		{
			m_ObjectManager = new CObjectManager{};
		}
		return m_ObjectManager;
	}

	static void DeleteManager() {
		if (m_ObjectManager != nullptr)
		{
			delete m_ObjectManager;
		}
	}

public:
	// 모두 삭제
	void DeleteAll();
	void DeleteVec(int WantVecNum);

	void DeleteObject();

	// 플레이어
	void SetPlayer(CGameObject* player) { m_Player = player; }
	CGameObject* GetPlayer() { return m_Player; }


	// 오브젝트 넣기


	void PushObject(std::unique_ptr<CGameObject> object);
	void PushEnemyBullet(std::unique_ptr<CGameObject> object);
	void PushPlayerBullet(std::unique_ptr<CGameObject> object);
	void PushEnemy(std::unique_ptr<CGameObject> object);
	void PushFloorObejct(std::unique_ptr<CGameObject> object);


	// 오브젝트 얻기
	std::array<std::list<std::unique_ptr<CGameObject>>*, ALLARRAYSIZE>& GetAllObject() { return m_AllObject; }


	std::list<std::unique_ptr<CGameObject>>& GetObjectVec() { return *m_AllObject[0]; }
	std::list<std::unique_ptr<CGameObject>>& GetEnemyBulletVec() { return  *m_AllObject[1]; }
	std::list<std::unique_ptr<CGameObject>>& GetPlayerBulletVec() { return  *m_AllObject[2]; }
	std::list<std::unique_ptr<CGameObject>>& GetEnemy() { return  *m_AllObject[3]; }
	std::list<std::unique_ptr<CGameObject>>& GetFloor() { return  *m_AllObject[4]; }

	CGameObject* Terrain{};


private:

	static CObjectManager* m_ObjectManager;

	CGameObject* m_Player{};

	std::array<std::list<std::unique_ptr<CGameObject>>*, ALLARRAYSIZE> m_AllObject{}; // 현재 씬 오브젝트

};

