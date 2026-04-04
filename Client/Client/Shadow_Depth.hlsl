// b0: 기존 월드 행렬 (RenderComponent cbGameObjectInfo와 동일)
cbuffer cbWorldMatrix : register(b0)
{
    matrix g_matWorld;
    matrix g_matWorldInverseTranspose;
};

// b1: 3개 cascade의 LightViewProjection 행렬
cbuffer cbCascades : register(b1)
{
    matrix g_lightVP;
};

// VS
struct VS_INPUT_SHADOW
{
    float3 Position : POSITION;
};

struct VS_OUTPUT_SHADOW
{
    float4 Position : SV_POSITION;
};

VS_OUTPUT_SHADOW VS_ShadowDepth(VS_INPUT_SHADOW input)
{
    VS_OUTPUT_SHADOW output;
    float4 worldPos = mul(float4(input.Position, 1.0f), g_matWorld);
    output.Position = mul(worldPos, g_lightVP);
    return output;
}