// Particle_Draw.hlsl

cbuffer cbCamera : register(b1) // 엔진 표준 카메라 레지스터
{
    matrix g_matView;
    matrix g_matProjection;
    float4 gvCameraPosition;
};

// 파티클 색상 및 크기
cbuffer cbParticle : register(b2)
{
    float4 g_Color;
    float g_Size;
    float progress; // 0~1 사이의 값으로, 파티클이 다 모였는지?
    float dying_progress; // 0 ~ 1로, 파티클이 사라지는 중인지?
};

// 컴퓨트 셰이더가 계산해둔 현재 파티클 위치 버퍼 (읽기 전용)
StructuredBuffer<float3> g_ParticleBuffer : register(t0);

// 반짝이는 빛 텍스처
Texture2D g_txParticle : register(t1);
SamplerState g_samLinear : register(s0);

struct VS_OUT
{
    float4 Pos : SV_POSITION;
    float2 UV : TEXCOORD0;
    float4 Color : COLOR;
};

// 가상의 쿼드(사각형)를 위한 4개의 UV와 로컬 좌표
static const float2 QuadUVs[4] = { float2(0, 0), float2(1, 0), float2(0, 1), float2(1, 1) };
static const float2 QuadPos[4] = { float2(-0.5, 0.5), float2(0.5, 0.5), float2(-0.5, -0.5), float2(0.5, -0.5) };

VS_OUT VS_Particle(uint vI : SV_VertexID, uint instI : SV_InstanceID)
{
    VS_OUT Out;
    
    // 인스턴스 ID를 통해 컴퓨트 셰이더가 계산한 월드 위치를 가져옵니다.
    float3 worldPos = g_ParticleBuffer[instI];

    // 카메라를 항상 정면으로 바라보도록 축(Right, Up) 계산 (Billboarding)
    float3 look = normalize(gvCameraPosition.xyz - worldPos);
    float3 right = normalize(cross(float3(0, 1, 0), look));
    float3 up = cross(look, right);

    // 0~3번 VertexID에 따라 사각형의 네 꼭짓점 위치로 벌려줍니다.
    float2 qpos = QuadPos[vI] * g_Size;
    worldPos += right * qpos.x + up * qpos.y;

    // 투영 변환
    Out.Pos = mul(float4(worldPos, 1.0f), g_matView);
    Out.Pos = mul(Out.Pos, g_matProjection);
    Out.UV = QuadUVs[vI];
    Out.Color = g_Color;
    
    // 알파값 서서히 줄이기
    // 진행도가 0.2(20%)를 넘어가면 서서히 투명해지기 시작해서 1.0일 때 완전히 사라짐
    float fadeProgress = saturate((dying_progress - 0.34f) / 0.34f);
    float alphaFade = 1.0f - fadeProgress;
    Out.Color.a *= alphaFade;
    
    return Out;
}

//float4 PS_Particle(VS_OUT In) : SV_TARGET
//{
//    // 텍스처의 색상과 투명도 샘플링
//    float4 texColor = g_txParticle.Sample(g_samLinear, In.UV);
//    return texColor * In.Color;
//}

float4 PS_Particle(VS_OUT In) : SV_TARGET
{
   // -------------------------------------------------------------------
    // 1. UV 좌표를 이용해 '부드러운 원(Soft Circle)' 형태 만들기
    // -------------------------------------------------------------------
    float dist = distance(In.UV, float2(0.5f, 0.5f));

    // 거리가 0.5(외곽)에 가까울수록 투명(0.0)해지고, 0.2(중심)로 갈수록 불투명(1.0)
    // 각진 네모가 아니라, 외곽이 은은하게 퍼지는 아름다운 동그란 빛무리(Orb)
    float shapeAlpha = smoothstep(0.5f, 0.2f, dist);

    // 외곽선이 칼같이 딱 떨어지는 선명한 동그라미
    // if (dist > 0.5f) discard;
    // float shapeAlpha = 1.0f;


    // -------------------------------------------------------------------
    // 2. 모이는 진행도(progress)에 따라 밝기를 쭈욱 끌어올리기
    // -------------------------------------------------------------------
    // progress(0.0 ~ 1.0)에 따라 밝기 배율을 정합니다.
    // 처음 사방으로 퍼져있을 땐 원래 색상의 0.3배(어두움), 
    // 검으로 다 모였을 땐 1.0배(빛이 폭발하듯 쨍해짐)가 됩니다.
    float brightness = lerp(0.3f, 1.0f, progress);


    // -------------------------------------------------------------------
    // 3. 최종 색상 조합
    // -------------------------------------------------------------------
    float4 finalColor;
    
    // RGB 색상에는 밝기 배율을 곱해줍니다. (값이 1.0을 넘어가면 Bloom 효과가 겹쳐 빛나게 됩니다)
    finalColor.rgb = In.Color.rgb * brightness;
    
    // Alpha(투명도)에는 원 모양으로 깎아낸 마스크(shapeAlpha)를 곱해줍니다.
    finalColor.a = In.Color.a * shapeAlpha;

    return finalColor;
}