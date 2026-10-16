#pragma once
// 테스트용 컴포넌트 (main.cpp, SecondTU.cpp가 같이 쓴다: 두 번역 단위에서 같은 타입이어야 type_id를 비교할 수 있음)
#include <cstdint>
#include <memory>
#include <string>

struct Health { int cur; int max; };				// 생성자 없는 struct (C++20 괄호 집합체 초기화)
struct Dead {};										// 빈 태그
struct Name { std::string value; };					// 이동 시 내용이 옮겨지는 타입
struct Owned { std::unique_ptr<int> ptr; };			// 이동만 되는 타입 (복사 불가)
struct Position { float x, y, z; };					// type_id 교차 확인에서 SecondTU가 먼저 번호를 받게 하는 타입

// SecondTU.cpp가 받은 type_id 번호 (main.cpp와 비교)
struct SecondTuIds { uint32_t position, dead, health, onlyHere; };
SecondTuIds second_tu_type_ids();
int second_tu_smoke();
