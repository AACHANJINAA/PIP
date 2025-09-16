#pragma once
#include "GameObject.h"


class OtherPlayer : public GameObject, public HPObject
{
public:
	OtherPlayer(int x = 0, int y = 0, int z = 0);
	~OtherPlayer();

public:
	// GameObject을(를) 통해 상속됨
	void Animate(float fTimeElapsed, Camera* pCamera, ID3D12GraphicsCommandList* pd3dCommandList)  override;
	void Collision(float fElapsedTime) override;
	void ProcessInput(float fElapsedTime) override;

public:
	void SetDistance(float MoveDistance) { _MoveDistance = MoveDistance; }
	void SetID(int64_t id) { _id = id; }
	int64_t GetID() const { return _id; }
	void SetName(const std::string& name) { _name = name; }
	std::string GetName() const { return _name; }

private:
	int64_t		_id = -1;
	std::string _name;

	float _MoveDistance{2}; // 움직일 거리

	std::shared_ptr<TransformComponent> OtherPlayer_Trasnform = GetComponent<TransformComponent>();
};

