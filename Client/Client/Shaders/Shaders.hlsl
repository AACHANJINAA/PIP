//게임 객체의 정보를 위한 상수 버퍼를 선언한다. 
#define MAX_LIGHTS 8
#define MAX_MATERIALS 8

// C++의 MATERIAL 구조체와 일치해야함
struct MATERIAL
{
    float4 m_xmf4Ambient;
    float4 m_xmf4Diffuse;
    float4 m_xmf4Specular; 
    float4 m_xmf4Emissive;
};

// C++의 LIGHT 구조체와 일치해야함
struct LIGHT
{
    float4 m_xmf4Ambient;
    float4 m_xmf4Diffuse;
    float4 m_xmf4Specular;
    float3 m_xmf3Position;
    float m_fFalloff;
    float3 m_xmf3Direction;
    float m_fTheta; //cos(m_fTheta)
    float3 m_xmf3Attenuation;
    float m_fPhi; //cos(m_fPhi)
    bool m_bEnable;
    int m_nType;
    float m_fRange;
    float padding;
};

// C++의 LIGHTS 구조체와 일치시켜야 합니다.
struct LIGHTS
{
    LIGHT m_pLights[MAX_LIGHTS];
    float4 m_xmf4GlobalAmbient;
};

// C++의 MATERIALS 구조체와 일치시켜야 합니다.
struct MATERIALS
{
    MATERIAL m_pReflections[MAX_MATERIALS];
};



cbuffer cbGameObjectInfo : register(b0)
{
    matrix gmtxWorld : packoffset(c0);
};

//카메라의 정보를 위한 상수 버퍼를 선언한다. 
cbuffer cbCameraInfo : register(b1)
{
    matrix gmtxView : packoffset(c0);
    matrix gmtxProjection : packoffset(c4);
    float4 gf4Position : packoffset(c8);
};

// C++에서 SetGraphicsRootConstantBufferView로 전달하는 상수 버퍼
cbuffer cbMaterials : register(b2)
{
    MATERIALS gMaterials;
};

cbuffer cbLights : register(b3)
{
    LIGHTS gLights;
};

// 정점 셰이더의 입력을 위한 구조체 (위치와 법선)
struct VS_LIGHTING_INPUT
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD0; // 텍스쳐 좌표 (추가된 부분)
    float4 color : COLOR; // 기존 색상 정보도 그대로 가져오기
};

// 픽셀 셰이더의 입력을 위한 구조체
struct PS_LIGHTING_INPUT
{
    float4 positionH : SV_POSITION; // 최종 변환된 2D 화면 좌표
    float4 color : COLOR; // 정점의 기본 색상
    float3 positionW : POSITION; // 월드 좌표계에서의 위치
    float3 normalW : NORMAL; // 월드 좌표계에서의 법선 벡터
    float2 texcoord : TEXCOORD0; // 텍스쳐 좌표 (추가된 부분)
};

// --- 새로운 정점/픽셀 셰이더 함수 ---

// 정점 셰이더: VSLighting
PS_LIGHTING_INPUT VSLighting(VS_LIGHTING_INPUT input)
{
    PS_LIGHTING_INPUT output;

    // 정점의 위치를 월드 좌표계로 변환
    output.positionW = (float3) mul(float4(input.position, 1.0f), gmtxWorld);
    // 최종 화면 좌표로 변환
    output.positionH = mul(mul(float4(output.positionW, 1.0f), gmtxView), gmtxProjection);
    // 법선 벡터를 월드 좌표계로 변환 (방향이므로 float3x3 사용)
    output.normalW = mul(input.normal, (float3x3) gmtxWorld);
    
    // 정점의 고유 색상은 그대로 전달
    output.color = input.color;
    output.texcoord = input.texcoord;

    return output;
}


// 픽셀 셰이더: PSLighting (퐁 조명 계산의 핵심)
float4 PSLighting(PS_LIGHTING_INPUT input) : SV_TARGET
{
    // 최종적으로 계산될 픽셀의 색상 (초기값은 검은색)
    float4 f4TotalColor = float4(0, 0, 0, 1);
    
    // 법선 벡터를 정규화 (크기를 1로 만듦)
    float3 N = normalize(input.normalW);
    
    // 모든 조명(MAX_LIGHTS 개)에 대해 계산을 반복
    for (int i = 0; i < MAX_LIGHTS; i++)
    {
        // 켜져 있는 조명만 계산
        if (gLights.m_pLights[i].m_bEnable)
        {
            // 지금은 재질을 0번만 사용한다고 가정
            MATERIAL material = gMaterials.m_pReflections[0];

            // 1. 환경광(Ambient) 계산: 빛의 방향과 상관없이 은은하게 깔리는 빛
            float4 f4Ambient = gLights.m_pLights[i].m_xmf4Ambient * material.m_xmf4Ambient * input.color;
            f4TotalColor += f4Ambient;
            
            //f4TotalColor *= input.color;

            // 2. 난반사(Diffuse) 계산: 빛의 방향과 표면의 방향에 따라 밝기가 결정되는 빛 (입체감)
            float3 L = normalize(gLights.m_pLights[i].m_xmf3Direction); // 빛의 방향
            float fDiffuseFactor = max(dot(N, -L), 0.f); // 빛과 법선 벡터의 내적
            float4 f4Diffuse = fDiffuseFactor * gLights.m_pLights[i].m_xmf4Diffuse * input.color; // DW수정 input의 색상을 곱해주어야 함 
            f4TotalColor += f4Diffuse;
            
            // 3. 정반사(스펙큘러 항) 계산 : 카메라의 방향과 빛의 방향에 따라 더 밝게 빛나는 부분 표현 (반짝임)
            float3 V = normalize(gf4Position.xyz - input.positionW);
            V = reflect(V,input.normalW);
            float Shigning = max(dot(-L, V), 0.f); // (점 -> 빛), (점 -> 카메라) 방향벡터 내적
            
            float SpecualNum = 64.f;
            
            float4 f4Specular = gLights.m_pLights[i].m_xmf4Specular * material.m_xmf4Specular * pow(Shigning, SpecualNum);
            
            f4TotalColor += f4Specular;

        }
    }
    
    // 전역 환경광 추가
    f4TotalColor += gLights.m_xmf4GlobalAmbient;
    
    // 오브젝트 고유의 색상과 빛 계산 결과를 곱하여 최종 색 결정
    // input.color는 정점의 원래 색상 (예: 체스판의 밝은 칸, 어두운 칸)
    
    //return f4TotalColor * input.color; // DW수정 아마 이것 때문에 검은색은 0으로 다시 초기화 되는 것 같음
    
    return f4TotalColor; // DW수정
}
