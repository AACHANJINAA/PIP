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

SamplerState terrain_sampler : register(s0);

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
#define g_samLinear terrain_sampler 
#include "IBL.hlsl"
#include "Shadow_Sample.hlsl"

float4 PS_Main(PS_Input input) : SV_TARGET
{
    float3 P = input.PositionW;
    float3 N = normalize(input.NormalW);
    float3 V = normalize(gvCameraPosition.xyz - P);

    float2 edge_dist = min(input.UV, 1.0 - input.UV); // 0~0.5 범위
    float edge_blend = 1.0 - saturate(min(edge_dist.x, edge_dist.y) * 10.0);

    float3 final_albedo;
    float3 final_normal_TS; // Tangent Space
    float final_roughness;
    float final_metallic = 0.0f; // 지형은 비금속
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
            weights[i] = weightmaps.Sample(terrain_sampler, float3(uv, i)).r;
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
            weights[6] = 1.0f;
        }
        
        float grass_mix_ratio = 0.5f; // 섞고 싶은 풀의 양 (0.0 ~ 1.0)
        
        float painted_rock = weights[0];
        weights[0] = painted_rock * (1.0f - grass_mix_ratio);
        weights[3] += painted_rock * grass_mix_ratio;
        
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
        final_albedo = float3(0, 0, 0);
        final_normal_TS = float3(0, 0, 0); // Tangent space default normal
        final_roughness = 0.0;

        for (int i = 0; i < NumLayers; ++i)
        {
            if (weights[i] < 0.01)
                continue; // 미미한 가중치 스킵

            float2 layerUV = uv * LayerTiling;

            // 각 레이어 샘플링
            float3 layerAlb = layerAlbedos.Sample(terrain_sampler, float3(layerUV, i)).rgb;
            float3 layerNrm = layerNormals.Sample(terrain_sampler, float3(layerUV, i)).rgb * 2.0 - 1.0;
            float layerRgh = layerRoughness.Sample(terrain_sampler, float3(layerUV, i)).r;

            // 가중치 블렌딩
            final_albedo += layerAlb * weights[i];
            final_normal_TS += layerNrm * weights[i];
            final_roughness += layerRgh * weights[i];
        }

        // Normal 재정규화
        final_normal_TS = normalize(final_normal_TS);
    }
    else
    {
        // ===== 단일 지형 처리 (기존 방식) =====
        float2 base_uv = input.UV * Tiling;
        final_albedo = albedoTexture.Sample(terrain_sampler, input.UV).rgb;

        // Detail Texture
        float2 detail_uv = input.UV * DetailTiling;
        float3 detail_color = detailTexture.Sample(terrain_sampler, detail_uv).rgb;
        float blend_strength = 0.3;
        float3 detail_norm = detail_color * 2.0 - 1.0;
        final_albedo = final_albedo * (1.0 + detail_norm * blend_strength);

        // Normal Map
        final_normal_TS = normalTexture.Sample(terrain_sampler, base_uv).rgb * 2.0 - 1.0;

        // ORM
        float3 orm = ormTexture.Sample(terrain_sampler, base_uv).rgb;
        ao = orm.r; // Occlusion
        final_roughness = orm.g; // Roughness
        final_metallic = orm.b; // Metallic 
    }

	// ===== 공통: TBN 변환 및 라이팅 =====

	// Tangent Space -> World Space
    float3x3 TBN = float3x3(
             normalize(input.TangentW),
             normalize(input.BitangentW),
             N
         );
    N = normalize(mul(final_normal_TS, TBN));

         // PBR Lighting
    float4 lit_color = Lighting(P, N, V, final_albedo, final_metallic, final_roughness, ao, SpecularFactor);

         // IBL
    float3 ibl_color = CalculateIBL(N, V, final_albedo, final_metallic, final_roughness, ao);

         // Shadow
    float3 view_pos = mul(float4(input.PositionW, 1.0f), gmtxView).xyz;
    float view_depth = view_pos.z;
    float shadow_factor = sample_csm_shadow(input.PositionW, N, view_depth);

    float3 final_color = (lit_color.rgb * shadow_factor) + ibl_color;

	// [조건부] Emissive는 단일 지형에만 적용
    if (NumLayers == 0)
    {
        float2 base_uv = input.UV * Tiling;
        float3 emissive = emissiveTexture.Sample(terrain_sampler, base_uv).rgb;
        final_color += emissive;
    }

	// Tone Mapping
    final_color = final_color / (final_color + 1.0f);

	// Gamma Correction
    final_color = pow(final_color, 1.0f / 2.2f);

    return float4(final_color, 1.0f);
}