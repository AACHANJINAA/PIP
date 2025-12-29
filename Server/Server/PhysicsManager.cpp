#include "pch.h"
#include "PhysicsManager.h"


static void TraceImpl(const char* inFMT, ...)
{
	va_list list;
	va_start(list, inFMT);
	char buffer[1024];
	vsnprintf(buffer, sizeof(buffer), inFMT, list);
	va_end(list);
	MYLOG(buffer); // 가변 인자가 적용된 문자열 출력
}
static bool AssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, JPH::uint inLine)
{
	// ASSERT 실패 시 처리
	MYERROR("Jolt Assert Failed: Expression: {}, Message: {}, File: {}, Line: {}", inExpression, inMessage, inFile, inLine);
	return true; // true면 브레이크포인트
}

namespace PIP
{
	

	void PhysicsManager::Initialize()
	{
		if (_isInitialized) return;

		// 콜백 등록
		JPH::RegisterDefaultAllocator(); // 메모리 할당자 등록
										 // * 기본: RegisterDefaultAllocator() -> 그냥 malloc/new 씀. (편함)
										 // *심화: 게임 전용 메모리 풀(Memory Pool)이나 스택 할당기를 연결해서 성능을 극도로 쥐어짤 수 있음.
		JPH::Trace = TraceImpl; // 로그 출력 콜백 등록 Jolt가 "이거 이상해"라고 말하면 MYLOG로 찍어줌.
		JPH::AssertFailed = AssertFailedImpl; // ASSERT 실패 콜백 등록 Jolt가 치명적인 오류를 발견하면 MYERROR로 찍고 브레이크포인트를 걸어줌.

		// 팩토리 및 타입 등록 (가장 중요)
		JPH::Factory::sInstance = new JPH::Factory(); // 물리 객체 설계도 공장 생성
		JPH::RegisterTypes(); // 기본 물리 객체 타입(구, 박스, 캡슐) 등록

		_isInitialized = true;
		MYLOG("[PhysicsManager] Jolt Initialized." << std::endl);
	}
	void PhysicsManager::Shutdown()
	{
		if (!_isInitialized) return;

		// 해제
		delete JPH::Factory::sInstance;
		JPH::Factory::sInstance = nullptr;

		_isInitialized = false;
	}
}
