cbuffer cbPerFrame : register(b1)
{
    float4x4 View;
    float4x4 Projection;
}

cbuffer cbskybox : register(b2)
{
    float4x4 ViewNoTranslate;
    float4x4 SkyboxProjection;
}

TextureCube SkyboxTexture : register(t0);
SamplerState SkyboxSampler : register(s0);

struct VS_Input
{
    float3 VS_Input_PositionL : POSITION;
};

struct VS_Output
{
    float4 Position : SV_POSITION;
    float3 VS_output_PositionL : TEXCOORD0;
};

VS_Output VS_Main(VS_Input input)
{
	VS_Output output;

     // Çà·Ä °ö¼À ¼ø¼­ ¼öÁ¤ (DirectX row-major)
    float4 viewPosition = mul(float4(input.VS_Input_PositionL, 1.0f), ViewNoTranslate);
		
    output.Position = mul(viewPosition, SkyboxProjection);

    output.Position.z = output.Position.w;

    output.VS_output_PositionL = input.VS_Input_PositionL;

    return output;
}

float4 PS_Main(VS_Output input) : SV_TARGET
{
    float3 texDir = normalize(input.VS_output_PositionL);
    float4 skycolor = SkyboxTexture.Sample(SkyboxSampler, texDir);
    return skycolor;
   
    //float3 texdir = normalize(input.VS_output_PositionL);
    //return float4(texdir * 0.5f + 0.5f, 1.0f);
}