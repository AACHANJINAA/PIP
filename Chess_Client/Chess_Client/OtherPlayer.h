#pragma once
#include "GameObject.h"

// TODO: 이것은 스크립트가 되어야함 오브젝트에 컴포넌트 합치는 것은 런타임에? 아님 상속으로?
class OtherPlayer : public GameObject, public HPObject
{
public:
	OtherPlayer(int x = 0, int y = 0, int z = 0);
	~OtherPlayer();

public:
	// GameObject을(를) 통해 상속됨
	void animate(float elapsed_time, Camera* camera, ID3D12GraphicsCommandList* command_list)  override;
	void collision(float elapsed_time) override;
	void process_input(float elapsed_time) override;

public:
	void SetDistance(float MoveDistance) { _MoveDistance = MoveDistance; }
	void SetID(int64_t id) { _id = id; }
	int64_t GetID() const { return _id; }
	void SetName(const std::string& name) { _name = name; }
	std::string GetName() const { return _name; }

private:
	

private:
	int64_t		_id = -1;
	std::string _name;

	float _MoveDistance{2}; // 움직일 거리
};

