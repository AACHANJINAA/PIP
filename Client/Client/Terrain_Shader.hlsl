#define MAX_LIGHTS 8
#define MAX_MATERIALS 8

cbuffer cbCameraInfo : register(b1)
{
    matrix gmtxView;
    matrix gmtxProjection;
    float4 gvCameraPosition; // Camera Position
};

cbuffer cbPerObject : register(b0)
{
    float4x4 World;
};

Texture2D albedoTexture : register(t0);
Texture2D normalTexture : register(t1);
Texture2D ormTexture : register(t2);
Texture2D emissiveTexture : register(t3);
Texture2D detailTexture : register(t4); // 디테일 텍스처
SamplerState terrainSampler : register(s0);

cbuffer cbTerrainInfo : register(b2)
{
    float4 Bounds;
    float2 Size;
    float HeightScale;
    float MinHeight;
    float2 Tiling;
    float2 DetailTiling; 
    float2 Padding; // 16바이트 정렬
};

struct VS_Input
{
    float3 PositionL : POSITION;
    float3 NormalL : NORMAL;
	float2 UV : TEXCOORD;
    float3 TangentL : TANGENT;
};

struct PS_Input
{
    float4 PositionH : SV_POSITION;
    float3 PositionW : POSITION;
    float3 NormalW : NORMAL;
    float3 TangentW : TANGENT; // Added for normal mapping
    float3 BitangentW : BITANGENT; // Added for normal mapping
    float2 UV : TEXCOORD0;
};

PS_Input VS_Main(VS_Input input)
{
    PS_Input output = (PS_Input) 0;

    // 이미 CPU에서 높이가 계산된 정점 위치를 사용
    output.PositionW = mul(float4(input.PositionL, 1.0f), World).xyz;
    output.NormalW = mul(input.NormalL, (float3x3) World);
    output.TangentW = normalize(mul(input.TangentL, (float3x3) World));
    output.BitangentW = normalize(cross(output.NormalW, output.TangentW));

    output.PositionH = mul(mul(float4(output.PositionW, 1.0f), gmtxView), gmtxProjection);
    output.UV = input.UV;

    return output;
}

#include "Light.hlsl"
#define g_samLinear terrainSampler 
#include "IBL.hlsl"
#include "Shadow_Sample.hlsl"

float4 PS_Main(PS_Input input) : SV_TARGET
{
    float3 P = input.PositionW;
    float3 N = normalize(input.NormalW);
    float3 V = normalize(gvCameraPosition.xyz - P); // View direction

    float2 baseUV = input.UV * Tiling;
    float3 albedo = albedoTexture.Sample(terrainSampler, input.UV).rgb;
    
    // 디테일 텍스처 샘플링 (별도의 타일링 값 사용)
    float2 detailUV = input.UV * DetailTiling;
    float3 detailColor = detailTexture.Sample(terrainSampler, detailUV).rgb;

    // Multiply Blending: 기본 색상과 디테일 색상을 곱하여 혼합
    float blend_strength = 0.8; // 블렌딩 강도
    albedo += (detailColor.r - 0.5) * blend_strength;

    // 나머지 텍스처들은 기본 UV 사용
    float3 sampledNormalMap = normalTexture.Sample(terrainSampler, baseUV).rgb;

    float3 orm = ormTexture.Sample(terrainSampler, baseUV).rgb;
    float ao = orm.r; // Red channel for Ambient Occlusion
    float roughness = orm.g; // Green channel for Roughness
    float metallic = orm.b; // Blue channel for Metallic
    
    float3 emissive = emissiveTexture.Sample(terrainSampler, baseUV).rgb; // Sample emissive
    
	// Convert normal from tangent space to world space using TBN matrix
    float3 N_tangent = sampledNormalMap * 2.0 - 1.0;
	// Construct TBN matrix
    float3x3 TBN = float3x3(normalize(input.TangentW), normalize(input.BitangentW), N);
    N = normalize(mul(N_tangent, TBN));
    
    float specular = 0.5f;
    
     // Call the PBR Lighting function
    float4 litColor = Lighting(P, N, V, albedo, metallic, roughness, ao, specular);

        // IBL 추가 ← 추가
    float3 iblColor = CalculateIBL(N, V, albedo, metallic, roughness, ao);
    
    // [추가] 그림자 계산
    float3 viewPos = mul(float4(input.PositionW, 1.0f), gmtxView).xyz;
    float viewDepth = viewPos.z;
    float shadowFactor = sample_csm_shadow(input.PositionW, viewDepth);

    float3 finalColor = (litColor.rgb * shadowFactor) + iblColor + emissive;
     
	// Tone Mapping (HDR -> LDR)
    finalColor.rgb = finalColor.rgb / (finalColor.rgb + 1.0f);
    
	// Gamma Correction
    finalColor.rgb = pow(finalColor.rgb, 1.0f / 2.2f);
   
    return float4(finalColor, 1.0f); // Assuming alpha is always 1.0 for terrain
}