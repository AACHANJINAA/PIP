// Particle_Update_CS.hlsl
// 범용 파티클 갱신: 풀 전체를 돌며 살아 있는 파티클의 나이·속도·위치를 진행 (ParticleSystem_Plan_KR.md 3.2)

#include "Particle_Common.hlsli"

RWStructuredBuffer<Particle> g_Pool : register(u0);

[numthreads(64, 1, 1)]
void CS_Update(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= g_MaxParticles) return;

    Particle p = g_Pool[id.x];
    if (g_ClearAll != 0)
    {
        p.lifetime = 0.0f;
        g_Pool[id.x] = p;
        return;
    }
    if (!particle_alive(p)) return;

    const float dt = g_DeltaTime;
    p.vel.y += g_Gravity * dt;
    p.vel /= (1.0f + g_Drag * dt);  // 공기 저항: 초당 drag 비율로 감쇠
    p.pos += p.vel * dt;
    p.age += dt;

    g_Pool[id.x] = p;
}
