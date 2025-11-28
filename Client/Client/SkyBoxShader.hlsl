cbuffer cbPerFrame : register(b0)
{
    float4x4 View;
    float4x4 Projection;
}

cbuffer cbskybox : register(b1)
{
    float4x4 ViewNoTranslate;
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

    float4 worldPosition = mul(float4(input.VS_Input_PositionL, 1.0f), ViewNoTranslate);
		
    output.Position = mul(worldPosition, Projection);

    output.Position.z = output.Position.w;

    output.VS_output_PositionL = input.VS_Input_PositionL;

    return output;
}

float4 PS_Main(VS_Output input) : SV_TARGET
{
    float3 texDir = normalize(input.VS_output_PositionL);
    float4 skyColor = SkyboxTexture.Sample(SkyboxSampler, texDir);
    return skyColor;
}