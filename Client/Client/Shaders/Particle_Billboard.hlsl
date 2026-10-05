// Particle_Billboard.hlsl
// 범용 파티클 그리기: 풀의 파티클 하나당 사각형 하나 (ParticleSystem_Plan_KR.md 3.2)
// 죽은 파티클은 사각형을 한 점으로 모아 그리지 않는다(정렬·압축 없음).

#define PARTICLE_EMITTER_REGISTER b2
#include "Particle_Common.hlsli"

cbuffer cbCamera : register(b1) // 엔진 표준 카메라 레지스터
{
    matrix g_matView;
    matrix g_matProjection;
    float4 gvCameraPosition;
};

StructuredBuffer<Particle> g_Pool : register(t0);

struct VS_OUT
{
    float4 Pos : SV_POSITION;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR;
};

static const float2 QuadUVs[4] = { float2(0, 0), float2(1, 0), float2(0, 1), float2(1, 1) };
static const float2 QuadPos[4] = { float2(-0.5, 0.5), float2(0.5, 0.5), float2(-0.5, -0.5), float2(0.5, -0.5) };

VS_OUT VS_Billboard(uint vI : SV_VertexID, uint instI : SV_InstanceID)
{
    VS_OUT Out;
    Particle p = g_Pool[instI];

    if (!particle_alive(p))
    {
        Out.Pos = float4(0.0f, 0.0f, -1.0f, 1.0f); // 네 꼭짓점이 같은 점, 화면 밖 (그려지지 않음)
        Out.UV = float2(0.0f, 0.0f);
        Out.Color = float4(0.0f, 0.0f, 0.0f, 0.0f);
        return Out;
    }

    float t = saturate(p.age / p.lifetime);
    float size = p.size * lerp(1.0f, g_EndSizeMultiplier, t);
    float4 color = lerp(unpack_color(p.color), g_EndColor, t);

    float4 viewPos = mul(float4(p.pos, 1.0f), g_matView);
    float2 corner = QuadPos[vI];
    float2 offset;

    if (g_RenderMode == 1)
    {
        // 속도 방향으로 늘림: 화면(뷰 공간)에서의 속도 방향을 긴 축으로
        float2 v = mul(float4(p.vel, 0.0f), g_matView).xy;
        float lenV = length(v);
        float2 axisX = lenV > 1e-4f ? v / lenV : float2(1.0f, 0.0f);
        float2 axisY = float2(-axisX.y, axisX.x);
        float stretch = size + length(p.vel) * g_LengthScale;
        offset = axisX * corner.x * stretch + axisY * corner.y * size;
    }
    else
    {
        float s = sin(p.rotation);
        float c = cos(p.rotation);
        offset = float2(c * corner.x - s * corner.y, s * corner.x + c * corner.y) * size;
    }

    viewPos.xy += offset;
    Out.Pos = mul(viewPos, g_matProjection);
    Out.UV = QuadUVs[vI];
    Out.Color = color;
    return Out;
}

float4 PS_Billboard(VS_OUT In) : SV_TARGET
{
    // 절차적 부드러운 원 (늘린 사각형에서는 긴 타원)
    float dist = distance(In.UV, float2(0.5f, 0.5f)) * 2.0f;
    float shape = pow(saturate(1.0f - dist), 1.5f);
    return float4(In.Color.rgb, In.Color.a * shape);
}
