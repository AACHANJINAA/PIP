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
    int HasOcclusionTexture;
    float SpecularFactor;

};

Texture2D g_txDiffuse : register(t0);
Texture2D g_txNormal : register(t1);
Texture2D g_txORM : register(t2); // Occlusion, Roughness, Metallic
Texture2D g_txEmissive : register(t3);
Texture2D g_txOcclusion : register(t4);
SamplerState g_samLinear : register(s0);

// 객체별 월드 행렬
cbuffer cbWorldMatrix : register(b0)
{
    matrix g_matWorld;
    matrix g_matWorldInverseTranspose;
    int g_bReceiveShadow; // [추가] 0이면 그림자 안 받음, 1이면 받음
    int g_otherplayerid;
    float2 g_worldPad; // 16바이트 정렬을 위한 패딩
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
#include "Shadow_Sample.hlsl"

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

float3 lerp_op(float3 final_color)
{
	const float3 playercolors[4] = {
        float3(0.863f, 0.078f, 0.235f), // crimson red
		float3(0.0f, 1.0f, 0.498f), // spring green
		float3(1.0f, 0.843f, 0.0f), // gold
		float3(0.541f, 0.169f, 0.886f), // violet
    };

    int colorIndex = g_otherplayerid % 4;

    float lerp_figure = 0.2;
    return lerp(final_color, playercolors[colorIndex], lerp_figure);
}

float4 PS_GLTF(VS_OUTPUT In) : SV_TARGET
{
    // 1. Albedo (BaseColor) 값 설정
    float4 diffuseSample = g_txDiffuse.Sample(g_samLinear, In.TexCoord);
    float3 albedo = diffuseSample.rgb * BaseColorFactor.rgb;

    // 2. ORM (Occlusion, Roughness, Metallic) 값 설정
    float3 orm = g_txORM.Sample(g_samLinear, In.TexCoord).rgb;

     // Occlusion: 별도 텍스처가 있으면 그것을 사용, 없으면 ORM.r 사용
    float ao = 1.0f; // 기본값은 밝음
    if (HasOcclusionTexture > 0)
    {
        ao = g_txOcclusion.Sample(g_samLinear, In.TexCoord).r;
    }
    else if (HasMetallicRoughnessTexture > 0)
    {
    // Occlusion 전용 텍스처는 없지만 ORM 텍스처는 있는 경우, R채널 사용
        ao = orm.r;
    }

    float roughness;
    float metallic;

    if (HasMetallicRoughnessTexture > 0)
    {
		// 텍스처가 있는 경우: 텍스처 값 * Factor
        roughness = orm.g * RoughnessFactor;
        metallic = orm.b * MetallicFactor;
    }
    else
    {
        roughness = RoughnessFactor;
        metallic = MetallicFactor; 
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
    float3 finalEmissive = emissiveSample * EmissiveFactor;

         // 5. View vector
    float3 V = normalize(gvCameraPosition.xyz - In.WorldPosition);

   
    if (dot(N, V) < 0.0)
    {
        N = -N;
    }
    
    // 1. 직접광 계산 (Light.hlsl의 Lighting 함수)
    float4 litColor = Lighting(In.WorldPosition, N, V, albedo, metallic, roughness, ao, SpecularFactor);

   // 2. 환경광 계산 (IBL.hlsl의 CalculateIBL 함수)
    float3 iblColor = CalculateIBL(N, V, albedo, metallic, roughness, ao);

    // View 공간에서의 깊이(Z) 값 계산 (어떤 Cascade를 쓸지 결정하기 위함)
    float3 viewPos = mul(float4(In.WorldPosition, 1.0f), g_matView).xyz;
    float viewDepth = viewPos.z;

    // 그림자 값 샘플링 (0.0: 완전 그림자 ~ 1.0: 빛 받음)
    float shadowFactor = sample_csm_shadow(In.WorldPosition, N, viewDepth);
    
    // 3. (직접광 * 그림자 팩터) + 환경광 + 자체발광 -> 아직 directional light에만 적용 (직교만)
    float3 finalColor = (litColor.rgb * shadowFactor) + iblColor + finalEmissive;

    // MASK 모드: alphaCutoff 이하의 픽셀을 폐기 (clip 함수 사용)
    if (AlphaMode == 1) // MASK
    {
        clip(diffuseSample.a - AlphaCutoff); // 알파가 cutoff보다 작으면 픽셀 폐기
    }
    
    // 톤 매핑 및 감마 보정
    finalColor = finalColor / (finalColor + 1.0f);
    finalColor = pow(finalColor, 1.0f / 2.2f);

    if (g_otherplayerid > -1)
    {
        finalColor = lerp_op(finalColor);
    }
    
    return float4(finalColor, diffuseSample.a);
}