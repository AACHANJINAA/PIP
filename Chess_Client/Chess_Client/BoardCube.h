#pragma once
#include "GameObject.h"
class BoardCube : public GameObject
{
public:
	BoardCube();
	virtual ~BoardCube();

public:
	virtual void Animate(float fTimeElapsed, Camera* pCamera, ID3D12GraphicsCommandList* pd3dCommandList);
	virtual void Collision(float fElapsedTime);
	virtual void ProcessInput(float fElapsedTime) override;
};

