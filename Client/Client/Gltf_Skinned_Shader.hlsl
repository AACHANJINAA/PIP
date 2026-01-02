
// 기존 Gltf_Shader의 구조체와 Pixel Shader를 그대로 가져옴
#include "Gltf_Shader.hlsl"

// --------------------------------------------------------
// 스키닝 전용 상수 버퍼 및 구조체 정의
// --------------------------------------------------------

// 뼈대 변환 행렬 배열 (Root Parameter 4번, register b4)
// SkinnedRootSignatureGenerator에서 b4로 정의됨
cbuffer cbBoneTransforms : register(b4)
{
    matrix g_BoneTransforms[128]; // 최대 128개 뼈대 지원
};

// 스키닝용 Input 구조체 (기존 VS_INPUT에 뼈대 정보 추가)
struct VS_SKINNED_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord0 : TEXCOORD;
    float4 Tangent : TANGENT;
    
    // [추가] 스키닝 데이터 (GltfSkinnedVertex와 매칭)
    uint4 BoneIndices : BLENDINDICES; // 뼈대 인덱스
    float4 BoneWeights : BLENDWEIGHT; // 가중치
};

// --------------------------------------------------------
// 스키닝 Vertex Shader
// --------------------------------------------------------
VS_OUTPUT VS_GLTF_SKINNED(VS_SKINNED_INPUT input)
{
    VS_OUTPUT Out;

    // 1. 스키닝 행렬 계산 (4개의 뼈대 영향 합산)
    // 정점의 로컬 위치를 뼈대의 움직임에 맞춰 변환해주는 행렬을 만들어줌
    matrix skinTransform = 0;
    skinTransform += mul(input.BoneWeights.x, g_BoneTransforms[input.BoneIndices.x]);
    skinTransform += mul(input.BoneWeights.y, g_BoneTransforms[input.BoneIndices.y]);
    skinTransform += mul(input.BoneWeights.z, g_BoneTransforms[input.BoneIndices.z]);
    skinTransform += mul(input.BoneWeights.w, g_BoneTransforms[input.BoneIndices.w]);
    
    // 2. 정점 위치 변환 
    // 순서: Local(Bind Pose) -> Skinning Transform -> World Matrix
    // 주의: mul(vector, matrix) 순서
    float4 skinnedPos = mul(float4(input.Position, 1.0f), skinTransform);
    
    // 월드 변환 (배치 위치)
    Out.WorldPosition = mul(skinnedPos, g_matWorld).xyz;

    // View -> Projection 변환
    Out.Position = mul(float4(Out.WorldPosition, 1.0f), g_matView);
    Out.Position = mul(Out.Position, g_matProjection);
    
    // 3. 텍스처 좌표 전달
    Out.TexCoord = input.TexCoord0;

    // 4. Normal, Tangent 변환
    // 법선 벡터도 뼈대의 회전에 따라 같이 회전해야 함
    // (정확한 계산을 위해선 Inverse Transpose가 필요하지만, 균등 스케일 가정 시 3x3 사용)
    float3x3 skinNormalTransform = (float3x3) skinTransform;
    
    float3 skinnedNormal = mul(input.Normal, skinNormalTransform);
    float3 skinnedTangent = mul(input.Tangent.xyz, skinNormalTransform);

    // 월드 회전 적용
    Out.Normal = normalize(mul((float3x3) g_matWorld, skinnedNormal));
    Out.Tangent = normalize(mul((float3x3) g_matWorld, skinnedTangent));
    
    // Bitangent 계산 (Normal과 Tangent 외적)
    Out.Bitangent = normalize(cross(Out.Tangent, Out.Normal) * input.Tangent.w);

    return Out;
}