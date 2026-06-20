#pragma once
#include "NPCScript.h"
#include "UIRenderComponent.h"

class TainerScript : public NPCScript {
public:
	void awake() override;
	void init_visual() override;
	void update(float deltaTime) override;
	void on_debug_bt_info(const std::string& nodeName) {
		_currentBTNodeName = nodeName; // 머리 위에 그릴 텍스트 저장
	}

	void handle_animation_branching() override;

private:
	float	_testTimer{ 0.0f };
	int		_testAnimIdx{ 0 };
	std::string _currentBTNodeName; // 현재 행동 트리 노드 이름을 저장하는 변수

	std::shared_ptr<UIRenderComponent> _hpBar_ui{ nullptr };
	float _displayHp{ 0.0f };
	void update_hp_bar(float deltaTime);

	bool _isEndingTriggered{ false };
	float _endingTimer{ 0.0f };
	std::shared_ptr<UIRenderComponent> _endingUI{ nullptr };
};
