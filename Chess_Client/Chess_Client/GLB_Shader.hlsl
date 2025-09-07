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
struct VS_SKINNED_INPUT
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD0;
    float4 boneIndices : BLENDINDICES;
    float4 boneWeights : BLENDWEIGHTS;
};

// 픽셀 셰이더의 입력을 위한 구조체
struct PS_SKINNED_INPUT
{
    float4 positionH : SV_POSITION; // H = Homogeneous (동차) -> 투영 변환까지 마친 좌표계
    float3 positionW : POSITION; // W = World (월드) 좌표계
    float3 normalW : NORMAL;
    float2 texcoord : TEXCOORD0;
};


// 스키닝 계산을 위한 뼈 변환 행렬 배열 (상수 버퍼)
cbuffer cbSkinningInfo : register(b4) // 기존 버퍼들과 겹치지 않는 레지스터(b4) 사용
{
    matrix gBoneTransforms[128]; // 최대 뼈 개수 (임의로 128로 설정)
};

// --- [수정] 정점 셰이더 함수 ---
PS_SKINNED_INPUT VSSkinning(VS_SKINNED_INPUT input)
{
    PS_SKINNED_INPUT output = (PS_SKINNED_INPUT) 0;

    // 월드 좌표 계산 (VSLighting과 동일한 순서로 수정)
    output.positionW = (float3) mul(float4(input.position, 1.0f), gmtxWorld);

    // 최종 화면 좌표 계산 (VSLighting과 동일한 순서로 수정)
    output.positionH = mul(mul(float4(output.positionW, 1.0f), gmtxView), gmtxProjection);

    // 법선 벡터 변환 (VSLighting과 동일한 순서로 수정)
    output.normalW = mul(input.normal, (float3x3) gmtxWorld);
    
    // 텍스처 좌표는 그대로 전달
    output.texcoord = input.texcoord;

    return output;
}


// 픽셀 셰이더: PSLighting (퐁 조명 계산의 핵심)
// 텍스처와 샘플러를 위한 리소스 선언
Texture2D gAlbedoTexture : register(t0);
SamplerState gSamplerState : register(s0);

// --- [수정] 픽셀 셰이더 함수 ---
//float4 PSSkinning(PS_SKINNED_INPUT input) : SV_TARGET
//{
//    // 텍스처에서 이 픽셀의 기본 색상(Albedo)을 가져옵니다.
//    float4 objectColor = gAlbedoTexture.Sample(gSamplerState, input.texcoord);

//    // --- 조명 계산 시작 (input.color 대신 objectColor 사용) ---
//    float4 f4TotalColor = float4(0, 0, 0, 1);
//    float3 N = normalize(input.normalW);
    
//    for (int i = 0; i < MAX_LIGHTS; i++)
//    {
//        if (gLights.m_pLights[i].m_bEnable)
//        {
//            MATERIAL material = gMaterials.m_pReflections[0];

//            // Ambient 계산 (objectColor 사용)
//            float4 f4Ambient = gLights.m_pLights[i].m_xmf4Ambient * material.m_xmf4Ambient * objectColor;
//            f4TotalColor += f4Ambient;
            
//            // Diffuse 계산 (objectColor 사용)
//            float3 L = normalize(gLights.m_pLights[i].m_xmf3Direction);
//            float fDiffuseFactor = max(dot(N, -L), 0.f);
//            float4 f4Diffuse = fDiffuseFactor * gLights.m_pLights[i].m_xmf4Diffuse * objectColor;
//            f4TotalColor += f4Diffuse;
            
//            // Specular 계산 (기존과 동일)
//            float3 V = normalize(gf4Position.xyz - input.positionW);
//            V = reflect(V, input.normalW);
//            float Shigning = max(dot(-L, V), 0.f);
//            float SpecualNum = 64.f;
//            float4 f4Specular = gLights.m_pLights[i].m_xmf4Specular * material.m_xmf4Specular * pow(Shigning, SpecualNum);
//            f4TotalColor += f4Specular;
//        }
//    }
    
//    f4TotalColor += gLights.m_xmf4GlobalAmbient;
//    return saturate(f4TotalColor); // saturate로 최종 색상 범위를 0~1로 제한
//}

