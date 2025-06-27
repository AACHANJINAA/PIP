#pragma once
#include "GameObject.h"

enum class CommandType : std::uint8_t
{
	MOVE_UP,
	MOVE_DOWN,
	MOVE_RIGHT,
	MOVE_LEFT,
	CONNECT,
	DISCONNECT,
	error
};

class CChess_King : public CGameObject
{
public:
	CChess_King(int X = 0, int Y = 0);
	~CChess_King();

public:
	// CGameObject을(를) 통해 상속됨
	void Animate(float fTimeElapsed, ID3D12GraphicsCommandList* pd3dCommandList) override;
	void Collision(float fElapsedTime) override;
	void ProcessInput(float fElapsedTime, HWND hWnd, UINT nMessageID, POINT ptOldCursorPos) override;

public:
	void SetDistance(float MoveDistance) { _MoveDistance = MoveDistance; }

private:
	void Move_Pos(CommandType Cmd);

private:
	float _MoveDistance{}; // 움직일 거리

	int _NowX{}; // 현재 X위치
	int _NowY{}; // 현재 Y위치
};

