// IBL.hlsl
    // Image-Based Lighting (환경광 기반 조명) 계산 셰이더
    // PBR 렌더링의 간접광(Indirect Lighting)을 처리합니다.

#ifndef _IBL_HLSL_
#define _IBL_HLSL_

    // ============================================================
    // IBL 텍스처 선언
    // ============================================================

    // t8: Irradiance Map - Diffuse IBL용 (사전 계산된 환경 확산광)
TextureCube g_IrradianceMap : register(t8);

    // t9: Prefiltered Environment Map - Specular IBL용 (Mipmap으로 Roughness 표현)
TextureCube g_PrefilteredMap : register(t9);

    // t10: BRDF LUT - Split-Sum Approximation용 (NdotV, Roughness -> scale, bias)
Texture2D g_BrdfLut : register(t10);

    // 주의: SamplerState g_samLinear는 Gltf_Shader.hlsl에서 선언됨
    // IBL.hlsl은 Gltf_Shader.hlsl에 include되므로 자동으로 사용 가능

  // ============================================================
    // Diffuse IBL 계산
    // ============================================================
float3 CalculateDiffuseIBL(float3 N, float3 albedo, float metallic)
{
        // 1. F0 계산 (표면 반사율)
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);

        // 2. kD 계산 (Diffuse 강도)
    float3 kD = (1.0 - F0) * (1.0 - metallic);

        // 3. Irradiance Map 샘플링
    float3 irradiance = g_IrradianceMap.Sample(g_samLinear, N).rgb;

        // HDR 텍스처 스케일링 (값이 너무 큰 경우)
    irradiance *= 0.001; // ← 이 줄 추가!
     // 최소 환경광 추가 (완전히 검은색 방지)
    float3 minAmbient = float3(0.02, 0.02, 0.02); // ← 이 값 조절 (0.01~0.05)
    irradiance = max(irradiance, minAmbient);
        // 4. Lambertian Diffuse BRDF 계산
    return kD * albedo * irradiance;
}

    // ============================================================
    // Specular IBL 계산
    // ============================================================
float3 CalculateSpecularIBL(float3 N, float3 V, float3 albedo, float metallic, float roughness)
{
        // 1. 반사 벡터 계산
    float3 R = reflect(-V, N);

        // 2. NdotV 계산
    float NdotV = saturate(dot(N, V));

        // 3. Prefiltered Environment Map LOD 선택
    float maxMipLevel = 5.0;
    float lod = roughness * maxMipLevel;

        // 4. Prefiltered Map 샘플링
    float3 prefilteredColor = g_PrefilteredMap.SampleLevel(g_samLinear, R, lod).rgb;

        // HDR 텍스처 스케일링 (값이 너무 큰 경우)
    prefilteredColor *= 0.1; // ← 이 줄 추가!

        // 5. BRDF LUT 샘플링
    float2 brdf = g_BrdfLut.Sample(g_samLinear, float2(NdotV, roughness)).rg;

        // 6. F0 계산
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);

        // 7. Split-Sum Approximation 최종 계산
    return prefilteredColor * (F0 * brdf.x + brdf.y);
}

    // IBL 통합 함수
float3 CalculateIBL(float3 N, float3 V, float3 albedo, float metallic, float roughness, float ao)
{
        // 1. Diffuse IBL 계산
    float3 diffuse = CalculateDiffuseIBL(N, albedo, metallic);

        // 2. Specular IBL 계산
    float3 specular = CalculateSpecularIBL(N, V, albedo, metallic, roughness);

        // 3. Diffuse + Specular 합산 후 AO 적용
    return (diffuse + specular) * ao; // ← iblStrength 제거!
}
#endif // _IBL_HLSL_