// b0: 기존 월드 행렬 (RenderComponent cbGameObjectInfo와 동일)
cbuffer cbWorldMatrix : register(b0)
{
    matrix g_matWorld;
    matrix g_matWorldInverseTranspose;
};

// b1: 3개 cascade의 LightViewProjection 행렬
cbuffer cbCascades : register(b1)
{
    matrix g_lightVP[3];
};

// VS
struct VS_INPUT_SHADOW
{
    float3 Position : POSITION;
    uint InstanceID : SV_InstanceID;
};

struct VS_OUTPUT_SHADOW
{
    float4 Position : SV_POSITION;
    uint InstanceID : TEXCOORD0; // GS에 넘길 cascade 번호
};

VS_OUTPUT_SHADOW VS_ShadowDepth(VS_INPUT_SHADOW input)
{
    VS_OUTPUT_SHADOW output;
    float4 worldPos = mul(float4(input.Position, 1.0f), g_matWorld);
    output.Position = mul(worldPos, g_lightVP[input.InstanceID]);
    output.InstanceID = input.InstanceID;
    return output;
}

// GS 
// 삼각형을 받아 SV_RenderTargetArrayIndex를 설정 → 올바른 슬라이스에 기록
struct GS_OUTPUT_SHADOW
{
    float4 Position : SV_POSITION;
    uint RTIndex : SV_RenderTargetArrayIndex;
};

[maxvertexcount(3)]
void GS_ShadowDepth(triangle VS_OUTPUT_SHADOW input[3],inout TriangleStream<GS_OUTPUT_SHADOW> stream)
{
    [unroll]
    for (int i = 0; i < 3; i++)
    {
        GS_OUTPUT_SHADOW output;
        output.Position = input[i].Position;
        output.RTIndex = input[i].InstanceID; // 0,1,2 → 슬라이스 0,1,2
        stream.Append(output);
    }
    stream.RestartStrip();
}