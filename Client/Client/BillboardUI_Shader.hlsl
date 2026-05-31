// ==============================================================================
// BillboardUI_Shader.hlsl
// 퀘스트 마커 및 UI를 위한 지오메트리 쉐이더 빌보드
// ==============================================================================

cbuffer cbCamera : register(b1)
{
    matrix g_matView;
    matrix g_matProjection;
    float4 gvCameraPosition;
};

cbuffer cbMarker : register(b2)
{
    float2 g_Size;      // 빌보드의 가로 세로 크기
    float  g_Alpha;     // 투명도 (알파값)
    float  padding;
    float4 g_Color;     // 기본 색상 (RGB)
};

struct VS_INPUT
{
    float3 PosW : POSITION; // 마커의 월드 좌표 위치
};

struct GS_OUTPUT
{
    float4 PosH : SV_POSITION;
    float2 UV : TEXCOORD0;
};

Texture2D g_Texture : register(t0);
SamplerState g_Sampler : register(s0);

// 버텍스 쉐이더: 단순히 월드 좌표 위치를 지오메트리 쉐이더로 넘김
VS_INPUT VS(VS_INPUT input)
{
    return input;
}

// 지오메트리 쉐이더: 단일 정점을 카메라를 바라보는 사각형(Quad)으로 확장
[maxvertexcount(4)]
void GS(point VS_INPUT input[1], inout TriangleStream<GS_OUTPUT> outputStream)
{
    float3 posW = input[0].PosW;

    // 카메라 기준 벡터 계산 (에지 케이스 방지)
    float3 up = float3(0.0f, 1.0f, 0.0f);
    float3 look = normalize(gvCameraPosition.xyz - posW);
    
    // 만약 look 벡터가 up 벡터와 거의 평행하다면 임의의 right 벡터 사용 (NaN 방지)
    float3 right;
    if (abs(dot(up, look)) > 0.999f)
    {
        right = float3(1.0f, 0.0f, 0.0f);
    }
    else
    {
        right = normalize(cross(up, look));
    }
    
    // 완벽하게 직교하도록 Up 벡터 재계산
    up = normalize(cross(look, right));

    float halfWidth = g_Size.x * 0.5f;
    float halfHeight = g_Size.y * 0.5f;

    float4 v[4];
    v[0] = float4(posW + halfWidth * right - halfHeight * up, 1.0f); // 우측 하단
    v[1] = float4(posW + halfWidth * right + halfHeight * up, 1.0f); // 우측 상단
    v[2] = float4(posW - halfWidth * right - halfHeight * up, 1.0f); // 좌측 하단
    v[3] = float4(posW - halfWidth * right + halfHeight * up, 1.0f); // 좌측 상단

    float2 uv[4] =
    {
        float2(1.0f, 1.0f),
        float2(1.0f, 0.0f),
        float2(0.0f, 1.0f),
        float2(0.0f, 0.0f)
    };

    GS_OUTPUT output;
    for (int i = 0; i < 4; ++i)
    {
        float4 posV = mul(v[i], g_matView);
        output.PosH = mul(posV, g_matProjection);
        output.UV = uv[i];
        outputStream.Append(output);
    }
}

// 픽셀 쉐이더
float4 PS(GS_OUTPUT input) : SV_TARGET
{
    float4 color = g_Texture.Sample(g_Sampler, input.UV);
    
    // 원래 로직 (임시 주석 처리)
    color *= g_Color;
    color.a *= g_Alpha;
    //if (color.a < 0.01f)
    //    discard;
    return color;
}
