 #define MAX_LIGHTS 16

 #define POINT_LIGHT 1
 #define SPOT_LIGHT 2
 #define DIRECTIONAL_LIGHT 3

cbuffer cbMaterial : register(b2)
{
    float4 BaseColorFactor;
    float3 EmissiveFactor;
    float MetallicFactor;
    float RoughnessFactor;
    float NormalTextureScale;
    float AlphaCutoff;
    int AlphaMode; // 0 = OPAQUE, 1 = MASK, 2 = BLEND
    int DoubleSided; // 0 = false, 1 = true
    int HasBaseColorTexture;
    int HasMetallicRoughnessTexture;
    int HasNormalTexture;
    int HasEmissiveTexture;
    float2 Padding; // 16바이트 정렬을 위한 패딩
};

Texture2D g_txDiffuse : register(t0);
Texture2D g_txNormal : register(t1);
Texture2D g_txORM : register(t2); // Occlusion, Roughness, Metallic
Texture2D g_txEmissive : register(t3);
SamplerState g_samLinear : register(s0);

// 객체별 월드 행렬
cbuffer cbWorldMatrix : register(b0)
{
    matrix g_matWorld;
    matrix g_matWorldInverseTranspose;
};

// 카메라 정보
cbuffer cbCamera : register(b1)
{
    matrix g_matView;
    matrix g_matProjection;
    float4 gvCameraPosition;
};

 // Light.hlsl에서 사용하는 구조체와 동일하게 맞춤
struct MATERIAL
{
    float4 m_cAmbient;
    float4 m_cDiffuse;
    float4 m_cSpecular; // a = power
    float4 m_cEmissive;
};

 // 임시로 머티리얼 정의 (PBR에서는 사용 안하지만 Light.hlsl 함수에 필요)
static MATERIAL gMaterial =
{
    float4(0.2, 0.2, 0.2, 1.0), // Ambient
	 float4(0.8, 0.8, 0.8, 1.0), // Diffuse
	 float4(1.0, 1.0, 1.0, 32.0), // Specular (a = shininess)
	 float4(0.0, 0.0, 0.0, 1.0) // Emissive
};

 #include "Light.hlsl"
 #include "IBL.hlsl"

struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord0 : TEXCOORD; // 텍스쳐 좌표 (추가된 부분)
    float4 Tangent : TANGENT;
};

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float3 WorldPosition : POSITION0;
    float2 TexCoord : TEXCOORD0;
    float3 Normal : NORMAL0;
    float3 Tangent : TANGENT0;
    float3 Bitangent : BITANGENT0;
};

VS_OUTPUT VS_GLTF(VS_INPUT input)
{
    VS_OUTPUT Out;
    Out.WorldPosition = mul(float4(input.Position, 1.0f), g_matWorld).xyz;
    Out.Position = mul(float4(Out.WorldPosition, 1.0f), g_matView);
    Out.Position = mul(Out.Position, g_matProjection);
    Out.TexCoord = input.TexCoord0;

    Out.Normal = normalize(mul(input.Normal, (float3x3) g_matWorldInverseTranspose));
    Out.Tangent = normalize(mul(input.Tangent.xyz, (float3x3) g_matWorldInverseTranspose));
    Out.Tangent = normalize(Out.Tangent - dot(Out.Tangent, Out.Normal) * Out.Normal);
    Out.Bitangent = cross(Out.Normal, Out.Tangent) * input.Tangent.w;

    return Out;
}

//float4 PS_GLTF(VS_OUTPUT In) : SV_TARGET
//{
//     float3 N = normalize(In.Normal);
//        float3 V = normalize(gvCameraPosition.xyz - In.WorldPosition);
//        float3 R = reflect(-V, N);

//        // TEST 1: Irradiance Map 직접 샘플링
//        float3 testIrr = g_IrradianceMap.Sample(g_samLinear, N).rgb;
//        return float4(testIrr * 10.0, 1.0); // 10배 밝게
//}

float4 PS_GLTF(VS_OUTPUT In) : SV_TARGET
{
    // 1. Albedo (BaseColor) 값 설정
    float4 diffuseSample = g_txDiffuse.Sample(g_samLinear, In.TexCoord);
    float3 albedo = diffuseSample.rgb;
    if (length(albedo) < 0.01f)
    {
        albedo = float3(1.0, 1.0, 1.0);
    }
    albedo *= BaseColorFactor.rgb;

    // 2. ORM (Occlusion, Roughness, Metallic) 값 설정
    float3 ormSample = g_txORM.Sample(g_samLinear, In.TexCoord).rgb;
    float ao = 1.0f;
    float roughness = RoughnessFactor;
    float metallic = MetallicFactor;
  

    if (dot(ormSample, float3(1, 1, 1)) > 0.05f)
    {
        ao = ormSample.r;
        roughness *= ormSample.g;
        metallic *= ormSample.b;
    }
	else
	{
        metallic = 0.0f;
    }

     // 3. Normal Map
    float3 N = normalize(In.Normal);
    float3 N_geom = normalize(In.Normal); // 기하학적 Normal 저장

    float3 normalMapSample = g_txNormal.Sample(g_samLinear, In.TexCoord).rgb;

    if (length(normalMapSample) > 0.1f)
    {
        float3 N_map = normalMapSample * 2.0 - 1.0;

        float3 T = normalize(In.Tangent);
        float3 B = normalize(In.Bitangent);

             // TBN 행렬을 이용한 변환
        float3x3 TBN = float3x3(T, B, N_geom);
        N = normalize(mul(N_map, TBN));
    }

         // 4. Emissive
    float3 emissiveSample = g_txEmissive.Sample(g_samLinear, In.TexCoord).rgb;
    float3 emissiveColor = (length(emissiveSample) > 0.01f) ? emissiveSample : float3(1.0, 1.0, 1.0);
    float3 finalEmissive = emissiveColor * EmissiveFactor;

         // 5. View vector
    float3 V = normalize(gvCameraPosition.xyz - In.WorldPosition);

         // ===== 수정: DoubleSided 처리 - 기하학적 Normal 기준 =====
    if (DoubleSided > 0 && dot(N_geom, V) < 0.0)
    {
        N = -N;
    }
    // 1. 직접광 계산 (Light.hlsl의 Lighting 함수)
    float4 litColor = Lighting(In.WorldPosition, N, V, albedo, metallic, roughness, ao);

   // 2. 환경광 계산 (IBL.hlsl의 CalculateIBL 함수)
    float3 iblColor = CalculateIBL(N, V, albedo, metallic, roughness, ao);

    // 3. 직접광 + 환경광 + 자체발광
    float3 finalColor = litColor.rgb + iblColor + finalEmissive;

    // 톤 매핑 및 감마 보정
    finalColor = finalColor / (finalColor + 1.0f);
    finalColor = pow(finalColor, 1.0f / 2.2f);
    
    return float4(finalColor, diffuseSample.a);
}

//////////////////////// HP 효과 픽셀 셰이더 추가 ////////////////////

cbuffer cbHp : register(b8, space1)
{
    int g_nHp;
};

float4 PS_HP_GLTF(VS_OUTPUT In) : SV_TARGET
{
    float4 color = PS_GLTF(In);
 
    // --- [고유 기능] HP 감소 효과 ---
    float hp_r = (100 - g_nHp) / 100.0f;
    if (color.r < hp_r)
    {
        color.r = hp_r;
    }
    // ---------------------------------

    return float4(color);
}