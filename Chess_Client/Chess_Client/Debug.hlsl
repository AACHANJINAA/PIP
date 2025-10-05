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

// [수정] 입력 구조체에서 position만 남깁니다.
struct VS_IN
{
    float3 position : POSITION;
};

  // 픽셀 셰이더로 전달할 구조체
struct VS_OUT
{
    float4 position : SV_POSITION;
};

VS_OUT VS_Debug(VS_IN vin)
{
    VS_OUT vout;
      // 강제 출력 코드는 그대로 유지합니다.
    vout.position = float4(vin.position.x * 0.1f, vin.position.y * 0.1f, 0.5f, 1.0f);
    return vout;
}

  // [수정] 픽셀 셰이더는 이제 아무것도 받지 않고, 무조건 녹색만 출력합니다.
float4 PS_Debug(VS_OUT pin) : SV_TARGET
{
    return float4(0.0, 1.0, 0.0, 1.0);
}