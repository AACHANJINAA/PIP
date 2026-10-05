#pragma once
#include <functional>
#include <set>
#include <string>

struct lua_State;

// 클라이언트 Lua 데이터 파일 읽기 도우미 (이펙트 프리셋 등)
// 서버 LuaManager처럼 Lua 5.4 C API를 직접 쓰되, 값이 없으면 기본값을 돌려주고
// 모르는 키(오타)는 로그로 알린다. 데이터 파일 읽기 전용이라 C++ 함수 등록은 하지 않는다.
class LuaState
{
public:
	LuaState();
	~LuaState();
	LuaState(const LuaState&) = delete;
	LuaState& operator=(const LuaState&) = delete;

	// 파일을 실행. 실패하면 오류를 로그로 남기고 false
	bool do_file(const std::string& path);

	lua_State* get() const { return _L; }

private:
	lua_State* _L = nullptr;
};

// 스택에 올라 있는 테이블 하나를 읽는 동안만 유효한 읽기 도구
// 읽은 키를 기록해 두었다가 warn_unknown_keys()로 읽지 않은 키(오타 등)를 알린다
class LuaTable
{
public:
	// index: 테이블의 스택 위치(음수 가능), context: 로그에 쓸 이름 (예: "vfx.hit_spark")
	LuaTable(lua_State* L, int index, std::string context);

	const std::string& context() const { return _context; }
	bool has(const char* key);

	float number(const char* key, float def);
	int integer(const char* key, int def);
	bool boolean(const char* key, bool def);
	std::string string(const char* key, const std::string& def);
	// { x, y, z } 배열
	XMFLOAT3 vec3(const char* key, const XMFLOAT3& def);
	// { r, g, b, a } 배열 (a 생략 시 1)
	XMFLOAT4 color(const char* key, const XMFLOAT4& def);
	// 숫자 하나면 (n, n), { 최소, 최대 } 배열이면 그 범위
	XMFLOAT2 range(const char* key, const XMFLOAT2& def);

	// key가 테이블 배열이면 각 원소(테이블)마다 fn 호출
	void for_each_array(const char* key, const std::function<void(LuaTable&)>& fn);

	// 이 테이블에서 읽지 않은 키를 로그로 알림 (allowed: 읽지 않아도 되는 키)
	void warn_unknown_keys(const std::set<std::string>& allowed = {});

private:
	// key 값을 스택 맨 위에 올림 (호출한 쪽이 pop)
	int push_field(const char* key);
	void warn_type(const char* key, const char* expected);

	lua_State* _L;
	int _index;	// 절대 스택 위치
	std::string _context;
	std::set<std::string> _readKeys;
};

// 전역 테이블 name의 문자열 키마다 (키, 값 테이블)로 fn 호출. 전역이 테이블이 아니면 false
bool lua_for_each_global_table(lua_State* L, const char* name, const std::function<void(const std::string& key, LuaTable& table)>& fn);
