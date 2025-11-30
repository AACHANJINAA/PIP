cbuffer cbPerFrame : register(b1)
{
    float4x4 View;
    float4x4 Projection;
};

cbuffer cbTerrain : register(b2)
{
    float4x4 World;
    float2 BaseTextureTiling; // 기본 텍스처 타일링
    float2 DetailTextureTiling; // 디테일 텍스처 타일링
    float HeightScale; // 높이 스케일
    float3 Padding; // 16바이트 정렬을 위한 패딩
};

Texture2D heightMap : register(t0); // 높이맵
Texture2D baseTexture : register(t1); // 기본 텍스처
Texture2D detailTexture : register(t2); // 디테일 텍스처
SamplerState terrainSampler : register(s0); // 샘플러
struct VS_Input
{
    float3 PositionL : POSITION;
    float2 UV : TEXCOORD;
   
};
struct PS_Input
{
    float4 PositionH : SV_POSITION;
    float2 BaseUV : TEXCOORD0; // 기본 텍스처용 UV
    float2 DetailUV : TEXCOORD1; // 디테일 텍스처용 UV
   
};
  
  
PS_Input VS_Main(VS_Input input)
{
   
	PS_Input output = (PS_Input) 0;
  
	// 1. 높이맵 샘플링
  
	float height = heightMap.SampleLevel(terrainSampler, input.UV, 0).r;
  
     // 2. 정점 변위 적용
	input.PositionL.y += height * HeightScale;
   
	// 3. 표준 변환
	output.PositionH = mul(mul(Projection, View), mul(World, float4(input.PositionL, 1.0f)));
   
	// 4. 각 텍스처에 맞는 UV 좌표를 계산하여 픽셀 셰이더로 전달
	output.BaseUV = input.UV * BaseTextureTiling;
	output.DetailUV = input.UV * DetailTextureTiling;
   
	return output;
}
   
   
float4 PS_Main(PS_Input input) : SV_TARGET
{
    // 1. 각 텍스처 샘플링
	float4 baseColor = baseTexture.Sample(terrainSampler, input.BaseUV);

	float4 detailColor = detailTexture.Sample(terrainSampler, input.DetailUV);

	// 2. 두 색상 혼합
	// 곱셈 혼합(Modulate)을 통해 디테일을 추가합니다. (2.0을 곱해 밝기를 보정)

	float4 finalColor = baseColor * detailColor * 2.0;

   	return finalColor;
}