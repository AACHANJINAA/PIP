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


//-- 정점 셰이더와 픽셀 셰이더 간 데이터 전달 구조체 (이전과 동일) --//
struct VS_OUTPUT
{
    float3 WorldPosition : POSITION0;
    float3 WorldNormal : NORMAL0;
    float2 TexCoord : TEXCOORD0;
    float4 Position : SV_POSITION;
};

struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord0 : TEXCOORD; // 텍스쳐 좌표 (추가된 부분)
    float4 Tangent : TANGENT;
};


//-- 정점 셰이더 (로직은 이전과 동일) --//
//-- 정점 셰이더 (수정된 버전) --//
VS_OUTPUT VS_GLTF(VS_INPUT input)
{
    VS_OUTPUT Out;

    // 월드 변환 (이 부분은 문제가 없어 보입니다)
    Out.WorldPosition = mul(g_matWorld, float4(input.Position, 1.0f)).xyz;
    Out.WorldNormal = mul((matrix) g_matWorld, float4(input.Normal, 0.0f)).xyz;
    Out.WorldNormal = normalize(Out.WorldNormal);

    Out.TexCoord = input.TexCoord0;
    
    // WVP 행렬 계산 순서 수정
    // float4x4 matWVP = mul(g_matView, g_matProjection); // 기존 코드 (불필요)
    // matWVP = mul(g_matWorld, g_matView);              // 기존 코드 (순서 오류)
    // matWVP = mul(matWVP, g_matProjection);            // 기존 코드 (순서 오류)
    
    float4 positionW4 = mul(g_matWorld, float4(input.Position, 1.0f));

// 최종 좌표 계산
    Out.Position = mul(mul(positionW4, g_matView), g_matProjection);

    return Out;
}

//-- 픽셀 셰이더 수정 --//
float4 PS_GLTF(VS_OUTPUT In) : SV_TARGET
{
    // 텍스처에서 Albedo (기본 색상) 샘플링
    float4 cAlbedo = g_txDiffuse.Sample(g_samLinear, In.TexCoord);
    
    // 조명 계산을 위해 초기화
    float4 cFinalColor = float4(0.0f, 0.0f, 0.0f, 1.0f);

    // Ambient와 Emissive를 더함
    cFinalColor += g_Material.m_xmf4Ambient * cAlbedo;
    cFinalColor += g_Material.m_xmf4Emissive;
    
    float3 N = normalize(In.WorldNormal);
    float3 V = normalize(g_xmf3CameraPosition - In.WorldPosition);
    
    for (int i = 0; i < MAX_LIGHTS; ++i)
    {
        if (!g_Lights[i].m_bEnable) continue;
        
        float3 L;
        float fAttenuation = 1.0f;
        float fIntensity = 1.0f;
        
        if (g_Lights[i].m_nType == 0) // Directional Light
        {
            L = normalize(-g_Lights[i].m_xmf3Direction);
        }
        else // Point or Spot Light
        {
            L = g_Lights[i].m_xmf3Position - In.WorldPosition;
            float fDistance = length(L);
            L = normalize(L);
            
            fAttenuation = 1.0f / (g_Lights[i].m_xmf3Attenuation.x +
                                  g_Lights[i].m_xmf3Attenuation.y * fDistance +
                                  g_Lights[i].m_xmf3Attenuation.z * fDistance * fDistance);
            
            if (g_Lights[i].m_nType == 2) // Spot Light
            {
                float rho = dot(L, -normalize(g_Lights[i].m_xmf3Direction));
                if (rho < g_Lights[i].m_fPhi)
                    fIntensity = 0.0f;
                else
                    fIntensity = pow(saturate((rho - g_Lights[i].m_fPhi) / (g_Lights[i].m_fTheta - g_Lights[i].m_fPhi)), 2.0f);
            }
        }
        
        if (fIntensity > 0.0f && fAttenuation > 0.0f)
        {
            // Diffuse (난반사)
            float fNDotL = saturate(dot(N, L));
            float4 cDiffuse = fNDotL * g_Lights[i].m_xmf4Diffuse * cAlbedo;
                
            // Specular (정반사)
            float3 H = normalize(L + V);
            float fNDotH = saturate(dot(N, H));
            float fSpecularPower = g_Material.m_xmf4Specular.a;
            float4 cSpecular = float4(pow(fNDotH, fSpecularPower) * g_Lights[i].m_xmf4Specular.xyz * g_Material.m_xmf4Specular.rgb, 1.0f);
                
            cFinalColor.rgb += (cDiffuse.rgb + cSpecular.rgb) * fIntensity * fAttenuation;
        }
    }
    cFinalColor.a = cAlbedo.a;
    
    return cFinalColor;
}