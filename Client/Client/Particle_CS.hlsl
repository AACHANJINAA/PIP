// Particle_CS.hlsl

// C++에서 넘겨줄 상수 데이터 (b0)
cbuffer cbUpdateInfo : register(b0)
{
    matrix g_matWorld; // 현재 무기(소켓)의 월드 행렬
    float3 g_PlayerPos; // 플레이어의 현재 위치 (파티클 스폰 중심점)
    float g_SkillProgress; // 스킬 진행도 (0.0: 스폰 위치 -> 1.0: 검 모양 완성)
    float g_DyingProgress; // 파티클 사라지는 연출 여부 (0.0: 생존, 1.0: 사라짐)
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

// 폭발 후 응집 (가슴에서 사방으로 팡 터진 후, 대검으로 일제히 빨려 들어감)
float3 Effect_BurstAndGather(uint idx, float3 spawnPos, float3 targetPos, float progress)
{
    // 1. 코어(가슴팍) 위치 설정
    float3 corePos = g_PlayerPos + float3(0.0f, 1.2f, 0.0f);
    
    // 2. 구형(Sphere) 팽창 목표 위치 계산
    float3 randDir = normalize(hash31((float) idx));
    float randRadius = 1.5f + frac(sin((float) idx * 12.345f) * 4567.89f) * 3.0f;
    float3 burstPos = corePos + randDir * randRadius;

    // -----------------------------------------------------------
    // [Phase 1: 폭발] (전체 시간의 0% ~ 30% 구간)
    // -----------------------------------------------------------
    float burstProgress = saturate(progress / 0.3f);
    float burstT = 1.0f - pow(1.0f - burstProgress, 3.0f); // Ease-Out (팍 터지고 스무스하게 멈춤)

    // -----------------------------------------------------------
    // [Phase 2: 응집] (전체 시간의 30% ~ 100% 구간)
    // -----------------------------------------------------------
    float gatherDelay = frac(cos((float) idx * 78.91f) * 1234.56f) * 0.2f;
    float gatherProgress = saturate((progress - 0.3f - gatherDelay) / (0.7f - gatherDelay));
    float gatherT = pow(gatherProgress, 2.0f); // Ease-In (서서히 출발해서 팍 꽂힘)

    // -----------------------------------------------------------
    // 4. 위치 합성
    // -----------------------------------------------------------
    // 먼저 코어에서 폭발 위치로 쏨
    float3 currentPos = lerp(corePos, burstPos, burstT);
    
    // 체공 중인 상태에서 최종 대검 위치로 빨려 들어감
    currentPos = lerp(currentPos, targetPos, gatherT);

    return currentPos;
}

// 256개의 스레드를 1그룹으로 묶어서 병렬 연산
[numthreads(256, 1, 1)]
void CS_Main(uint3 DTid : SV_DispatchThreadID)
{
    
    uint idx = DTid.x;

    if (idx >= 50000)
        return;
    
    // 1. 목표 월드 위치 계산
    float3 localTarget = TargetBuffer[idx];
    float3 targetWorldPos = mul(float4(localTarget, 1.0f), g_matWorld).xyz;

    // 2. 기본 스폰 위치 계산 (플레이어 주변의 둥근 구 형태)
    float3 randomVec = hash31((float) idx);
    float randomRadius = frac(sin((float) idx * 12.345f) * 4567.89f) * 8.0f;
    float3 spawnPos = g_PlayerPos + normalize(randomVec) * randomRadius;
    spawnPos.y += 3.0f;

    float3 finalPos;
    
    // =========================================================
    // [테스트 구역]
    // =========================================================
     finalPos = Effect_BurstAndGather(idx, spawnPos, targetWorldPos, g_SkillProgress); // 10번 연출
    
    // =========================================================

    // =========================================================
    // 파티클 산화(사라짐) 연출 - 오른쪽 하늘로 날아감
    // =========================================================
    if (g_DyingProgress > 0.0f)
    {
        // 날아갈 기본 방향 (우상단) + 파티클마다 약간 다른 방향으로 퍼지게 난수 추가
        float3 driftDir = float3(2.0f, 4.0f, 1.0f) + (hash31((float) idx) * 1.5f);
        
        // 처음엔 천천히, 나중엔 빠르게 날아가도록 진행도를 제곱(pow) 처리
        float moveT = pow(g_DyingProgress, 1.5f);
        
        // 시간(moveT)에 비례해서 파티클 위치를 하늘로 이동시킴 (최대 5배속)
        finalPos += driftDir * moveT * 5.0f;
    }
    
    
    // 결과 저장
    CurrentBuffer[idx] = finalPos;
    
    
    
   // 기존 파티클 스폰 + 흡입 연출 (폭발+응집 효과보다 훨씬 단순한 버전)
  //{  uint idx = DTid.x; // 파티클 고유 ID

  //  if (idx >= 50000)
  //      return;
    
  //  // 1. 목표 월드 위치 계산 (로컬 좌표 * 무기 월드 행렬)
  //  float3 localTarget = TargetBuffer[idx];
  //  float3 targetWorldPos = mul(float4(localTarget, 1.0f), g_matWorld).xyz;

  //  // 2. 고유 ID를 이용해 캐릭터 주변 반경 8m 내의 무작위 스폰 위치 생성
  //  float3 randomOffset = hash31((float) idx) * 8.0f;
  //  float3 spawnPos = g_PlayerPos + randomOffset;
  //  spawnPos.y += 3.0f; // 약간 위쪽 허공에서 생성되도록 보정

  //  // 3. 진행도(0~1)에 따라 부드럽게 모여들도록 보간(Lerp)
  //  // smoothstep을 쓰면 처음엔 천천히 움직이다가 확 빨려들어가고 마지막에 부드럽게 멈춥니다.
  //  float t = smoothstep(0.0f, 1.0f, g_SkillProgress);
    
  //  // 최종 위치 계산
  //  float3 currentPos = lerp(spawnPos, targetWorldPos, t);

  //  // 4. 결과 버퍼에 쓰기 (이 버퍼를 나중에 렌더링할 때 씁니다)
  //  CurrentBuffer[idx] = currentPos;
  //}

    
    // 회오리 치는 파티클
    
    //uint idx = DTid.x;
    //if (idx >= 50000)
    //    return;

    //float3 localTarget = TargetBuffer[idx];
    //float3 targetWorldPos = mul(float4(localTarget, 1.0f), g_matWorld).xyz;
    
    //// 1. 초기 위치에 약간의 소용돌이 벡터 추가
    //float3 randomOffset = hash31((float) idx) * 8.0f;
    //float3 spawnPos = g_PlayerPos + randomOffset;
    //spawnPos.y += 3.0f;

    //// 2. 진행도에 따른 나선 효과 (Spiral)
    //// 검의 중심축(위 방향)을 기준으로 회전시킵니다.
    //float angle = g_SkillProgress * 15.0f + (idx * 0.1f); // 진행될수록 더 많이 회전
    //float radius = (1.0f - g_SkillProgress) * 5.0f; // 검에 가까워질수록 회전 반경 감소
    
    //float3 spiral;
    //spiral.x = cos(angle) * radius;
    //spiral.z = sin(angle) * radius;
    //spiral.y = (idx % 10) * 0.2f * (1.0f - g_SkillProgress); // 높이에도 약간의 변동

    //float individualProgress = clamp(g_SkillProgress * 1.2f - (idx % 100) * 0.002f, 0.0f, 1.0f);
    //float t = smoothstep(0.0f, 1.0f, individualProgress);
    
    //// 직선 보간 대신 나선 오프셋을 더함
    //float3 currentPos = lerp(spawnPos, targetWorldPos, t) + spiral;
    
    //CurrentBuffer[idx] = currentPos;
}