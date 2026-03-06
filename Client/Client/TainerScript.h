#pragma once
#include "NPCScript.h"

class TainerScript : public NPCScript {
public:
	void awake() override;
	void init_visual() override;
	void update(float deltaTime) override;
private:
	float	_testTimer{ 0.0f };
	int		_testAnimIdx{ 0 };
};
