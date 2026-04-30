// 라이팅 패스에서 include 해서 사용
// register(b5): 그림자 관련 상수 버퍼
cbuffer cbShadow : register(b5)
{
    matrix g_shadowLightVP[2];
    float g_shadowSplitDist;
    float g_shadowBias;
    float g_shadowPad[2];
};

Texture2DArray g_shadowMap : register(t11);
SamplerComparisonState g_shadowSampler : register(s1);

// Poisson Disk 오프셋 상수 배열 (미리 계산된 불규칙한 원형 배치) -> 픽셀 사이 일정한 거리유지
static const float2 PoissonDisk[16] =
{
    float2(-0.94201624f, -0.39906216f),
     float2(0.94558609f, -0.76890725f),
     float2(-0.094184101f, -0.92938870f),
     float2(0.34495938f, 0.29387760f),
     float2(-0.91588581f, 0.45771432f),
     float2(-0.81544232f, -0.87912464f),
     float2(-0.38277543f, 0.27676845f),
     float2(0.97484398f, 0.75648379f),
     float2(0.44323325f, -0.97511554f),
     float2(0.53742981f, -0.47373420f),
     float2(-0.26496911f, -0.41893023f),
     float2(0.79197514f, 0.19090188f),
     float2(-0.24188840f, 0.99706507f),
     float2(-0.81409955f, 0.91437590f),
     float2(0.19984126f, 0.78641367f),
     float2(0.14383161f, -0.14100790f)
};

// 개선된 PCF 샘플링 함수 (해상도 동적 대응 + 푸아송 디스크 + 화면 노이즈 회전)
float get_pcf_shadow_pcss(float3 worldPos, float3 normal, int cascade, float viewDepth)
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
 
     // 월드 좌표 기반 고정 노이즈 (지글거림 방지)
    float noise = frac(52.9829189f * frac(dot(worldPos.xz, float2(0.06711056f, 0.00583715f))));
    float angle = noise * 6.2831853f;
     
    float s, c;
    sincos(angle, s, c);
    float2x2 rotationMat = float2x2(c, -s, s, c);

     // viewDepth 기반 동적 필터 크기 (카메라 기준 - 안정적!)
     // viewDepth는 카메라로부터의 거리 (0 ~ FarZ)
     
     // Cascade별 기본 필터 크기
    float baseFilterRadii[3] = { 0.8f, 1.5f, 3.0f };
     
     // viewDepth를 0~1로 정규화 (Cascade 2의 끝 범위 기준)
    float maxViewDepth = g_shadowSplitDist + 200.0f;
    float normalizedDepth = saturate(viewDepth / maxViewDepth);
     
     // 카메라에 가까우면 선명(0.5배), 멀면 부드럽게(2.5배)
    float depthScale = lerp(0.5f, 2.5f, normalizedDepth);
    float filterRadius = baseFilterRadii[cascade] * depthScale;
 
    float shadow = 0.0f;
     
     // Cascade별 샘플링 횟수 차등 적용 (분기 최적화 위해 if-else 사용)
    if (cascade == 0)
    {
     [unroll]
        for (int i = 0; i < 16; ++i) // 근거리는 16샘플 유지 (고품질)
        {
            float2 rotatedOffset = mul(PoissonDisk[i], rotationMat);
            float2 offset = rotatedOffset * texelSize * filterRadius;
            shadow += g_shadowMap.SampleCmpLevelZero(g_shadowSampler, float3(uv + offset, (float) cascade), sp.z - g_shadowBias);
        }
        return shadow / 16.0f;
    }
    else // cascade 1 (원거리)
    {
     [unroll]
        for (int i = 0; i < 4; ++i) // 원거리는 4샘플로 타협
        {
            float2 rotatedOffset = mul(PoissonDisk[i * 4], rotationMat);
            float2 offset = rotatedOffset * texelSize * filterRadius;
            shadow += g_shadowMap.SampleCmpLevelZero(g_shadowSampler, float3(uv + offset, (float) cascade), sp.z - g_shadowBias);
        }
        return shadow / 4.0f;
    }
}

// 메인 CSM 샘플링 함수 (블렌딩 적용)
float sample_csm_shadow(float3 worldPos, float3 normal, float viewDepth)
{
    float maxShadowDistance = 300.0f;
    if (viewDepth >= maxShadowDistance)
        return 1.0f;

    float blendThreshold = 15.0f; // 블렌딩 구간 (취향에 따라 조절)
    float finalShadow = 1.0f;

 // Cascade 0 -> 1 단일 경계 판정
    if (viewDepth < g_shadowSplitDist - blendThreshold)
    {
     // 확실히 Cascade 0 영역
        finalShadow = get_pcf_shadow_pcss(worldPos, normal, 0, viewDepth);
    }
    else if (viewDepth < g_shadowSplitDist + blendThreshold)
    {
     // Cascade 0과 1 사이 블렌딩 영역
        float shadow0 = get_pcf_shadow_pcss(worldPos, normal, 0, viewDepth);
        float shadow1 = get_pcf_shadow_pcss(worldPos, normal, 1, viewDepth);

     // 0~1 사이의 보간 값 계산
        float t = (viewDepth - (g_shadowSplitDist - blendThreshold)) / (blendThreshold * 2.0f);
        finalShadow = lerp(shadow0, shadow1, saturate(t));
    }
    else
    {
     // 확실히 Cascade 1 영역
        finalShadow = get_pcf_shadow_pcss(worldPos, normal, 1, viewDepth);
    }

    return lerp(0.3f, 1.0f, finalShadow);
}