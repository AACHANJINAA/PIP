cbuffer cbGameObject : register(b0)
{
    matrix gmtxWorld;
};
cbuffer cbCamera : register(b1)
{
    matrix gmtxView;
    matrix gmtxProjection;
    float3 gvCameraPosition;
};

struct VS_IN
{
    float3 position : POSITION;
};
struct VS_OUT
{
    float4 position : SV_POSITION;
};

VS_OUT VS_Debug(VS_IN vin)
{
    VS_OUT vout;
    // 월드 -> 뷰 -> 투영 변환 순서 준수
    float4 worldPos = mul(float4(vin.position, 1.0f), gmtxWorld);
    float4 viewPos = mul(worldPos, gmtxView);
    vout.position = mul(viewPos, gmtxProjection);
    return vout;
}

float4 PS_Debug(VS_OUT pin) : SV_TARGET
{
    return float4(1.0f, 0.0f, 0.0f, 1.0f); // 디버그는 빨간색!
}