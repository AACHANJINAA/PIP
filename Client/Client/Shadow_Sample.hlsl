// 라이팅 패스에서 include 해서 사용
// register(b5): 그림자 관련 상수 버퍼
cbuffer cbShadow : register(b5)
{
    matrix g_shadowLightVP[3]; // 각 cascade 의 LightViewProjection
    float g_shadowSplitNear; // view-space Z: cascade 0→1 경계
    float g_shadowSplitMid; // view-space Z: cascade 1→2 경계
    float g_shadowBias; // z-fighting 방지
    float g_shadowPad; // 16byte 패딩
};

Texture2DArray g_shadowMap : register(t11);
SamplerComparisonState g_shadowSampler : register(s1);

// 1. Poisson Disk 오프셋 상수 배열 (미리 계산된 불규칙한 원형 배치) -> 픽셀 사이 일정한 거리유지
static const float2 PoissonDisk[9] =
{
    float2(0.0f, 0.0f),
    float2(0.27f, -0.62f), float2(-0.84f, 0.22f),
    float2(0.39f, 0.69f), float2(-0.16f, -0.92f),
    float2(0.96f, -0.19f), float2(-0.64f, -0.63f),
    float2(-0.73f, 0.66f), float2(0.66f, 0.59f)
};

// 2. 개선된 PCF 샘플링 함수 (해상도 동적 대응 + 푸아송 디스크 + 화면 노이즈 회전)
float get_pcf_shadow_advanced(float3 worldPos, float3 normal, int cascade, float2 screenPos)
{
    float normalOffsets[3] = { 0.05f, 0.1f, 0.2f };
	float3 offsetWorldPos = worldPos + (normal * normalOffsets[cascade]);
    float4 sp = mul(float4(offsetWorldPos, 1.0f), g_shadowLightVP[cascade]);
    sp.xyz /= sp.w;

    float2 uv;
    uv.x = sp.x * 0.5f + 0.5f;
    uv.y = -sp.y * 0.5f + 0.5f;

    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f)
        return 1.0f;

    // 동적으로 텍스처 해상도 가져오기
    uint width, height, elements;
    g_shadowMap.GetDimensions(width, height, elements);
    float2 texelSize = 1.0f / float2(width, height);

    // IGN (Interleaved Gradient Noise) 기반 랜덤 회전값 생성 -> 시각적으로 더 자연스러운 노이즈 패턴
    float noise = frac(52.9829189f * frac(dot(screenPos, float2(0.06711056f, 0.00583715f))));
    float angle = noise * 6.2831853f; // 2 * PI
    
    // 회전 행렬 구성
    float s, c;
    sincos(angle, s, c);
    float2x2 rotationMat = float2x2(c, -s, s, c);

    float shadow = 0.0f;
    float filterRadii[3] = { 0.8f, 1.5f, 2.5f }; // Cascade 0: 매우 작게, 2: 크게
    float filterRadius = filterRadii[cascade];

    // 3x3 격자 대신 회전된 푸아송 디스크 샘플링
    [unroll]
    for (int i = 0; i < 9; ++i)
    {
        // 디스크 오프셋을 픽셀 위치 기반으로 랜덤하게 회전시킴
        float2 rotatedOffset = mul(PoissonDisk[i], rotationMat);
        float2 offset = rotatedOffset * texelSize * filterRadius;
        
        shadow += g_shadowMap.SampleCmpLevelZero(
            g_shadowSampler,
            float3(uv + offset, (float) cascade),
            sp.z - g_shadowBias
        );
    }
    
    return shadow / 9.0f;
}

//float get_pcf_shadow_simple(float3 worldPos, float3 normal, int cascade)
//{
//    float3 offsetWorldPos = worldPos + (normal * 0.05f); // Normal Offset 줄임
//    float4 sp = mul(float4(offsetWorldPos, 1.0f), g_shadowLightVP[cascade]);
//    sp.xyz /= sp.w;

//    float2 uv;
//    uv.x = sp.x * 0.5f + 0.5f;
//    uv.y = -sp.y * 0.5f + 0.5f;

//    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f)
//        return 1.0f;

//         // 단일 샘플 (보간 없음)
//    float shadow = g_shadowMap.SampleCmpLevelZero(
//             g_shadowSampler,
//             float3(uv, (float) cascade),
//             sp.z - g_shadowBias
//         );

//    return shadow;
//}

// 2. 메인 CSM 샘플링 함수 (블렌딩 적용)
float sample_csm_shadow(float3 worldPos, float3 normal, float viewDepth, float2 screenPos)
{
    float blendThreshold = 15.0f;

    float shadow0 = 1.0f;
    float shadow1 = 1.0f;
    float finalShadow = 1.0f;

    // Cascade 0 -> 1 경계 확인
    if (viewDepth < g_shadowSplitNear + blendThreshold)
    {
        shadow0 = get_pcf_shadow_advanced(worldPos, normal, 0, screenPos);

        if (viewDepth > g_shadowSplitNear - blendThreshold)
        {
            shadow1 = get_pcf_shadow_advanced(worldPos, normal, 1, screenPos);
            float t = (viewDepth - (g_shadowSplitNear - blendThreshold)) / (blendThreshold);
            finalShadow = lerp(shadow0, shadow1, saturate(t));
        }
        else
        {
            finalShadow = shadow0;
        }
    }
    // Cascade 1 -> 2 경계 확인
    else
    {
        shadow1 = get_pcf_shadow_advanced(worldPos, normal, 1, screenPos);

        if (viewDepth > g_shadowSplitMid - blendThreshold)
        {
            float shadow2 = get_pcf_shadow_advanced(worldPos, normal, 2, screenPos);
            float t = (viewDepth - (g_shadowSplitMid - blendThreshold)) / (blendThreshold * 2.0f);
            finalShadow = lerp(shadow1, shadow2, saturate(t));
        }
        else
        {
            finalShadow = shadow1;
        }
    }
    
    return lerp(0.1f, 1.0f, finalShadow);
}