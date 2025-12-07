#define MAX_LIGHTS 8
#define MAX_MATERIALS 8

struct MATERIAL
{
    float4 m_xmf4Ambient;
    float4 m_xmf4Diffuse;
    float4 m_xmf4Specular;
    float4 m_xmf4Emissive;
};

struct LIGHT
{
    float4 m_xmf4Ambient;
    float4 m_xmf4Diffuse;
    float4 m_xmf4Specular;
    float3 m_xmf3Position;
    float m_fFalloff;
    float3 m_xmf3Direction;
    float m_fTheta;
    float3 m_xmf3Attenuation;
    float m_fPhi;
    bool m_bEnable;
    int m_nType;
    float m_fRange;
    float padding;
};

// [b0] World Matrix
cbuffer cbPerObject : register(b0)
{
    float4x4 World;
};

cbuffer cbCameraInfo : register(b1)
{
    matrix gmtxView;
    matrix gmtxProjection;
    float4 gf4Position; // Light
};

cbuffer cbMaterials : register(b2)
{
    MATERIAL gMaterials[MAX_MATERIALS];
}

cbuffer cbLights : register(b3)
{
    LIGHT gLights[MAX_LIGHTS];
    float4 m_xmf4GlobalAmbient;
};

Texture2D baseTexture : register(t0);
Texture2D detailTexture : register(t1);
SamplerState terrainSampler : register(s0);

struct VS_Input
{
    float3 PositionL : POSITION;
    float3 NormalL : NORMAL;
	float2 UV : TEXCOORD;
};

struct PS_Input
{
    float4 PositionH : SV_POSITION;
    float3 PositionW : POSITION;
    float3 NormalW : NORMAL;
    float2 UV : TEXCOORD0;
};

PS_Input VS_Main(VS_Input input)
{
    PS_Input output = (PS_Input) 0;

    // 이미 CPU에서 높이가 계산된 정점 위치를 사용
    output.PositionW = mul(float4(input.PositionL, 1.0f), World).xyz;
    output.NormalW = mul(input.NormalL, (float3x3) World);
    output.PositionH = mul(mul(float4(output.PositionW, 1.0f), gmtxView), gmtxProjection);
    output.UV = input.UV;

    return output;
}

float4 PS_Main(PS_Input input) : SV_TARGET
{
    float4 sampledColor = baseTexture.Sample(terrainSampler, input.UV);

    if (dot(sampledColor.rgb, float3(1.0, 1.0, 1.0)) < 0.01) // RGB 합이 0.01보다 작으면 검은색으로 간주
    {
        return float4(1.0, 0.0, 1.0, 1.0); // 마젠타 (Magenta)
    }

    return baseTexture.Sample(terrainSampler, input.UV);
    float4 baseColor = baseTexture.Sample(terrainSampler, input.UV * 10.0); // 타일링 값 조절
    float4 detailColor = detailTexture.Sample(terrainSampler, input.UV * 5.0);
    float4 texColor = baseColor * detailColor * 1.5;
    

    float3 N = normalize(input.NormalW);
    float4 totalColor = float4(0, 0, 0, 1);
	
	// 이 재질은 C++에서 cbMaterials의 첫 번째(인덱스 0) 재질로 설정해주어야 합니다.
    MATERIAL material = gMaterials[0];
    
    for (int i = 0; i < MAX_LIGHTS; i++)
        {
        
        if (gLights[i].m_bEnable)
            {
            float4 ambient = gLights[i].m_xmf4Ambient * material.m_xmf4Ambient;
            totalColor += ambient;
            
            float3 L = normalize(gLights[i].m_xmf3Direction);
            float diffuseFactor = max(dot(N, -L), 0.0f);
            float4 diffuse = diffuseFactor * gLights[i].m_xmf4Diffuse * material.m_xmf4Diffuse;
            totalColor += diffuse;
           
            float3 V = normalize(gf4Position.xyz - input.PositionW);
      
            float3 R = reflect(L, N);
          
            float specularFactor = pow(max(dot(R, V), 0.0f), material.m_xmf4Specular.w); // w에 shininess 저장 가정
         
            float4 specular = specularFactor * gLights[i].m_xmf4Specular * material.m_xmf4Specular;
           
            totalColor += specular;
        }
    }

    totalColor += m_xmf4GlobalAmbient;
    totalColor.a = 1.0f;
    
    return texColor * totalColor;
}