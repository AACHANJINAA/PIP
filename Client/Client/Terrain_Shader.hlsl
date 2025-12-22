#define MAX_LIGHTS 8
#define MAX_MATERIALS 8

cbuffer cbCameraInfo : register(b1)
{
    matrix gmtxView;
    matrix gmtxProjection;
    float4 gvCameraPosition; // Camera Position
};

#include "Light.hlsl"

cbuffer cbPerObject : register(b0)
{
    float4x4 World;
};

Texture2D albedoTexture : register(t0);
Texture2D normalTexture : register(t1);
Texture2D ormTexture : register(t2);
Texture2D emissiveTexture : register(t3);
SamplerState terrainSampler : register(s0);

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

float4 PS_Main(PS_Input input) : SV_TARGET
{
    float3 P = input.PositionW;
    float3 N = normalize(input.NormalW);
    float3 V = normalize(gvCameraPosition.xyz - P); // View direction

    float3 albedo = albedoTexture.Sample(terrainSampler, input.UV).rgb;
    float3 sampledNormalMap = normalTexture.Sample(terrainSampler, input.UV).rgb;
   
    float3 orm = ormTexture.Sample(terrainSampler, input.UV).rgb;
    float ao = orm.r; // Red channel for Ambient Occlusion
    float roughness = orm.g; // Green channel for Roughness
    float metallic = orm.b; // Blue channel for Metallic
    
    float3 emissive = emissiveTexture.Sample(terrainSampler, input.UV).rgb; // Sample emissive
    
	// Convert normal from tangent space to world space using TBN matrix
    float3 N_tangent = sampledNormalMap * 2.0 - 1.0;
	// Construct TBN matrix
    float3x3 TBN = float3x3(normalize(input.TangentW), normalize(input.BitangentW), N);
    N = normalize(mul(N_tangent, TBN));

    // Call the PBR Lighting function
    float4 litColor = Lighting(P, N, V, albedo, metallic, roughness, ao);
    
    float3 finalColor = litColor.rgb + emissive; // Add emissive after lighting
     
	// Tone Mapping (HDR -> LDR)
    finalColor.rgb = finalColor.rgb / (finalColor.rgb + 1.0f);
    
	// Gamma Correction
    finalColor.rgb = pow(finalColor.rgb, 1.0f / 2.2f);
   
    return float4(finalColor, 1.0f); // Assuming alpha is always 1.0 for terrain
}