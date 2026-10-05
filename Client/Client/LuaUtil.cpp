#include "stdafx.h"
#include "LuaUtil.h"
#include <lua.hpp>

LuaState::LuaState()
{
	_L = luaL_newstate();
	luaL_openlibs(_L);
}

LuaState::~LuaState()
{
	if (_L) lua_close(_L);
}

bool LuaState::do_file(const std::string& path)
{
	if (luaL_dofile(_L, path.c_str()) != LUA_OK)
	{
		const char* err = lua_tostring(_L, -1);
		CERROR("[Lua] " << path << " 읽기 실패: " << (err ? err : "(알 수 없음)"));
		lua_pop(_L, 1);
		return false;
	}
	return true;
}

LuaTable::LuaTable(lua_State* L, int index, std::string context)
	: _L(L), _index(lua_absindex(L, index)), _context(std::move(context))
{
}

int LuaTable::push_field(const char* key)
{
	_readKeys.insert(key);
	return lua_getfield(_L, _index, key);
}

void LuaTable::warn_type(const char* key, const char* expected)
{
	CERROR("[Lua] " << _context << "." << key << ": 형식이 맞지 않음, " << expected << " 필요 (기본값 사용)");
}

bool LuaTable::has(const char* key)
{
	const bool exists = push_field(key) != LUA_TNIL;
	lua_pop(_L, 1);
	return exists;
}

float LuaTable::number(const char* key, float def)
{
	float value = def;
	const int type = push_field(key);
	if (type == LUA_TNUMBER) value = static_cast<float>(lua_tonumber(_L, -1));
	else if (type != LUA_TNIL) warn_type(key, "숫자");
	lua_pop(_L, 1);
	return value;
}

int LuaTable::integer(const char* key, int def)
{
	int value = def;
	const int type = push_field(key);
	if (type == LUA_TNUMBER) value = static_cast<int>(lua_tointeger(_L, -1));
	else if (type != LUA_TNIL) warn_type(key, "숫자");
	lua_pop(_L, 1);
	return value;
}

bool LuaTable::boolean(const char* key, bool def)
{
	bool value = def;
	const int type = push_field(key);
	if (type == LUA_TBOOLEAN) value = lua_toboolean(_L, -1) != 0;
	else if (type != LUA_TNIL) warn_type(key, "true/false");
	lua_pop(_L, 1);
	return value;
}

std::string LuaTable::string(const char* key, const std::string& def)
{
	std::string value = def;
	const int type = push_field(key);
	if (type == LUA_TSTRING) value = lua_tostring(_L, -1);
	else if (type != LUA_TNIL) warn_type(key, "문자열");
	lua_pop(_L, 1);
	return value;
}

namespace
{
	// 스택 맨 위 배열 테이블에서 n개 숫자를 읽음. 숫자가 아닌 원소가 있으면 false
	bool read_numbers(lua_State* L, float* out, int count, int required)
	{
		for (int i = 0; i < count; ++i)
		{
			const int type = lua_rawgeti(L, -1, i + 1);
			if (type == LUA_TNUMBER) out[i] = static_cast<float>(lua_tonumber(L, -1));
			lua_pop(L, 1);
			if (type != LUA_TNUMBER && (i < required || type != LUA_TNIL)) return false;
		}
		return true;
	}
}

XMFLOAT3 LuaTable::vec3(const char* key, const XMFLOAT3& def)
{
	XMFLOAT3 value = def;
	const int type = push_field(key);
	if (type == LUA_TTABLE)
	{
		float v[3];
		if (read_numbers(_L, v, 3, 3)) value = { v[0], v[1], v[2] };
		else warn_type(key, "{ x, y, z }");
	}
	else if (type != LUA_TNIL) warn_type(key, "{ x, y, z }");
	lua_pop(_L, 1);
	return value;
}

XMFLOAT4 LuaTable::color(const char* key, const XMFLOAT4& def)
{
	XMFLOAT4 value = def;
	const int type = push_field(key);
	if (type == LUA_TTABLE)
	{
		float v[4] = { 0, 0, 0, 1 };
		if (read_numbers(_L, v, 4, 3)) value = { v[0], v[1], v[2], v[3] };
		else warn_type(key, "{ r, g, b, a }");
	}
	else if (type != LUA_TNIL) warn_type(key, "{ r, g, b, a }");
	lua_pop(_L, 1);
	return value;
}

XMFLOAT2 LuaTable::range(const char* key, const XMFLOAT2& def)
{
	XMFLOAT2 value = def;
	const int type = push_field(key);
	if (type == LUA_TNUMBER)
	{
		const float n = static_cast<float>(lua_tonumber(_L, -1));
		value = { n, n };
	}
	else if (type == LUA_TTABLE)
	{
		float v[2];
		if (read_numbers(_L, v, 2, 2)) value = { v[0], v[1] };
		else warn_type(key, "숫자 또는 { 최소, 최대 }");
	}
	else if (type != LUA_TNIL) warn_type(key, "숫자 또는 { 최소, 최대 }");
	lua_pop(_L, 1);
	return value;
}

void LuaTable::for_each_array(const char* key, const std::function<void(LuaTable&)>& fn)
{
	const int type = push_field(key);
	if (type == LUA_TTABLE)
	{
		const lua_Integer count = luaL_len(_L, -1);
		for (lua_Integer i = 1; i <= count; ++i)
		{
			if (lua_rawgeti(_L, -1, i) == LUA_TTABLE)
			{
				LuaTable element(_L, -1, _context + "." + key + "[" + std::to_string(i) + "]");
				fn(element);
			}
			else
			{
				CERROR("[Lua] " << _context << "." << key << "[" << i << "]: 테이블이어야 함 (건너뜀)");
			}
			lua_pop(_L, 1);
		}
	}
	else if (type != LUA_TNIL) warn_type(key, "테이블 배열");
	lua_pop(_L, 1);
}

void LuaTable::warn_unknown_keys(const std::set<std::string>& allowed)
{
	lua_pushnil(_L);
	while (lua_next(_L, _index) != 0)
	{
		// 키는 복사본으로 문자열 확인 (lua_next 순회 중 원래 키를 바꾸지 않기 위해)
		lua_pushvalue(_L, -2);
		if (lua_type(_L, -1) == LUA_TSTRING)
		{
			const std::string key = lua_tostring(_L, -1);
			if (!_readKeys.contains(key) && !allowed.contains(key))
				CERROR("[Lua] " << _context << "." << key << ": 알 수 없는 설정 (오타인지 확인)");
		}
		lua_pop(_L, 2); // 키 복사본, 값
	}
}

bool lua_for_each_global_table(lua_State* L, const char* name, const std::function<void(const std::string& key, LuaTable& table)>& fn)
{
	if (lua_getglobal(L, name) != LUA_TTABLE)
	{
		lua_pop(L, 1);
		return false;
	}

	lua_pushnil(L);
	while (lua_next(L, -2) != 0)
	{
		if (lua_type(L, -2) == LUA_TSTRING && lua_type(L, -1) == LUA_TTABLE)
		{
			const std::string key = lua_tostring(L, -2);
			LuaTable table(L, -1, std::string(name) + "." + key);
			fn(key, table);
		}
		lua_pop(L, 1); // 값 (키는 다음 lua_next용으로 남김)
	}
	lua_pop(L, 1); // 전역 테이블
	return true;
}
