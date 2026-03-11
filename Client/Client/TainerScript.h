#pragma once
#include "NPCScript.h"

class TainerScript : public NPCScript {
public:
	void awake() override;
	void init_visual() override;
	void update(float deltaTime) override;
	void on_server_update(const XMFLOAT3& pos, const XMFLOAT3& vel, const XMFLOAT4& rot, uint32_t timestamp) override;

private:
	float	_testTimer{ 0.0f };
	int		_testAnimIdx{ 0 };
};
