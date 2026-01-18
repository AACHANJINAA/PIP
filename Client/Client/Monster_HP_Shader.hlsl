// 1. 상수 버퍼
// 카메라 정보
cbuffer cbCamera : register(b1)
{
    matrix g_matView;
    matrix g_matProjection;
    float4 gvCameraPosition;
};

cbuffer cbHPBar : register(b2)
{
    float2 g_Size; // 모든 몬스터가 공유하는 "화면상 절대 크기"
};

struct VS_INPUT
{
    float3 PosW : POSITION; // 정점 버퍼에서 오는 몬스터 위치
    float hpRatio : TEXCOORD0; // 정점 버퍼에서 오는 각 몬스터의 HP 비율
};

struct GS_OUTPUT
{
    float4 PosH : SV_POSITION;
    float2 UV : TEXCOORD0;
    nointerpolation uint TexIndex : SV_InstanceID; // 보간 하면 안됨
};

// 3. 리소스 (t0: 체력바 알맹이, t1: 테두리 프레임)
Texture2D g_HPBarTex : register(t0); // HP_Bar.dds
Texture2D g_FrameTex : register(t1); // HP_Bar_Frame.dds
SamplerState g_Sampler : register(s0);

// 4. Vertex Shader
VS_INPUT VS(VS_INPUT input)
{
    return input;
}

// 5. Geometry Shader
[maxvertexcount(8)]
void GS(point VS_INPUT input[1], inout TriangleStream<GS_OUTPUT> outputStream)
{
    GS_OUTPUT v;
    
    matrix VP = mul(g_matView, g_matProjection);
    
    // 원근 투영 변환 및 고정 크기(Billboarding)를 위한 W값 추출
    float4 clipPos = mul(float4(input[0].PosW, 1.0f), VP);
    float w = g_Size.x * clipPos.w;
    float h = g_Size.y * clipPos.w;

    // 배경 프레임 그리기
    v.TexIndex = 1; // 프레임은 1번
    float4 frameOffsets[4] =
    {
        float4(-w, h, 0.001f * clipPos.w, 0), float4(w, h, 0.001f * clipPos.w, 0),
        float4(-w, -h, 0.001f * clipPos.w, 0), float4(w, -h, 0.001f * clipPos.w, 0)
    };
    [unroll]
    for (int i = 0; i < 4; i++)
    {
        v.PosH = clipPos + frameOffsets[i];
        v.UV = float2(i % 2, i / 2);
        outputStream.Append(v);
    }
    outputStream.RestartStrip();

    // 체력 바 알맹이 그리기
    v.TexIndex = 0; // 체력 바는 0번
    float innerW = w * 0.94f;
    float innerH = h * 0.60f;
    const float barUV_StartX = 25.0f / 512.0f;
    const float barUV_EndX = 393.0f / 512.0f;
    const float barUV_StartY = 4.0f / 16.0f;
    const float barUV_EndY = 12.0f / 16.0f;

    float monsterRatio = input[0].hpRatio;
    float currentBarUV_EndX = barUV_StartX + (barUV_EndX - barUV_StartX) * monsterRatio;
    float rightOffset = -innerW + (innerW * 2.0f * monsterRatio);
    float zDepth = 0.0f;

    // 왼쪽 위, 오른쪽 위, 왼쪽 아래, 오른쪽 아래
    v.PosH = clipPos + float4(-innerW, innerH, zDepth, 0);
    v.UV = float2(barUV_StartX, barUV_StartY);
    outputStream.Append(v);
    
    v.PosH = clipPos + float4(rightOffset, innerH, zDepth, 0);
    v.UV = float2(currentBarUV_EndX, barUV_StartY);
    outputStream.Append(v);
    
    v.PosH = clipPos + float4(-innerW, -innerH, zDepth, 0);
    v.UV = float2(barUV_StartX, barUV_EndY);
    outputStream.Append(v);
    
    v.PosH = clipPos + float4(rightOffset, -innerH, zDepth, 0);
    v.UV = float2(currentBarUV_EndX, barUV_EndY);
    outputStream.Append(v);

    outputStream.RestartStrip();
}

// 6. Pixel Shader
float4 PS(GS_OUTPUT input) : SV_Target
{
    float4 color;
    
    // TexIndex 값에 따라 샘플링할 텍스처를 결정
    if (input.TexIndex == 1)
    {
        color = g_FrameTex.Sample(g_Sampler, input.UV);
    }
    else
    {
        color = g_HPBarTex.Sample(g_Sampler, input.UV);
    }
    
    return color;
}