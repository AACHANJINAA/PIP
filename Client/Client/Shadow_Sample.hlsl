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

// 1. 특정 Cascade에서 3x3 PCF 그림자를 가져오는 보조 함수 (코드 중복 방지)
float get_pcf_shadow(float3 worldPos, float3 normal, int cascade)
{
    float3 offsetWorldPos = worldPos + (normal * 0.1f); // 아까 조정한 Normal Offset
    float4 sp = mul(float4(offsetWorldPos, 1.0f), g_shadowLightVP[cascade]);
    sp.xyz /= sp.w;

    float2 uv;
    uv.x = sp.x * 0.5f + 0.5f;
    uv.y = -sp.y * 0.5f + 0.5f;

    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f)
        return 1.0f;

    float shadow = 0.0f;
    float2 texelSize = 1.0f / 1024.0f;

    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float2 offset = float2(x, y) * texelSize;
            shadow += g_shadowMap.SampleCmpLevelZero(
                g_shadowSampler,
                float3(uv + offset, (float) cascade),
                sp.z - g_shadowBias
            );
        }
    }
    return shadow / 9.0f;
}

// 2. 메인 CSM 샘플링 함수 (블렌딩 적용)
float sample_csm_shadow(float3 worldPos, float3 normal, float viewDepth)
{
    // viewDepth가 경계값에 가까워질 때 블렌딩을 시작
    float blendThreshold = 15.0f; // 이 수치를 조절하여 경계의 부드러움 조절 가능

    float shadow0 = 1.0f;
    float shadow1 = 1.0f;
    float finalShadow = 1.0f;

    // Cascade 0 -> 1 경계 확인
    if (viewDepth < g_shadowSplitNear + blendThreshold)
    {
        shadow0 = get_pcf_shadow(worldPos, normal, 0);

        // 블렌딩 구간 진입 (SplitNear 근처)
        if (viewDepth > g_shadowSplitNear - blendThreshold)
        {
            shadow1 = get_pcf_shadow(worldPos, normal, 1);
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
        shadow1 = get_pcf_shadow(worldPos, normal, 1);

        if (viewDepth > g_shadowSplitMid - blendThreshold)
        {
            float shadow2 = get_pcf_shadow(worldPos, normal, 2);
            float t = (viewDepth - (g_shadowSplitMid - blendThreshold)) / (blendThreshold * 2.0f);
            finalShadow = lerp(shadow1, shadow2, saturate(t));
        }
        else
        {
            finalShadow = shadow1;
        }
    }

    // 그림자 농도 조절 (0.3 ~ 1.0)
    return lerp(0.2f, 1.0f, finalShadow);
}