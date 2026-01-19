#pragma once
#include "BehaviorTree.h"
#include "Component.h"

namespace PIP
{
	enum class AIMode
	{
		Lua = 0,
		BT = 1,
		None = 2,
	};
	class AIComponent : public Component
	{
	public:
		AIComponent(GameObject* owner) : Component(owner) {}
		~AIComponent() override;

		void Initialize() override;
		void Update(float deltaTime) override;

		// Lua 모드 설정 및 스크립트 로드
		void SetLuaScript(const std::string& path);

		// BT 모드 설정 및 루트 노드 설정
		void SetBehaviorTree(std::shared_ptr<BTNode> root);

		// AI 데이터 공유를 위한 블랙보드 접근
		Blackboard& GetBlackboard() { return _blackboard; }
		const Blackboard& GetBlackboard() const { return _blackboard; }

	private:
		AIMode _mode = AIMode::None;

		// --- Lua 관련 ---
		lua_State* _L = nullptr;

		// --- Behavior Tree 관련 ---
		std::shared_ptr<BTNode> _btRoot = nullptr;
		Blackboard _blackboard;
	};
}