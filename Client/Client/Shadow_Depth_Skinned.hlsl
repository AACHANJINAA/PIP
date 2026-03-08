// b0: 월드 행렬 (RenderComponent cbGameObjectInfo와 동일)
cbuffer cbWorldMatrix : register(b0)
{
    matrix g_matWorld;
    matrix g_matWorldInverseTranspose;
};

   // b1: 3개 cascade의 LightViewProjection 행렬
cbuffer cbCascades : register(b1)
{
    matrix g_lightVP[3];
};

   // b4: 뼈대 변환 행렬 (skinned 루트시그의 b4)
cbuffer cbBoneTransforms : register(b4)
{
    matrix g_BoneTransforms[128];
};

   // VS 입력: GltfSkinnedVertex와 동일한 레이아웃
struct VS_SKINNED_SHADOW_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL; // shadow에서는 미사용 (스트라이드 맞춤용)
    float2 TexCoord : TEXCOORD; // shadow에서는 미사용
    float4 Tangent : TANGENT; // shadow에서는 미사용
    uint4 BoneIndices : BLENDINDICES;
    float4 BoneWeights : BLENDWEIGHT;
    uint InstanceID : SV_InstanceID;
};

struct VS_OUTPUT_SHADOW
{
    float4 Position : SV_POSITION;
    uint InstanceID : TEXCOORD0;
};

VS_OUTPUT_SHADOW VS_ShadowDepthSkinned(VS_SKINNED_SHADOW_INPUT input)
{
    VS_OUTPUT_SHADOW output;

       // 1. 스키닝 행렬 합산 (4개 뼈대 가중치 합)
    matrix skinTransform = (matrix) 0;
    skinTransform += input.BoneWeights.x * g_BoneTransforms[input.BoneIndices.x];
    skinTransform += input.BoneWeights.y * g_BoneTransforms[input.BoneIndices.y];
    skinTransform += input.BoneWeights.z * g_BoneTransforms[input.BoneIndices.z];
    skinTransform += input.BoneWeights.w * g_BoneTransforms[input.BoneIndices.w];

       // 2. Local → Skinned → World → LightVP
    float4 skinnedPos = mul(float4(input.Position, 1.0f), skinTransform);
    float4 worldPos = mul(skinnedPos, g_matWorld);
    output.Position = mul(worldPos, g_lightVP[input.InstanceID]);
    output.InstanceID = input.InstanceID;
    return output;
}

   // GS: Shadow_Depth.hlsl과 동일 (슬라이스 라우팅)
struct GS_OUTPUT_SHADOW
{
    float4 Position : SV_POSITION;
    uint RTIndex : SV_RenderTargetArrayIndex;
};

[maxvertexcount(3)]
   void GS_ShadowDepthSkinned(triangle VS_OUTPUT_SHADOW input[3],
                               inout TriangleStream<GS_OUTPUT_SHADOW> stream)
{
       [unroll]
    for (int i = 0; i < 3; i++)
    {
        GS_OUTPUT_SHADOW output;
        output.Position = input[i].Position;
        output.RTIndex = input[i].InstanceID;
        stream.Append(output);
    }
    stream.RestartStrip();
}