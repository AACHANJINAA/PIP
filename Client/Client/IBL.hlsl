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
   // t10: BRDF LUT - Split-Sum Approximation 용 (NdotV, Roughness -> scale, bias)
Texture2D g_BrdfLut : register(t10);

    // 주의: SamplerState g_samLinear는 Gltf_Shader.hlsl에서 선언됨
    // IBL.hlsl은 Gltf_Shader.hlsl에 include되므로 자동으로 사용 가능

// Diffuse IBL 계산
float3 CalculateDiffuseIBL(float3 N, float3 albedo, float metallic)
{
        // 1. F0 계산 (표면 반사율)
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);

        // 2. kD 계산 (Diffuse 강도)
    float3 kD = (1.0 - F0) * (1.0 - metallic);

        // 3. Irradiance Map 샘플링
    float3 irradiance = g_IrradianceMap.Sample(g_samLinear, N).rgb;
    
   // irradiance *= 0.1;
    
        // 4. Lambertian Diffuse BRDF 계산
    return kD * albedo * irradiance;
}

// Specular IBL 계산
float3 CalculateSpecularIBL(float3 N, float3 V, float3 albedo, float metallic, float roughness)
{
        // 1. 반사 벡터 계산
    float3 R = reflect(-V, N);

        // 2. NdotV 계산
    float NdotV = saturate(dot(N, V));

        // 3. Prefiltered Environment Map LOD 선택
    float maxMipLevel = 3.0;
    float safeRoughness = max(roughness, 0.045); // 최소 4.5%
    float lod = safeRoughness * maxMipLevel;
    
     // 4. Prefiltered Map 샘플링
    float3 prefilteredColor = g_PrefilteredMap.SampleLevel(g_samLinear, R, lod).rgb;
    
    prefilteredColor *= 0.005;
    
       // 5. BRDF LUT 샘플링
    float2 brdf = g_BrdfLut.Sample(g_samLinear, float2(NdotV, roughness)).rg;

       // 6. F0 계산
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), albedo, metallic);

       // 7. Single Scattering Specular 계산
    float3 specularSingle = F0 * brdf.x + brdf.y;

       // 8. Multiple Scattering Energy Compensation
       // Ess = Single Scattering의 총 에너지
    float Ess = brdf.x + brdf.y;
    Ess = max(Ess, 0.001); // 0으로 나누기 방지

       // Energy Compensation Factor 계산
       // 수식: 1.0 + F0 * (1.0/Ess - 1.0)
       // 의미: 손실된 에너지를 F0에 비례하여 복구
    float3 energyCompensation = 1.0 + F0 * (1.0 / Ess - 1.0);

       // 9. 최종 Specular = Single Scattering * Energy Compensation
    return prefilteredColor * specularSingle * energyCompensation;
}

    // IBL 통합 함수
float3 CalculateIBL(float3 N, float3 V, float3 albedo, float metallic, float roughness, float ao)
{
        // 1. Diffuse IBL 계산
    float3 diffuse = CalculateDiffuseIBL(N, albedo, metallic);

        // 2. Specular IBL 계산
    float3 specular = CalculateSpecularIBL(N, V, albedo, metallic, roughness);

        // 3. Diffuse + Specular 합산 후 AO 적용
    return (diffuse + specular) * ao; 
}
#endif // _IBL_HLSL_