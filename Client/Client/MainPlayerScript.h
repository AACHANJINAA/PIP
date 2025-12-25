#pragma once
#include "ScriptComponent.h"

class RenderComponent;
class MainPlayerScript : public ScriptComponent
{
public:
	using required_components = std::tuple<RenderComponent>;

    MainPlayerScript() = default;
    virtual ~MainPlayerScript() = default;

    // --- 생명주기 함수 오버라이드 ---
    // 매 프레임 입력을 확인하기 위해 update 함수를 재정의합니다.
    virtual void update(float deltaTime) override;
    void awake() override;

    // --- 고유 기능 ---
    // (예시: HP, ID, 이름 등 플레이어의 상태를 관리하는 변수와 함수들이 여기에 위치할 수 있습니다.)
	void set_hp(int hp) { _hp = hp; }
	int hp() const { return _hp; }
    void set_position(const f3& pos)
	{
		auto transform = this->transform();
        if (transform)
        {
            transform->set_local_position(pos);
		}
	}
	const f3& position() const { return this->transform()->local_position(); }

    void set_id(int64_t id) { _playerId = id; }
	int64_t id() const { return _playerId; }

private:
    // 기존 MainPlayer의 private 함수였던 move_pos를 가져옵니다.
    void move_pos(common::packet::MOVE_TYPE cmd);

    // (예시: 플레이어의 상태 변수들)
    int _hp;
    int64_t _playerId;
	RenderComponent* _renderComponent{ nullptr };
	GameObject* _camera{ nullptr };

    // 속도
    float _speed{5.f};

    // 20ms 마다 서버에 위치 정보를 전송하기 위한 타이머
    float _sendTimer{ 0.f };
    const float _sendInterval{ 0.02f };
};


//class MainPlayer : public GameObject, public HPObject
//{
//public:
//	MainPlayer(int x = 0, int y = 0, int z = 0);
//	~MainPlayer();
//
//public:
//	// GameObject을(를) 통해 상속됨
//	void animate(float elapsed_time, Camera* camera, ID3D12GraphicsCommandList* command_list) override;
//	void collision(float elapsed_time) override;
//	void process_input(float elapsed_time) override;
//
//public:
//	void SetDistance(float MoveDistance) { _MoveDistance = MoveDistance; }
//	void SetID(int64_t id) { _id = id; }
//	int64_t GetID() const { return _id; }
//	void SetName(const std::string& name) { _name = name; }
//	std::string GetName() const { return _name; }
//	
//private:
//	void Move_Pos(common::packet::MOVE_TYPE Cmd);
//
//private:
//	int64_t		_id = -1;
//	std::string _name;
//
//	float _MoveDistance{2}; // 움직일 거리
//
//	std::shared_ptr<TransformComponent> MainPlayer_Trasnform = get_component<TransformComponent>();
//};

