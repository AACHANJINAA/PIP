
// [b0] World Matrix (RenderComponent에서 전달)
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

// [b2] Terrain Info (JSON에서 로드한 정보)
cbuffer cbTerrain : register(b2)
{
    float4 Bounds; // x: min_x, y: max_x, z: min_z, w: max_z
    float2 Size; // x: width (504), y: height (504)
    float HeightScale; // Y축 스케일 (100.0)
    float Padding; // 16바이트 정렬용
};


Texture2D heightMap : register(t0); // 높이맵 (Heightmap.r16, R16_UNORM)
Texture2D baseTexture : register(t1); // 기본 텍스처 (T_ground_Moss_D.png)
Texture2D detailTexture : register(t2); // 디테일 텍스처 (T_Ground_Moss_N.png)
SamplerState terrainSampler : register(s0);


struct VS_Input
{
    float3 PositionL : POSITION; // Local Space Position
    float2 UV : TEXCOORD; // UV Coordinates
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

         // HeightMap 샘플링
    float height = heightMap.SampleLevel(terrainSampler, input.UV, 0).r; // 0 ~ 1

         // 중심을 0으로 만들기 (0.5를 빼서 -0.5 ~ +0.5)
    height = height - 0.5f;

         // Y축 변위 적용
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
         // 1. 텍스처 타일링 설정
         //    Base Texture: 1배 (원본 크기)
         //    Detail Texture: 4배 (디테일을 위해 반복)
    float2 baseUV = input.UV * 1.0;
    float2 detailUV = input.UV * 4.0;

         // 2. 텍스처 샘플링
         //    Base: 주요 색상 (T_ground_Moss_D.png)
         //    Detail: 디테일/노멀맵 (T_Ground_Moss_N.png)
    float4 baseColor = baseTexture.Sample(terrainSampler, baseUV);
    float4 detailColor = detailTexture.Sample(terrainSampler, detailUV);

         // 3. 텍스처 블렌딩 (Modulate)
         //    두 텍스처를 곱한 후 2배로 밝기 보정
    float4 finalColor = baseColor * detailColor * 2.0;

         // 4. 최종 색상 반환
    return finalColor;
}