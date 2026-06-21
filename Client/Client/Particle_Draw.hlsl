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

    // -------------------------------------------------------------------
    // 3. 날치알(단단한 알갱이) 느낌 제거 -> 아주 부드러운 안개/빛무리(Glow) 연출
    // -------------------------------------------------------------------
    // 중심(dist=0)에서 외곽(dist=0.5)으로 갈수록 아주 부드럽게 사라지는 감쇠 공식 적용
    float shapeAlpha = pow(1.0f - saturate(dist * 2.0f), 1.8f);

    // -------------------------------------------------------------------
    // 4. 색상 및 밝기: 흰색 -> 고유 색상(Player Color) 변화
    // -------------------------------------------------------------------
    // progress(0.0 ~ 1.0)에 따라 처음에는 순백색, 끝으로 갈수록 플레이어 고유 색상으로 변화
    float3 startColor = float3(1.0f, 1.0f, 1.0f);
    float3 timeBlendedColor = lerp(startColor, In.Color.rgb, progress);
    
    // 밝기: 처음엔 0.3배로 흐릿하다가, 마지막에 에너지가 응집되며 1.5배로 밝게 빛남
    float brightness = lerp(0.3f, 1.5f, progress);

    // [추가] 퍼질 때(죽어갈 때) 투명해지면서 회색/탁하게 변하는 것을 방지
    if (dying_progress > 0.0f) {
        // 흩어질 때는 온전한 본연의 색(고유 색상)을 100% 유지
        timeBlendedColor = In.Color.rgb;
        // 흩어지면서 사라질 때 색이 죽지 않도록 스스로 강렬하게 빛나게 부스트(Bloom)
        brightness *= lerp(1.0f, 3.0f, dying_progress);
    }

    float4 finalColor;
    finalColor.rgb = timeBlendedColor * brightness;
    
    // 처음 나타날 때 부드럽게 페이드인
    float appearAlpha = saturate(progress / 0.1f);
    
    // 알파 렌더링
    finalColor.a = In.Color.a * shapeAlpha * appearAlpha;

    return finalColor;
}