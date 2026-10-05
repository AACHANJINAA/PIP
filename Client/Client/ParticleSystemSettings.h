#pragma once
#include <vector>

// 범용 파티클 설정 (ParticleSystem_Plan_KR.md 3.3). 이름·단위는 유니티 ParticleSystem 모듈을 따른다.
// 범위 값(XMFLOAT2)은 { 최소, 최대 } 사이 무작위. 같은 값이면 고정값.

enum class ParticleShape : uint32_t
{
	Point = 0,		// 한 점에서 모든 방향
	Sphere = 1,		// 구 안에서 바깥 방향 (radius)
	Hemisphere = 2,	// 방출 방향 쪽 반구
	Cone = 3,		// 방출 방향 축으로 angle만큼 퍼짐, 시작 원 반지름 radius
	Edge = 4,		// 방출 방향에 수직인 선분(length) 위에서 방출 방향으로
};

enum class ParticleBlend : uint32_t
{
	Additive = 0,	// 겹칠수록 밝아짐 (불꽃, 스파크)
	Alpha = 1,		// 일반 반투명 (먼지, 연기)
};

enum class ParticleRenderMode : uint32_t
{
	Billboard = 0,			// 카메라를 향한 사각형
	StretchedBillboard = 1,	// 속도 방향으로 늘린 사각형 (스파크, 불똥)
};

struct ParticleBurst
{
	float time = 0.0f;	// 재생 시작 후 초
	int count = 0;
};

struct ParticleSystemSettings
{
	// --- Main ---
	float duration = 5.0f;				// 한 주기 길이 (초)
	bool looping = true;				// 주기가 끝나면 처음부터 반복
	bool play_on_awake = true;			// 붙자마자 재생
	XMFLOAT2 start_lifetime = { 1.0f, 1.0f };	// 초
	XMFLOAT2 start_speed = { 5.0f, 5.0f };		// m/s
	XMFLOAT2 start_size = { 0.1f, 0.1f };		// m
	XMFLOAT2 start_rotation = { 0.0f, 0.0f };	// 도 (Billboard에서 사각형 회전)
	XMFLOAT4 start_color = { 1, 1, 1, 1 };
	XMFLOAT4 start_color_2 = { 1, 1, 1, 1 };	// start_color와 이 색 사이 무작위 (같으면 단색)
	float gravity_modifier = 0.0f;		// 1이면 중력 9.81 m/s²
	float drag = 0.0f;					// 공기 저항 (초당 속도 감쇠 비율)
	uint32_t max_particles = 2048;		// 동시에 살아 있을 수 있는 최대 수. 꽉 차면 새로 만들지 않음 (처음 재생 후 변경 불가)

	// --- Emission ---
	float rate_over_time = 10.0f;		// 초당 방출 수
	std::vector<ParticleBurst> bursts;	// 정해진 시각에 한 번에 방출

	// --- Shape ---
	ParticleShape shape = ParticleShape::Cone;
	float radius = 0.0f;				// m
	float angle = 25.0f;				// 도 (Cone)
	float length = 1.0f;				// m (Edge)

	// --- Over lifetime ---
	XMFLOAT4 end_color = { 1, 1, 1, 0 };	// 수명 끝의 색 (시작 색에서 선형으로 바뀜)
	float end_size_multiplier = 1.0f;		// 수명 끝의 크기 배율

	// --- Renderer ---
	ParticleBlend blend = ParticleBlend::Additive;
	ParticleRenderMode render_mode = ParticleRenderMode::Billboard;
	float length_scale = 0.03f;			// StretchedBillboard 길이 = 크기 + 속력 × length_scale (초)
};