// 내장 그래픽 용 PSSkinning (조명 계산 제거 버전)

//float4 PSSkinning(PS_SKINNED_INPUT input) : SV_TARGET
//{
//    float4 objectColor = gAlbedoTexture.Sample(gSamplerState, input.texcoord);

//    if (length(input.normalW) < 0.0001f)
//    {
//        return objectColor * gLights.m_xmf4GlobalAmbient;
//    }
//    float4 f4TotalColor = gLights.m_xmf4GlobalAmbient;
//    float3 N = normalize(input.normalW);
//    // --- 뷰 벡터 계산 ---
//    float3 viewVector = gf4Position.xyz - input.positionW;
//    // 뷰 벡터의 길이가 너무 짧으면 Specular 계산을 생략
//    if (length(viewVector) < 0.0001f)
//    { // V가 0이면 Specular 계산이 불가능하므로, Ambient+Diffuse만 계산
//        for (int i = 0; i < MAX_LIGHTS; i++)
//        {
//            if (gLights.m_pLights[i].m_bEnable)
//            {
//                f4TotalColor += gLights.m_pLights[i].m_xmf4Ambient * gMaterials.m_pReflections[0].m_xmf4Ambient * objectColor;
//                float3 L = normalize(gLights.m_pLights[i].m_xmf3Direction);
//                float fDiffuseFactor = max(dot(N, -L), 0.f);
//                f4TotalColor += fDiffuseFactor * gLights.m_pLights[i].m_xmf4Diffuse * objectColor;
//            }
//        }
//        return saturate(f4TotalColor);
//    }
//    // 길이가 유효할 때만 정규화
//    float3 V = normalize(viewVector);
//    // --- 전체 조명 계산 ---
//    for (int i = 0; i < MAX_LIGHTS; i++)
//    {
//        if (gLights.m_pLights[i].m_bEnable)
//        {
//            MATERIAL material = gMaterials.m_pReflections[0];
//            float3 L = normalize(gLights.m_pLights[i].m_xmf3Direction); // Ambient
//            f4TotalColor += gLights.m_pLights[i].m_xmf4Ambient * material.m_xmf4Ambient * objectColor; // Diffuse
//            float fDiffuseFactor = max(dot(N, -L), 0.f);
//            f4TotalColor += fDiffuseFactor * gLights.m_pLights[i].m_xmf4Diffuse * objectColor;
        
//        // Specular
//            float3 H = normalize(-L + V);
//            float fSpecularFactor = pow(max(dot(N, H), 0.0f), 16.0f);
//            f4TotalColor += fSpecularFactor * gLights.m_pLights[i].m_xmf4Specular * material.m_xmf4Specular;
//        }
//    }
//    return saturate(f4TotalColor);
//}

float4 PSSkinning(PS_SKINNED_INPUT input) : SV_TARGET
{
    float4 objectColor = gAlbedoTexture.Sample(gSamplerState, input.texcoord);

    if (length(input.normalW) < 0.0001f)
    {
        return objectColor * gLights.m_xmf4GlobalAmbient;
    }
    float4 f4TotalColor = gLights.m_xmf4GlobalAmbient;
    float3 N = normalize(input.normalW);
    float3 V = gf4Position.xyz - input.positionW;
    
    // --- 전체 조명 계산 ---
    for (int i = 0; i < MAX_LIGHTS; i++)
    {
        if (gLights.m_pLights[i].m_bEnable)
        {
            MATERIAL material = gMaterials.m_pReflections[0];
            float3 L = normalize(gLights.m_pLights[i].m_xmf3Direction); // Ambient
            f4TotalColor += gLights.m_pLights[i].m_xmf4Ambient * material.m_xmf4Ambient * objectColor; // Diffuse
            float fDiffuseFactor = max(dot(N, -L), 0.f);
            f4TotalColor += fDiffuseFactor * gLights.m_pLights[i].m_xmf4Diffuse * objectColor;
        
        // Specular
            float3 H = max(normalize(-L + V), 0.f);
            float fSpecularFactor = pow(max(dot(N, H), 0.0f), 16.0f);
            f4TotalColor += fSpecularFactor * gLights.m_pLights[i].m_xmf4Specular * material.m_xmf4Specular;
        }
    }
    return saturate(f4TotalColor);
}