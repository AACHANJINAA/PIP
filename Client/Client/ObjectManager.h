#pragma once


class Object;
class GameObject;
//struct GameObjectHandle;
class ObjectManager : public Singleton<ObjectManager>
{
    friend Singleton<ObjectManager>;
private:
    ObjectManager() = default;
    ~ObjectManager() = default;

public:
    std::shared_ptr<GameObject> create_game_object(const std::string& name = "GameObject");
    void request_destruction(std::shared_ptr<Object> objectToDestroy);
    void process_destructions();
	void remove_game_object_from_list(std::shared_ptr<GameObject> gameObject);


    std::shared_ptr<GameObject> find_by_name(const std::string& name);
    std::vector<std::shared_ptr<GameObject>> find_by_layer(uint32_t layerMask);
	const std::vector<std::shared_ptr<GameObject>>& get_all_game_objects() const { return _gameObjects; }

	// [추가] 새로 생성된 GameObject의 Awake/Start를 처리합니다. (GameFramework가 프레임 시작에 호출)
	void process_new_game_objects();

    // [추가] 영속성(persistent) 플래그가 없는 모든 게임 오브젝트를 파괴 요청 목록에 추가합니다.
    void clear_non_persistent_objects();
private:
    
    std::vector<std::shared_ptr<GameObject>> _gameObjects; // tODO : 순회 속도보다 삽입삭제 속도가 더 중요해 질 가능성 농후함 -> 트리 구조로 바꿔야할 가능성 있음
    std::vector<std::shared_ptr<Object>>     _destructionQueue;
    std::queue<std::shared_ptr<GameObject>>  _newGameObjects;
};
// =================================================================
 // [제거된 기능 목록]
 // - RequestObject, MakeObject: 역할이 Scene/Factory로 이전되어 제거
 // - PushObject, PushEnemy, GetObjectVec, GetEnemy 등: Layer 기반 검색으로 대체되어 제거
 // - MakeRenderMap, GetRenderMap: 역할이 Renderer로 이전되어 제거
 // - DeleteObject, DeleteAll, ChangeRoom: 새로운 지연 파괴 메커니즘으로 대체되어 제거
 // - m_Player, _allobject, _requestobjects, _rendermap 등: 새로운 멤버 변수로 대체/통합되어 제거
 // =================================================================



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

