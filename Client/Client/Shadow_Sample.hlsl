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

// worldPos: 픽셀의 월드 좌표, viewDepth: 카메라 기준 view-space Z
float sample_csm_shadow(float3 worldPos, float viewDepth)
{
    // 1. 카메라 거리로 cascade 선택
    int cascade;
    if (viewDepth < g_shadowSplitNear)
        cascade = 0;
    else if (viewDepth < g_shadowSplitMid)
        cascade = 1;
    else
        cascade = 2;

    // 2. 선택된 cascade의 light space 좌표로 변환
    float4 sp = mul(float4(worldPos, 1.0f), g_shadowLightVP[cascade]);
    sp.xyz /= sp.w;

    // 3. NDC → UV (Y축 반전)
    float2 uv;
    uv.x = sp.x * 0.5f + 0.5f;
    uv.y = -sp.y * 0.5f + 0.5f;

    // 4. shadow map 범위 밖이면 그림자 없음
    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f)
    return 1.0f;

    // 5. 단순 1샘플 비교 (PCF 없음)
    return g_shadowMap.SampleCmpLevelZero(g_shadowSampler, float3(uv, (float) cascade),sp.z - g_shadowBias);
}