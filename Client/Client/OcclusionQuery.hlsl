 cbuffer cbGameObject : register(b0)
{
    float4x4 gWorld;
};
cbuffer cbCamera : register(b1)
{
    float4x4 gViewProj;
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
    output.position = mul(worldPos, gViewProj);
    return output;
}

 // Pixel Shader는 색상을 기록하지 않으므로 비워둡니다.
void PS_Main()
{
}