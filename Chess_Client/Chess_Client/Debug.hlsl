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

// 정점 입력 구조체 (위치와 색상만 필요)
struct VS_IN
{
    float3 position : POSITION;
    float4 color : COLOR;
};

// 픽셀 셰이더로 전달할 구조체
struct VS_OUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

VS_OUT VS_Debug(VS_IN vin)
{
    VS_OUT vout;
    // 최종 화면 좌표로 변환
    vout.position = mul(float4(vin.position, 1.0f), gmtxWorld);
    vout.position = mul(vout.position, gmtxView);
    vout.position = mul(vout.position, gmtxProjection);
    // 정점 색상을 픽셀 셰이더로 그대로 전달
    vout.color = vin.color;
    return vout;
}

float4 PS_Debug(VS_OUT pin) : SV_TARGET
{
    // 입력받은 색상을 그대로 최종 출력
    return pin.color;
}