#pragma once


class Object;
class GameObject;
class Component;
class ObjectManager : public Singleton<ObjectManager>
{
    friend Singleton<ObjectManager>;
private:
    ObjectManager() = default;
    ~ObjectManager() = default;

public:
    // --- 객체 생성 및 파괴 ---
    std::shared_ptr<GameObject> create_game_object(const std::string& name = "GameObject", int layer = 0);
    void request_destruction(std::shared_ptr<Object> objectToDestroy);
    void process_destructions();

    // --- 객체 검색 ---
    std::shared_ptr<GameObject> find_by_name(const std::string& name);
    std::vector<std::shared_ptr<GameObject>> find_by_layer(int layer);
    // (필요시) std::shared_ptr<GameObject> find_by_tag(const std::string& tag);

    // --- 전체 객체 접근 ---
    const std::vector<std::shared_ptr<GameObject>>& get_all_game_objects() const { return _gameObjects; }

private:
    void remove_game_object_from_list(std::shared_ptr<GameObject> gameObject);

    std::vector<std::shared_ptr<GameObject>> _gameObjects; // 모든 GameObject를 관리하는 단일 리스트

    std::vector<std::shared_ptr<Object>> _destructionQueue; // 모든 Object 파생 클래스를 담는 단일 큐
    std::mutex _mutex;
};
//class ObjectManager : public Singleton<ObjectManager>
//{
//	friend Singleton<ObjectManager>;
//public:
//
//	ObjectManager();
//	~ObjectManager();
//
//public:
//	// 요청과 만들기
//	void RequestObject(std::shared_ptr<GameObject> WhatYouWant);
//	void MakeObject(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
//
//	// 모두 삭제
//	void DeleteAll();
//	void DeleteVec(int WantVecNum);
//
//	void DeleteObject();
//
//	// 방 바꾸기 -> 임시방편으로 바닥 빼고 다 지우는 걸로 해놨쪙
//	void ChangeRoom();
//
//	// 플레이어
//	void SetPlayer(std::shared_ptr <GameObject> player) { m_Player = player; }
//	std::shared_ptr<GameObject> GetPlayer() const { return m_Player; }
//
//	// 어떤 셰이더를 사용하는지에 따라 렌더하기 위한 것들
//	void MakeRenderMap(Camera* pCamera);
//	std::map<std::type_index, std::vector<std::shared_ptr<GameObject>>>& GetRenderMap();
//
//	// 오브젝트 넣기
//	// 플레이어 = 체력, 위치(x,y,z), name, id, size
//	// 다른 플레이어 = 체력, 위치(x,y,z), name, id, size
//
//
//	void PushObject(std::shared_ptr<GameObject> object);
//	void PushEnemyBullet(std::shared_ptr<GameObject> object);
//	void PushPlayerBullet(std::shared_ptr<GameObject> object);
//	void PushEnemy(std::shared_ptr<GameObject> object);
//	void PushFloorObject(std::shared_ptr<GameObject> object);
//
//
//	// 오브젝트 얻기
//	std::array<std::list<std::shared_ptr<GameObject>>, ALLARRAYSIZE>& GetAllObject() { return _allobject; }
//
//	std::list<std::shared_ptr<GameObject>>& GetObjectVec() { return _allobject[0]; }
//	std::list<std::shared_ptr<GameObject>>& GetEnemy() { return  _allobject[1]; }
//	std::list<std::shared_ptr<GameObject>>& GetPlayerBulletVec() { return  _allobject[2]; }
//	std::list<std::shared_ptr<GameObject>>& GetEnemyBulletVec() { return  _allobject[3]; }
//	std::list<std::shared_ptr<GameObject>>& GetFloor() { return  _allobject[4]; }
//
//	GameObject* Terrain{};
//
//
//private:
//
//
//	std::shared_ptr <GameObject> m_Player{};
//
//	std::array<std::list<std::shared_ptr<GameObject>>, ALLARRAYSIZE> _allobject{}; // 현재 씬 오브젝트
//
//
//	// 요청 임시 변수
//	std::queue<std::shared_ptr<GameObject>> _requestobjects{};
//	
//	std::map<std::type_index, std::vector<std::shared_ptr<GameObject>>> _rendermap{};
//
//};

