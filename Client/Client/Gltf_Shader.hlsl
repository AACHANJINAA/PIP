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
    float3 g_xmf3CameraPosition;
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
VS_OUTPUT VS_GLTF(VS_INPUT input)
{
    VS_OUTPUT Out;

    Out.WorldPosition = mul(g_matWorld, float4(input.Position, 1.0f)).xyz;
    Out.WorldNormal = mul((matrix) g_matWorld, float4(input.Normal, 0.0f)).xyz;
    Out.WorldNormal = normalize(Out.WorldNormal);

    Out.TexCoord = input.TexCoord0;
    
    float4x4 matWVP = mul(g_matProjection, g_matView);
    matWVP = mul(matWVP, g_matWorld);
    Out.Position = mul(matWVP, float4(input.Position, 1.0f));

    return Out;
}

//-- 픽셀 셰이더 (로직은 이전과 동일) --//
float4 PS_GLTF(VS_OUTPUT In) : SV_TARGET
{
    float4 cFinalColor = float4(0.0f, 0.0f, 0.0f, 1.0f);
    
    float4 cAlbedo = g_Material.m_xmf4Diffuse;

    cFinalColor += (g_Material.m_xmf4Ambient * cAlbedo);
    cFinalColor += g_Material.m_xmf4Emissive;
    
    float3 N = normalize(In.WorldNormal);
    float3 V = normalize(g_xmf3CameraPosition - In.WorldPosition);

    for (int i = 0; i < MAX_LIGHTS; ++i)
    {
        if (!g_Lights[i].m_bEnable)
            continue;

        float3 L;
        float fAttenuation = 1.0f;
        float fIntensity = 1.0f;

        if (g_Lights[i].m_nType == 0) // Directional
        {
            L = -normalize(g_Lights[i].m_xmf3Direction);
        }
        else // Point or Spot
        {
            L = g_Lights[i].m_xmf3Position - In.WorldPosition;
            float fDistance = length(L);
            L = normalize(L);
            fAttenuation = 1.0f / (g_Lights[i].m_xmf3Attenuation.x + g_Lights[i].m_xmf3Attenuation.y * fDistance + g_Lights[i].m_xmf3Attenuation.z * fDistance * fDistance);
            
            if (g_Lights[i].m_nType == 2) // Spot
            {
                float rho = dot(L, -normalize(g_Lights[i].m_xmf3Direction));
                if (rho < g_Lights[i].m_fPhi)
                    fIntensity = 0.0f;
                else
                    fIntensity = pow(saturate((rho - g_Lights[i].m_fPhi) / (g_Lights[i].m_fTheta - g_Lights[i].m_fPhi)), 2.0);
            }
        }
        
        if (fIntensity > 0.0f && fAttenuation > 0.0f)
        {
            float fNDotL = saturate(dot(N, L));
            float4 cDiffuse = fNDotL * g_Lights[i].m_xmf4Diffuse * cAlbedo;

            float3 H = normalize(L + V);
            float fNDotH = saturate(dot(N, H));
            float fSpecularPower = g_Material.m_xmf4Specular.a;
            float4 cSpecular = float4(pow(fNDotH, fSpecularPower) * g_Lights[i].m_xmf4Specular.xyz * g_Material.m_xmf4Specular.rgb, 1.0f);

            cFinalColor.rgb += (cDiffuse.rgb + cSpecular.rgb) * fIntensity * fAttenuation;
        }
    }
    
    // 임시로 추가
    //cFinalColor.a = 1.0f;
    cFinalColor.rgba = (0.5f, 0.5f, 0.5f, 1.f);

    return cFinalColor;
}