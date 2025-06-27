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
	void UpdateAll();
	void DeleteVec(int WantVecNum);

	void DeleteObject();

	// 플레이어
	void SetPlayer(CGameObject* player) { m_NewPlayer = player; }
	CGameObject* GetPlayer() { return m_Player; }


	// 오브젝트 넣기
	void NewPushObject(CGameObject* Object) { m_AllNewObject[0]->push_back(Object); }
	void NewPushEnemyBullet(CGameObject* Object) { m_AllNewObject[1]->push_back(Object); }
	void NewPushPlayerBullet(CGameObject* Object) { m_AllNewObject[2]->push_back(Object); }
	void NewPushEnemy(CGameObject* Object) { m_AllNewObject[3]->push_back(Object);}
	void NewPushFloorObejct(CGameObject* Object) { m_AllNewObject[4]->push_back(Object); }


	void PushObject(CGameObject* Object) { m_AllObject[0]->push_back(Object); }
	void PushEnemyBullet(CGameObject* Object) { m_AllObject[1]->push_back(Object); }
	void PushPlayerBullet(CGameObject* Object) { m_AllObject[2]->push_back(Object); }
	void PushEnemy(CGameObject* Object) { m_AllObject[3]->push_back(Object); }
	void PushFloorObejct(CGameObject* Object) { m_AllObject[4]->push_back(Object); }


	// 오브젝트 얻기
	std::vector<std::list<CGameObject*>*>& GetAllObject(){ return m_AllObject; }


	std::list<CGameObject*>& GetObjectVec() { return *m_AllObject[0];}
	std::list<CGameObject*>& GetEnemyBulletVec() { return  *m_AllObject[1]; }
	std::list<CGameObject*>& GetPlayerBulletVec() { return  *m_AllObject[2]; }
	std::list<CGameObject*>& GetEnemy() { return  *m_AllObject[3]; }
	std::list<CGameObject*>& GetFloor() { return  *m_AllObject[4]; }

	CGameObject* Terrain{};

private:
	static CObjectManager* m_ObjectManager;

	CGameObject* m_Player{};

	CGameObject* m_NewPlayer{};

	std::vector<std::list<CGameObject*>*> m_AllObject{}; // 현재 씬 오브젝트

	std::vector<std::list<CGameObject*>*> m_AllNewObject{}; // 새로운 씬 오브젝트

	size_t m_vecSize{5}; // 벡터의 사이즈

};

