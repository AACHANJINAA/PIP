#pragma once
#include <cstdint>
#include <type_traits>

namespace ECS
{
	// detail: 바깥에서 직접 부르지 않는 내부 구현이라는 관례상 이름입니다. type_id<T>()만 쓰세요.
	// - 익명 namespace(namespace { })를 쓰면 안 됩니다. 익명 namespace 안의 것은 static과 같은 내부 연결이라
	//   .cpp마다 next_type_id와 cnt가 따로 생깁니다 (inline을 붙여도 마찬가지). 그러면 서로 다른 타입이 같은 번호를 받을 수 있고,
	//   링커가 같은 이름의 type_id<T>를 합쳐 우연히 맞기도 해서 결과가 링커에 따라 달라집니다 (ODR 위반).
	// - 이름 있는 namespace + inline이면 링커가 모든 .cpp의 정의를 하나로 합쳐서 cnt가 프로그램 전체에 하나만 존재합니다.
	// - next_type_id()를 직접 부르면 번호가 하나 건너뛰므로 detail 안에 둡니다.
	namespace detail
	{
		// 모든 타입이 공유하는 번호 발급기. 처음 불린 순서대로 0, 1, 2...
		// 번호는 실행마다 달라질 수 있으므로 파일 저장·패킷 전송에 쓰면 안 됩니다 (메모리 안 색인 용도).
		// 단일 스레드 전제: cnt++는 원자적이지 않습니다 (필요하면 std::atomic<uint32_t>).
		inline uint32_t next_type_id()
		{
			static uint32_t cnt = 0;
			return cnt++;
		}

		// 함수 템플릿은 T마다 별개의 함수가 만들어지므로 static id도 T마다 하나씩 생깁니다.
		// 처음 호출될 때 한 번만 next_type_id()로 번호를 받고, 이후에는 저장된 번호를 돌려줍니다.
		// 템플릿이라 여러 .cpp에 정의돼도 프로그램 전체에서 하나로 합쳐집니다.
		template<typename T>
		uint32_t type_id_impl()
		{
			static const uint32_t id = next_type_id();
			return id;
		}
	}

	template<typename T>
	inline uint32_t type_id() { return detail::type_id_impl<std::remove_cvref_t<T>>();}
	// std::remove_cvref_t<T>는 T에서 const, volatile, reference를 제거한 타입을 반환합니다.
	// 예를 들어, T가 const int&이면 std::remove_cvref_t<T>는 int가 됩니다.
	// 이렇게 하면 type_id<T>()가 const int&, volatile int, int 등과 같은 타입에 대해 동일한 ID를 반환하도록 보장할 수 있습니다.
	// 원래 형태 typename std::remove_cvref<T>::type -> std::remove_cvref_t<T>로 C++20에서 간단히 쓸 수 있습니다.
}
