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
	// =================================================================
    // 1. 객체 생성 및 파괴 (새로운 핵심 기능)
    // =================================================================

    // [변경] 이제 ObjectManager는 이 함수를 통해 모든 GameObject를 생성합니다.
    std::shared_ptr<GameObject> create_game_object(const std::string& name = "GameObject", int layer = 0);

    // [변경] 모든 Object 파생 클래스(GameObject, Component 등)의 파괴 요청을 받습니다.
    void request_destruction(std::shared_ptr<Object> objectToDestroy);

    // [변경] GameFramework가 프레임 끝에 호출하여 파괴 큐를 실제로 처리합니다.
    void process_destructions();


    // =================================================================
    // 2. 객체 검색 (새로운 핵심 기능)
    // =================================================================

    // [대체] GetPlayer() 등 특정 객체를 찾던 기능을 대체합니다.
    std::shared_ptr<GameObject> find_by_name(const std::string& name);

    // [대체] GetObjectVec(), GetEnemy() 등 종류별 리스트를 반환하던 기능을 대체합니다.
    std::vector<std::shared_ptr<GameObject>> find_by_layer(int layer);

    // [대체] GetAllObject()를 대체합니다. 이제 모든 객체는 이 함수를 통해 단일 리스트로 접근합니다.
    const std::vector<std::shared_ptr<GameObject>>& get_all_game_objects() const { return _gameObjects; }

	// [추가] 새로 생성된 GameObject의 Awake/Start를 처리합니다. (GameFramework가 프레임 시작에 호출)
	void process_new_game_objects();

private:
    // [추가] 파괴 과정에서 사용되는 내부 헬퍼 함수입니다.
    void remove_game_object_from_list(std::shared_ptr<GameObject> gameObject);

    // =================================================================
    // 3. 멤버 변수 (완전히 새로 구성)
    // =================================================================

    // [변경] _allobject, _requestobjects, _rendermap 등 모든 복잡한 컨테이너를 아래 단 두 개로 통합합니다.

	// 모든 활성 GameObject를 저장하는 단일 리스트
	std::vector<std::shared_ptr<GameObject>> _gameObjects;

	std::queue<std::shared_ptr<GameObject>> _newGameObjects; // 새로 생성된 오브젝트를 임시로 담아두는 큐 (awke,start 루틴)

    // 파괴가 예약된 모든 Object를 임시로 담아두는 큐
    std::vector<std::shared_ptr<Object>> _destructionQueue;

    // (참고) 멀티스레딩 환경에서의 안전한 접근을 위한 뮤텍스 -> 락 프리를 지향해야함 임시적으로 추가
    std::mutex _mutex;
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

