// Particle_Common.hlsli
// 범용 파티클 공용 정의 (ParticleSystemComponent, ParticleSystem_Plan_KR.md 3.2)
// C++ 쪽 구조체(ParticleSystemComponent.cpp의 GpuParticle, EmitRequest, EmitterConstants)와 배치가 같아야 한다.

#ifndef PARTICLE_EMITTER_REGISTER
#define PARTICLE_EMITTER_REGISTER b0
#endif

// 파티클 하나 (48바이트). 수명 0은 빈 자리(버퍼는 0으로 시작), 나이 >= 수명이면 죽음
struct Particle
{
    float3 pos;
    float age;
    float3 vel;
    float lifetime;
    float size;
    float rotation;     // 라디안
    uint seed;
    uint color;         // RGBA8
};

// 방출 요청 (48바이트). CPU가 풀 시작 위치와 이번 프레임 스레드 시작 번호를 정해 둔다
struct EmitRequest
{
    float3 pos;
    uint poolStart;
    float3 dir;
    uint count;
    uint seed;
    uint firstThread;
    uint2 pad;
};

// 방출기 설정 + 이번 프레임 값 (방출·갱신 컴퓨트와 빌보드가 같이 씀)
cbuffer ParticleEmitter : register(PARTICLE_EMITTER_REGISTER)
{
    float4 g_StartColorA;
    float4 g_StartColorB;
    float4 g_EndColor;
    float2 g_Lifetime;
    float2 g_Speed;
    float2 g_Size;
    float2 g_Rotation;          // 라디안
    float g_Gravity;            // m/s² (아래 방향이면 음수)
    float g_Drag;
    float g_EndSizeMultiplier;
    float g_LengthScale;
    uint g_Shape;
    float g_Radius;
    float g_Angle;              // 라디안
    float g_EdgeLength;
    uint g_MaxParticles;
    uint g_RenderMode;
    float g_DeltaTime;
    uint g_RequestCount;
    uint g_NewParticleCount;
    uint g_ClearAll;            // 1이면 갱신 컴퓨트가 모든 파티클을 지움 (clear())
    uint2 g_Pad;
};

bool particle_alive(Particle p)
{
    return p.lifetime > 0.0f && p.age < p.lifetime;
}

// PCG 해시 난수 (같은 시드면 같은 결과)
uint pcg_hash(uint v)
{
    uint state = v * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float rand01(inout uint state)
{
    state = pcg_hash(state);
    return (state & 0x00FFFFFFu) / 16777216.0f;
}

float rand_range(inout uint state, float2 range)
{
    return lerp(range.x, range.y, rand01(state));
}

float3 rand_unit_vector(inout uint state)
{
    float z = rand01(state) * 2.0f - 1.0f;
    float phi = rand01(state) * 6.2831853f;
    float r = sqrt(saturate(1.0f - z * z));
    return float3(r * cos(phi), r * sin(phi), z);
}

uint pack_color(float4 c)
{
    uint4 u = (uint4)round(saturate(c) * 255.0f);
    return u.r | (u.g << 8) | (u.b << 16) | (u.a << 24);
}

float4 unpack_color(uint c)
{
    return float4(c & 0xFF, (c >> 8) & 0xFF, (c >> 16) & 0xFF, (c >> 24) & 0xFF) / 255.0f;
}
