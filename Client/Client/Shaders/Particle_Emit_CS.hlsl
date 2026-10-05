// Particle_Emit_CS.hlsl
// 범용 파티클 방출: 이번 프레임 새 파티클 하나당 스레드 하나 (ParticleSystem_Plan_KR.md 3.2)
// 스레드는 자기 번호로 방출 요청을 찾고, 요청 시드 + 번호로 만든 난수와 방출기 설정으로 초기값을 만든다.

#include "Particle_Common.hlsli"

StructuredBuffer<EmitRequest> g_Requests : register(t0);
RWStructuredBuffer<Particle> g_Pool : register(u0);

[numthreads(64, 1, 1)]
void CS_Emit(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= g_NewParticleCount) return;

    // 이 스레드가 속한 요청 찾기 (요청 수가 적어 순차 탐색)
    uint r = 0;
    for (uint i = 0; i < g_RequestCount; ++i)
    {
        if (id.x >= g_Requests[i].firstThread && id.x < g_Requests[i].firstThread + g_Requests[i].count)
        {
            r = i;
            break;
        }
    }
    EmitRequest req = g_Requests[r];
    uint local = id.x - req.firstThread;
    uint state = pcg_hash(req.seed ^ (local * 2654435761u));

    // 방출 방향 축과 수직 기저
    float3 axis = dot(req.dir, req.dir) > 1e-8f ? normalize(req.dir) : float3(0, 1, 0);
    float3 helper = abs(axis.y) < 0.99f ? float3(0, 1, 0) : float3(1, 0, 0);
    float3 t1 = normalize(cross(helper, axis));
    float3 t2 = cross(axis, t1);

    float3 pos = req.pos;
    float3 dir = axis;
    switch (g_Shape)
    {
    case 0: // Point
        dir = rand_unit_vector(state);
        break;
    case 1: // Sphere
        dir = rand_unit_vector(state);
        pos += dir * g_Radius * pow(rand01(state), 1.0f / 3.0f);
        break;
    case 2: // Hemisphere
        dir = rand_unit_vector(state);
        if (dot(dir, axis) < 0.0f) dir = -dir;
        pos += dir * g_Radius * pow(rand01(state), 1.0f / 3.0f);
        break;
    case 3: // Cone
    {
        float cosTheta = lerp(cos(g_Angle), 1.0f, rand01(state));
        float sinTheta = sqrt(saturate(1.0f - cosTheta * cosTheta));
        float phi = rand01(state) * 6.2831853f;
        dir = axis * cosTheta + (t1 * cos(phi) + t2 * sin(phi)) * sinTheta;
        float phi2 = rand01(state) * 6.2831853f;
        pos += (t1 * cos(phi2) + t2 * sin(phi2)) * g_Radius * sqrt(rand01(state));
        break;
    }
    default: // 4 Edge
        pos += t1 * (rand01(state) - 0.5f) * g_EdgeLength;
        break;
    }

    Particle p;
    p.pos = pos;
    p.age = 0.0f;
    p.vel = dir * rand_range(state, g_Speed);
    p.lifetime = max(rand_range(state, g_Lifetime), 1e-3f);
    p.size = rand_range(state, g_Size);
    p.rotation = rand_range(state, g_Rotation);
    p.seed = state;
    p.color = pack_color(lerp(g_StartColorA, g_StartColorB, rand01(state)));

    g_Pool[(req.poolStart + local) % g_MaxParticles] = p;
}
