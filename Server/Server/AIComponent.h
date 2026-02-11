#pragma once
#include "Component.h"
#include "BehaviorTree.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

namespace PIP::GAME
{
	enum class AIMode
	{
		None,
		Lua,
		BT
	};

	class AIComponent : public Component
	{
	public:
		AIComponent(GameObject* owner) : Component(owner) {}
		~AIComponent() override;

		void Initialize() override;
		
        // 기존
        void Update(float deltaTime) override {}

        // [추가] 할당자 버전
		void Update(float deltaTime, JPH::TempAllocator* allocator) override;

		void SetLuaScript(const std::string& path);
		void SetBehaviorTree(std::shared_ptr<BTNode> root);

		std::shared_ptr<Blackboard> GetBlackboard() {
			if (!_blackboard) _blackboard = std::make_shared<Blackboard>();
			return _blackboard;
		}

	private:
		AIMode _mode = AIMode::None;
		lua_State* _L = nullptr;
		
		std::shared_ptr<BTNode> _btRoot;
		std::shared_ptr<Blackboard> _blackboard; // 멤버 변수를 shared_ptr로 변경
	};
}