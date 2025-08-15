#pragma once
#include "GameObject.h"
class BoardCube : public GameObject
{
public:
	BoardCube();
	virtual ~BoardCube();

public:
	virtual void Animate(float fTimeElapsed, ID3D12GraphicsCommandList* pd3dCommandList);
	virtual void Collision(float fElapsedTime);

	// CGameObject을(를) 통해 상속됨
	void ProcessInput(float fElapsedTime, HWND hWnd, UINT nMessageID, POINT ptOldCursorPos) override;
};

