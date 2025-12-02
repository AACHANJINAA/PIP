// [b0] World Matrix
cbuffer cbPerObject : register(b0)
{
    float4x4 World;
};

     // [b1] Camera Info (View, Projection)
cbuffer cbPerFrame : register(b1)
{
    float4x4 View;
    float4x4 Projection;
};

     // [b2] Terrain Info
cbuffer cbTerrain : register(b2)
{
    float4 Bounds; // x: min_x, y: max_x, z: min_z, w: max_z
    float2 Size; // x: width, y: height
    float HeightScale; // Y축 스케일
    float MinHeight; // 최소 높이 (정규화된 값)
};

Texture2D heightMap : register(t0);
Texture2D baseTexture : register(t1);
Texture2D detailTexture : register(t2);
SamplerState terrainSampler : register(s0);

struct VS_Input
{
    float3 PositionL : POSITION;
    float2 UV : TEXCOORD;
};

struct PS_Input
{
    float4 PositionH : SV_POSITION;
    float3 PositionW : POSITION;
    float2 UV : TEXCOORD0;
};

PS_Input VS_Main(VS_Input input)
{
    PS_Input output = (PS_Input) 0;

         // HeightMap 샘플링 (R16_UNORM: 0~1 범위)
    float height = heightMap.SampleLevel(terrainSampler, input.UV, 0).r;

         // 서버와 동일: normalized * HeightScale (절대 높이)
         // += 가 아니라 = 로 대입 (절대 높이)
    input.PositionL.y += height * HeightScale;

         // Transform
    float4 positionL = float4(input.PositionL, 1.0f);
    output.PositionW = mul(positionL, World).xyz;
    output.PositionH = mul(mul(float4(output.PositionW, 1.0f), View), Projection);
    output.UV = input.UV;

    return output;
}

float4 PS_Main(PS_Input input) : SV_TARGET
{   
         // Base Texture (5배 타일링)
    float2 baseUV = input.UV * 5.0;
    float4 baseColor = baseTexture.Sample(terrainSampler, baseUV);

         // Detail Texture (1배 타일링)
    float2 detailUV = input.UV * 1.0;
    float4 detailColor = detailTexture.Sample(terrainSampler, detailUV);

         // 텍스처 블렌딩
    float4 finalColor = baseColor * detailColor * 2.0;

    return finalColor;
}