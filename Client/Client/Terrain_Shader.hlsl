#define MAX_LIGHTS 8
#define MAX_MATERIALS 8
#define MAX_TERRAIN_LAYERS 8

cbuffer cbPerObject : register(b0)
{
    float4x4 World;
};

cbuffer cbCameraInfo : register(b1)
{
    matrix gmtxView;
    matrix gmtxProjection;
    float4 gvCameraPosition; // Camera Position
};

// 단일 지형 텍스쳐용
Texture2D albedoTexture : register(t0);
Texture2D normalTexture : register(t1);
Texture2D ormTexture : register(t2);
Texture2D emissiveTexture : register(t3);
Texture2D detailTexture : register(t4); 

// 다중 지형 텍스쳐용
Texture2DArray<float> weightmaps : register(t12); // R8 x 8 layers
Texture2DArray<float4> layerAlbedos : register(t13); // RGBA x 8 layers
Texture2DArray<float4> layerNormals : register(t14); // RGBA x 8 layers
Texture2DArray<float> layerRoughness : register(t15); // R x 8 layers

SamplerState terrainSampler : register(s0);

cbuffer cbTerrainInfo : register(b2)
{
    float4 Bounds;
    float2 Size;
    float HeightScale;
    float MinHeight;
    float2 Tiling;
    float2 DetailTiling;
    float SpecularFactor;
    float Padding;
};

cbuffer cbLayerInfo : register(b6)
{
    int NumLayers; // 실제 사용하는 레이어 수 (0 = 비활성, 1~8 = 활성)
    float LayerTiling; // 레이어별 타일링 (통일값)
    float2 LayerPadding;
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
    float3 V = normalize(gvCameraPosition.xyz - P);

    float2 edge_dist = min(input.UV, 1.0 - input.UV); // 0~0.5 범위
    float edge_blend = 1.0 - saturate(min(edge_dist.x, edge_dist.y) * 10.0);

    float3 finalAlbedo;
    float3 finalNormalTS; // Tangent Space
    float finalRoughness;
    float finalMetallic = 0.0f; // 지형은 비금속
    float ao = 1.0f;

    //[분기] Layer 시스템 활성화 여부 체크
    if (NumLayers > 0)
    {
        // ===== Multi-Layer 지형 처리 =====
        float2 uv = input.UV;

        // 1. Weightmap 샘플링 (각 레이어의 가중치 추출)
        float weights[MAX_TERRAIN_LAYERS];
        float total_weight = 0.0;

        for (int i = 0; i < NumLayers; ++i)
        {
            weights[i] = weightmaps.Sample(terrainSampler, float3(uv, i)).r;
            total_weight += weights[i];
        }
        
        // 2. 가중치 정규화 (합이 1이 되도록)
        if (total_weight > 0.001)
        {
            for (int i = 0; i < NumLayers; ++i)
                weights[i] /= total_weight;
        }
        else
        {
            // set basic layer 
            // 0 : ROCK,                 1 : Ground_2,     2 : Ground,     3 : Dead_Grass, 
            // 4 : Underwater_Ground_01, 5 : Sand_w_Rocks, 6 : Grass,      7 : Cobblestone
            weights[3] = 1.0f;
        }

        float original_weights[MAX_TERRAIN_LAYERS];
        for (int i = 0; i < NumLayers; ++i)
        {
            original_weights[i] = weights[i];
        }
         
         // 경계로 갈수록 Rock(index 0)의 비율 증가
        weights[0] = lerp(original_weights[0], 1.0, edge_blend);
        for (int i = 1; i < NumLayers; ++i)
        {
            weights[i] = lerp(original_weights[i], 0.0, edge_blend);
        }

        // 3. 레이어별 텍스처 블렌딩
        finalAlbedo = float3(0, 0, 0);
        finalNormalTS = float3(0, 0, 1); // Tangent space default normal
        finalRoughness = 0.5;

        for (int i = 0; i < NumLayers; ++i)
        {
            if (weights[i] < 0.01)
                continue; // 미미한 가중치 스킵

            float2 layerUV = uv * LayerTiling;

            // 각 레이어 샘플링
            float3 layerAlb = layerAlbedos.Sample(terrainSampler, float3(layerUV, i)).rgb;
            float3 layerNrm = layerNormals.Sample(terrainSampler, float3(layerUV, i)).rgb * 2.0 - 1.0;
            float layerRgh = layerRoughness.Sample(terrainSampler, float3(layerUV, i)).r;

            // 가중치 블렌딩
            finalAlbedo += layerAlb * weights[i];
            finalNormalTS += layerNrm * weights[i];
            finalRoughness += layerRgh * weights[i];
        }

        // Normal 재정규화
        finalNormalTS = normalize(finalNormalTS);
    }
    else
    {
        // ===== 단일 지형 처리 (기존 방식) =====
        float2 baseUV = input.UV * Tiling;
        finalAlbedo = albedoTexture.Sample(terrainSampler, input.UV).rgb;

        // Detail Texture
        float2 detailUV = input.UV * DetailTiling;
        float3 detailColor = detailTexture.Sample(terrainSampler, detailUV).rgb;
        float blend_strength = 0.3;
        float3 detail_norm = detailColor * 2.0 - 1.0;
        finalAlbedo = finalAlbedo * (1.0 + detail_norm * blend_strength);

        // Normal Map
        finalNormalTS = normalTexture.Sample(terrainSampler, baseUV).rgb * 2.0 - 1.0;

        // ORM
        float3 orm = ormTexture.Sample(terrainSampler, baseUV).rgb;
        ao = orm.r; // Occlusion
        finalRoughness = orm.g; // Roughness
        finalMetallic = orm.b; // Metallic 
    }

	// ===== 공통: TBN 변환 및 라이팅 =====

	// Tangent Space -> World Space
    float3x3 TBN = float3x3(
             normalize(input.TangentW),
             normalize(input.BitangentW),
             N
         );
    N = normalize(mul(finalNormalTS, TBN));

         // PBR Lighting
    float4 litColor = Lighting(P, N, V, finalAlbedo, finalMetallic, finalRoughness, ao, SpecularFactor);

         // IBL
    float3 iblColor = CalculateIBL(N, V, finalAlbedo, finalMetallic, finalRoughness, ao);

         // Shadow
    float3 viewPos = mul(float4(input.PositionW, 1.0f), gmtxView).xyz;
    float viewDepth = viewPos.z;
    float shadowFactor = sample_csm_shadow(input.PositionW, N, viewDepth);

    float3 finalColor = (litColor.rgb * shadowFactor) + iblColor;

	// [조건부] Emissive는 단일 지형에만 적용
    if (NumLayers == 0)
    {
        float2 baseUV = input.UV * Tiling;
        float3 emissive = emissiveTexture.Sample(terrainSampler, baseUV).rgb;
        finalColor += emissive;
    }

	// Tone Mapping
    finalColor = finalColor / (finalColor + 1.0f);

	// Gamma Correction
    finalColor = pow(finalColor, 1.0f / 2.2f);

    return float4(finalColor, 1.0f);
}