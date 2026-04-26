// Particle_CS.hlsl

// C++에서 넘겨줄 상수 데이터 (b0)
cbuffer cbUpdateInfo : register(b0)
{
    matrix g_matWorld; // 현재 무기(소켓)의 월드 행렬
    float3 g_PlayerPos; // 플레이어의 현재 위치 (파티클 스폰 중심점)
    float g_SkillProgress; // 스킬 진행도 (0.0: 스폰 위치 -> 1.0: 검 모양 완성)
};

// C++에서 구워둔 정답지 버퍼 (t0 - 읽기 전용)
StructuredBuffer<float3> TargetBuffer : register(t0);

// 계산된 결과를 저장할 현재 파티클 위치 버퍼 (u0 - 읽기/쓰기 가능)
RWStructuredBuffer<float3> CurrentBuffer : register(u0);

// 스레드 ID 기반의 아주 빠르고 가벼운 랜덤 해시 함수
float3 hash31(float p)
{
    float3 p3 = frac(p * float3(0.1031, 0.1030, 0.0973));
    p3 += dot(p3, p3.yzx + 33.33);
    return frac((p3.xxy + p3.yzz) * p3.zyx) * 2.0 - 1.0;
}

// 256개의 스레드를 1그룹으로 묶어서 병렬 연산
[numthreads(256, 1, 1)]
void CS_Main(uint3 DTid : SV_DispatchThreadID)
{
    uint idx = DTid.x; // 파티클 고유 ID

    if (idx >= 50000)
        return;
    
    // 1. 목표 월드 위치 계산 (로컬 좌표 * 무기 월드 행렬)
    float3 localTarget = TargetBuffer[idx];
    float3 targetWorldPos = mul(float4(localTarget, 1.0f), g_matWorld).xyz;

    // 2. 고유 ID를 이용해 캐릭터 주변 반경 8m 내의 무작위 스폰 위치 생성
    float3 randomOffset = hash31((float) idx) * 8.0f;
    float3 spawnPos = g_PlayerPos + randomOffset;
    spawnPos.y += 3.0f; // 약간 위쪽 허공에서 생성되도록 보정

    // 3. 진행도(0~1)에 따라 부드럽게 모여들도록 보간(Lerp)
    // smoothstep을 쓰면 처음엔 천천히 움직이다가 확 빨려들어가고 마지막에 부드럽게 멈춥니다.
    float t = smoothstep(0.0f, 1.0f, g_SkillProgress);
    
    // 최종 위치 계산
    float3 currentPos = lerp(spawnPos, targetWorldPos, t);

    // 4. 결과 버퍼에 쓰기 (이 버퍼를 나중에 렌더링할 때 씁니다)
    CurrentBuffer[idx] = currentPos;
}