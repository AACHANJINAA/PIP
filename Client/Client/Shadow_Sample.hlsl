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
float sample_csm_shadow(float3 worldPos, float3 normal, float viewDepth)
{
    // 1. 카메라 거리로 cascade 선택
    int cascade;
    if (viewDepth < g_shadowSplitNear)
        cascade = 0;
    else if (viewDepth < g_shadowSplitMid)
        cascade = 1;
    else
        cascade = 2;
    
    float3 offsetWorldPos = worldPos + (normal * 0.1f);
    
    // 2. 선택된 cascade의 light space 좌표로 변환
    float4 sp = mul(float4(offsetWorldPos, 1.0f), g_shadowLightVP[cascade]);
    sp.xyz /= sp.w;

    // 3. NDC → UV (Y축 반전)
    float2 uv;
    uv.x = sp.x * 0.5f + 0.5f;
    uv.y = -sp.y * 0.5f + 0.5f;

    // 4. shadow map 범위 밖이면 그림자 없음
    if (uv.x < 0.0f || uv.x > 1.0f || uv.y < 0.0f || uv.y > 1.0f)
        return 1.0f;

    // 5. 3x3 PCF (Percentage Closer Filtering) 적용
    float shadow = 0.0f;

    // 쉐도우 맵 해상도(현재 1024)의 역수를 계산하여 텍셀 크기를 구합니다.
    float2 texelSize = 1.0f / 1024.0f;

    // 현재 픽셀 주변 3x3 영역을 샘플링합니다.
    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
    [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float2 offset = float2(x, y) * texelSize;

        // SampleCmpLevelZero는 하드웨어 비교 기능을 사용하여 0.0(그림자) 또는 1.0(빛)을 반환합니다.
        // (하드웨어 설정에 따라 중간값인 0.5 등이 반환될 수도 있어 부드러워집니다.)
            shadow += g_shadowMap.SampleCmpLevelZero(
            g_shadowSampler,
            float3(uv + offset, (float) cascade),
            sp.z - g_shadowBias
        );
        }
    }

    // 9개 샘플의 평균을 내어 최종 그림자 강도를 결정 (0.0 ~ 1.0)
    return shadow / 9.0f;
}