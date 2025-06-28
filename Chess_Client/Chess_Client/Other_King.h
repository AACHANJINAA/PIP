#pragma once
#include "GameObject.h"


class COther_King : public CGameObject
{
public:
	COther_King(int X = 0, int Y = 0);
	~COther_King();

public:
	// CGameObject을(를) 통해 상속됨
	void Animate(float fTimeElapsed, ID3D12GraphicsCommandList* pd3dCommandList) override;
	void Collision(float fElapsedTime) override;
	void ProcessInput(float fElapsedTime, HWND hWnd, UINT nMessageID, POINT ptOldCursorPos) override;

public:
	void SetDistance(float MoveDistance) { _MoveDistance = MoveDistance; }
	void SetID(int64_t id) { _id = id; }
	int64_t GetID() const { return _id; }
	void SetName(const std::string& name) { _name = name; }
	std::string GetName() const { return _name; }
	void SetPos(int x = 0, int y = 0) { _NowX = x; _NowY = y; }

private:
	

private:
	int64_t		_id = -1;
	std::string _name;
	int			_NowX{}; // 현재 X위치
	int			_NowY{}; // 현재 Y위치

	float _MoveDistance{2}; // 움직일 거리
};

