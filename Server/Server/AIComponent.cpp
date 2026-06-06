#include "pch.h"
#include "AIComponent.h"
#include "GameObject.h"
#include "LuaManager.h"

namespace PIP::GAME
{
	AIComponent::~AIComponent()
	{
		if (_L)
		{
			lua_close(_L);
			_L = nullptr;
		}
	}

	void AIComponent::Initialize()
	{
		_blackboard = nullptr;
		_L = nullptr;
		_btRoot = nullptr;
		_mode = AIMode::None;
		auto bb = GetBlackboard();
		bb->set("owner", GetOwner());
	}

	void AIComponent::Update(float deltaTime, JPH::TempAllocator* allocator)
	{
		if (_mode == AIMode::None) return;

		if (_mode == AIMode::Lua)
		{
			if (!_L) return;

			lua_getglobal(_L, "Update");
			if (lua_isfunction(_L, -1))
			{
				lua_pushnumber(_L, deltaTime);
				if (lua_pcall(_L, 1, 0, 0) != LUA_OK)
				{
					const char* err = lua_tostring(_L, -1);
					MYERROR("Lua Update Error: " << err);
					lua_pop(_L, 1);
				}
			}
			else
			{
				lua_pop(_L, 1);
			}
		}
		else if (_mode == AIMode::BT)
		{
			if (_btRoot)
			{
				_btRoot->tick(deltaTime, allocator);
			}
		}
	}

	void AIComponent::SetLuaScript(const std::string& path)
	{
		if (_L) lua_close(_L);

		_L = luaL_newstate();
		if (_L)
		{
			LuaManager::Instance()->RegisterFunctions(_L);

			lua_pushlightuserdata(_L, GetOwner());
			lua_setglobal(_L, "__gameObject");

			if (luaL_dofile(_L, path.c_str()) != LUA_OK)
			{
				const char* err = lua_tostring(_L, -1);
				MYERROR("Lua Load Error (" << path << "): " << err);
				lua_pop(_L, 1);
				return;
			}
			_mode = AIMode::Lua;
		}
	}
	void AIComponent::SetBehaviorTree(std::shared_ptr<BTNode> root)
	{
		_btRoot = root;
		_mode = AIMode::BT;
	}
}