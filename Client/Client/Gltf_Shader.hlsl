#define MAX_LIGHTS 8
#define MAX_MATERIALS 8

struct Light
{
    float4 m_xmf4Ambient;
    float4 m_xmf4Diffuse;
    float4 m_xmf4Specular;
    float3 m_xmf3Position;
    float m_fFalloff;
    float3 m_xmf3Direction;
    float m_fTheta; // cos(m_fTheta)
    float3 m_xmf3Attenuation;
    float m_fPhi; // cos(m_fPhi)
    int m_bEnable;
    int m_nType;
    float m_fRange;
    float padding;
};

struct Material
{
    float4 m_xmf4Ambient;
    float4 m_xmf4Diffuse;
    float4 m_xmf4Specular; //(r,g,b,a=power)
    float4 m_xmf4Emissive;
};

Texture2D g_txDiffuse : register(t0);
Texture2D g_txNormal : register(t1);
Texture2D g_txORM : register(t2); // Occlusion, Roughness, Metallic
Texture2D g_txEmissive : register(t3);
SamplerState g_samLinear : register(s0);

// 객체별 월드 행렬
cbuffer cbWorldMatrix : register(b0)
{
    matrix g_matWorld;
};

// 카메라 정보
cbuffer cbCamera : register(b1)
{
    matrix g_matView;
    matrix g_matProjection;
    float4 g_xmf3CameraPosition;
};

// 재질 정보
cbuffer cbMaterial : register(b2)
{
    Material g_Material;
};

// 조명 정보
cbuffer cbLights : register(b3)
{
    Light g_Lights[MAX_LIGHTS];
};


struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord0 : TEXCOORD; // 텍스쳐 좌표 (추가된 부분)
    float4 Tangent : TANGENT;
};

//-- 정점 셰이더와 픽셀 셰이더 간 데이터 전달 구조체 (이전과 동일) --//
struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float3 WorldPosition : POSITION0;
    float2 TexCoord : TEXCOORD0;
    float3 Normal : NORMAL0;
    float3 Tangent : TANGENT0;
    float3 Bitangent : BITANGENT0;
};

///-- 정점 셰이더 (로직은 이전과 동일) --///
//-- 정점 셰이더 (수정된 버전) --//
VS_OUTPUT VS_GLTF(VS_INPUT input)
{
        VS_OUTPUT Out;

        Out.WorldPosition = mul(float4(input.Position, 1.0f), g_matWorld).xyz;
        Out.Position = mul(float4(Out.WorldPosition, 1.0f), g_matView);
        Out.Position = mul(Out.Position, g_matProjection);
    
        Out.TexCoord = input.TexCoord0;

    // 월드 공간 기준으로 Normal, Tangent, Bitangent를 계산하여 전달
        Out.Normal = normalize(mul((float3x3) g_matWorld, input.Normal));
        Out.Tangent = normalize(mul((float3x3) g_matWorld, input.Tangent.xyz));
        Out.Bitangent = normalize(cross(Out.Normal, Out.Tangent) * input.Tangent.w);

        return Out;
}


const float PI = 3.14159265359;

// D: Normal Distribution Function (GGX)
float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = saturate(dot(N, H));
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / denom;
}

// G: Geometry Function (Smith's method with Schlick-GGX)
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;
    return nom / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = saturate(dot(N, V));
    float NdotL = saturate(dot(N, L));
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

// F: Fresnel Equation (Schlick's approximation)
float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// ▲▲▲▲▲ 여기까지 PBR 표준 함수들 ▲▲▲▲▲


float4 PS_GLTF(VS_OUTPUT In) : SV_TARGET
{
    // 1. 모든 텍스처에서 서피스(표면) 속성 샘플링
    float4 albedoMap = g_txDiffuse.Sample(g_samLinear, In.TexCoord);
    float3 normalMap = g_txNormal.Sample(g_samLinear, In.TexCoord).rgb;
    float3 ormMap = g_txORM.Sample(g_samLinear, In.TexCoord).rgb;
    float3 emissiveMap = g_txEmissive.Sample(g_samLinear, In.TexCoord).rgb;

    // 2. 노멀맵 계산 (탄젠트 공간 -> 월드 공간)
    float3 N_tangent = normalMap * 2.0 - 1.0; // [0, 1] 범위를 [-1, 1] 범위로 변환
    float3x3 TBN = float3x3(normalize(In.Tangent), normalize(In.Bitangent), normalize(In.Normal));
    float3 N = normalize(mul(N_tangent, TBN)); // 최종적으로 사용할 표면 법선 벡터

    // 3. PBR 변수 준비
    float3 albedo = albedoMap.rgb;
    float ao = ormMap.r; // Ambient Occlusion
    float roughness = ormMap.g; // Roughness
    float metallic = ormMap.b; // Metallic

    float3 V = normalize(g_xmf3CameraPosition.xyz - In.WorldPosition);
    float3 F0 = lerp(0.04, albedo, metallic); // Fresnel 반사율 F0 계산

    // 4. 조명 계산 시작 (IBL은 제외하고 직접 조명만 계산)
    float3 Lo = float3(0.0, 0.0, 0.0); // 최종 반사될 빛의 양
    for (int i = 0; i < MAX_LIGHTS; ++i)
    {
        if (!g_Lights[i].m_bEnable)
            continue;

        float3 L = normalize(g_Lights[i].m_xmf3Position - In.WorldPosition); // Point Light 기준
        float3 H = normalize(V + L);
        float distance = length(g_Lights[i].m_xmf3Position - In.WorldPosition);
        float attenuation = 1.0 / (distance * distance);
        float3 radiance = g_Lights[i].m_xmf4Diffuse.rgb * attenuation;

        // Cook-Torrance BRDF 계산
        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        float3 F = FresnelSchlick(saturate(dot(H, V)), F0);

        float3 kD = (1.0 - F) * (1.0 - metallic); // Diffuse 반사율
        float NdotL = saturate(dot(N, L));

        float3 numerator = NDF * G * F;
        float denominator = 4.0 * saturate(dot(N, V)) * NdotL + 0.0001; // 0으로 나누는 것 방지
        float3 specular = numerator / denominator;
        
        // 최종 조명 추가
        Lo += (kD * albedo / PI + specular) * radiance * NdotL;
    }

    // 5. 최종 색상 조합
    // Ambient Occlusion 적용, Emissive(자체 발광) 추가
    float3 ambient = float3(0.03, 0.03, 0.03) * albedo * ao;
    float3 color = ambient + Lo + emissiveMap;
    
    // HDR to LDR, 감마 보정 등 추가적인 톤 매핑이 필요할 수 있음
    color = color / (color + float3(1.0, 1.0, 1.0));
    color = pow(color, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));

    return float4(color, albedoMap.a);
}