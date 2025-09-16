#pragma once
#include "GameObject.h"
class BoardCube : public GameObject
{
public:
	BoardCube();
	virtual ~BoardCube();

public:
	virtual void animate(float elapsed_time, Camera* camera, ID3D12GraphicsCommandList* command_list);
	virtual void collision(float elapsed_time);
	virtual void process_input(float elapsed_time) override;
};

