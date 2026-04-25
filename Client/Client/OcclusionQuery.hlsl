 cbuffer cbGameObject : register(b0)
{
    float4x4 gWorld;
};
cbuffer cbCamera : register(b1)
{
    float4x4 gView;
    float4x4 gProj;
    float4 gvCameraPosition;
};

struct VS_INPUT
{
    float3 position : POSITION;
};
struct VS_OUTPUT
{
    float4 position : SV_POSITION;
};

VS_OUTPUT VS_Main(VS_INPUT input)
{
    VS_OUTPUT output;
    float4 worldPos = mul(float4(input.position, 1.0f), gWorld);
    float4 viewPos = mul(worldPos, gView);
    output.position = mul(viewPos, gProj);
    return output;
}

float4 PS_Main(VS_OUTPUT input) : SV_TARGET
{
    return float4(0.0f, 1.0f, 0.0f, 1.0f);
}
