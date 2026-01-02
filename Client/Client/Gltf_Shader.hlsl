 #define MAX_LIGHTS 16

 #define POINT_LIGHT 1
 #define SPOT_LIGHT 2
 #define DIRECTIONAL_LIGHT 3

cbuffer cbMaterial : register(b2)
{
    float4 BaseColorFactor;
    float3 EmissiveFactor;
    float MetallicFactor;
    float RoughnessFactor;
    float NormalTextureScale;
    float AlphaCutoff;
    int AlphaMode; // 0 = OPAQUE, 1 = MASK, 2 = BLEND
    int DoubleSided; // 0 = false, 1 = true
    int HasBaseColorTexture;
    int HasMetallicRoughnessTexture;
    int HasNormalTexture;
    int HasEmissiveTexture;
    float2 Padding; // 16바이트 정렬을 위한 패딩
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
    float4 gvCameraPosition;
};

 // Light.hlsl에서 사용하는 구조체와 동일하게 맞춤
struct MATERIAL
{
    float4 m_cAmbient;
    float4 m_cDiffuse;
    float4 m_cSpecular; // a = power
    float4 m_cEmissive;
};

 // 임시로 머티리얼 정의 (PBR에서는 사용 안하지만 Light.hlsl 함수에 필요)
static MATERIAL gMaterial =
{
    float4(0.2, 0.2, 0.2, 1.0), // Ambient
	 float4(0.8, 0.8, 0.8, 1.0), // Diffuse
	 float4(1.0, 1.0, 1.0, 32.0), // Specular (a = shininess)
	 float4(0.0, 0.0, 0.0, 1.0) // Emissive
};

 #include "Light.hlsl"

struct VS_INPUT
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 TexCoord0 : TEXCOORD; // 텍스쳐 좌표 (추가된 부분)
    float4 Tangent : TANGENT;
};

struct VS_OUTPUT
{
    float4 Position : SV_POSITION;
    float3 WorldPosition : POSITION0;
    float2 TexCoord : TEXCOORD0;
    float3 Normal : NORMAL0;
    float3 Tangent : TANGENT0;
    float3 Bitangent : BITANGENT0;
};

VS_OUTPUT VS_GLTF(VS_INPUT input)
{
    VS_OUTPUT Out;

    Out.WorldPosition = mul(float4(input.Position, 1.0f), g_matWorld).xyz;
    Out.Position = mul(float4(Out.WorldPosition, 1.0f), g_matView);
    Out.Position = mul(Out.Position, g_matProjection);
    
    Out.TexCoord = input.TexCoord0;

    float3x3 worldRot = (float3x3) g_matWorld;
    
    // 각 축을 정규화해서 스케일을 제거 (Uniform Scale 가정)
    worldRot[0] = normalize(worldRot[0]);
    worldRot[1] = normalize(worldRot[1]);
    worldRot[2] = normalize(worldRot[2]);

    // [중요] 벡터 * 행렬 (Vector * Matrix) 순서 유지!
    Out.Normal = normalize(mul(input.Normal, worldRot));
    Out.Tangent = normalize(mul(input.Tangent.xyz, worldRot));
    float tangentW = input.Tangent.w;
    // Bitangent 재계산 (Tangent.w 사용)
    Out.Bitangent = normalize(cross(Out.Tangent, Out.Normal));

    return Out;
}   

float4 PS_GLTF(VS_OUTPUT In) : SV_TARGET
{
    // 비추는 방향 밝은 초록이면 아래 위 방향
    //float3 L = normalize(-gLights[0].m_vDirection); // Light.hlsl 94줄과 동일
    //return float4(L * 0.5 + 0.5, 1.0f);
    
    // tangent 확인 -> bitangent도 똑같은 형식으로 확인 가능
    //float3 T = normalize(In.Tangent);
    //return float4(T * 0.5 + 0.5, 1.0f);
    
    // 1. Albedo (BaseColor) 계산
    // 공식: FinalBaseColor = TextureSample * BaseColorFactor
    float4 diffuseSample = g_txDiffuse.Sample(g_samLinear, In.TexCoord);
    
    float3 albedo = diffuseSample.rgb;
    
    // 만약 텍스처가 로드되지 않아 검은색(0,0,0)이라면 흰색으로 보정 (Factor가 색을 결정하도록)
    if (length(albedo) < 0.01f)
    {
        albedo = float3(1.0, 1.0, 1.0);
    }
    
    // [핵심] C++에서 넘어온 BaseColorFactor 곱하기 (색상 틴트 적용)
    albedo *= BaseColorFactor.rgb;

    
    // 2. ORM (Occlusion, Roughness, Metallic) 계산
    // 공식: Value = TextureSample * Factor
    float3 ormSample = g_txORM.Sample(g_samLinear, In.TexCoord).rgb;
    
    //[핵심] 기본값을 C++에서 넘어온 Factor로 설정! (쓰레기 값이 아니므로 믿고 씀)
    float ao = 1.0f; // AO는 보통 별도 Factor가 없으므로 1.0 시작
    float roughness = RoughnessFactor; // C++에서 설정한 거칠기 값
    float metallic = MetallicFactor; // C++에서 설정한 금속성 값

    // ORM 맵이 유효하다면(검은색이 아니라면), 텍스처 값을 Factor에 곱해줌 (PBR 표준)
    // 예: 텍스처가 없으면 Factor(1.0) 그대로 사용, 텍스처가 있으면 Texture * Factor
    if (dot(ormSample, float3(1, 1, 1)) > 0.05f)
    {
        ao = ormSample.r;
        roughness *= ormSample.g; // 텍스처(G) * 계수
        metallic *= ormSample.b; // 텍스처(B) * 계수
    }
    
    // 3. Normal Map
    float3 N = normalize(In.Normal);
    
    float3 normalMapSample = g_txNormal.Sample(g_samLinear, In.TexCoord).rgb;

    if (length(normalMapSample) > 0.1f)
    {
        float3 N_map = normalMapSample * 2.0 - 1.0;
        N_map.y = -N_map.y; // 언리얼 Exporter의 OpenGL 변환 되돌리기
        float3 T = normalize(In.Tangent);
        float3 B = normalize(In.Bitangent);
        N = normalize(T * N_map.x + B * N_map.y + N * N_map.z);
    }

    // tangent 확인용
    //return float4(N * 0.5 + 0.5, 1.0f);

    
    // 4. Emissive (발광)
    // 공식: FinalEmissive = TextureSample * EmissiveFactor
    float3 emissiveSample = g_txEmissive.Sample(g_samLinear, In.TexCoord).rgb;
    
    // 텍스처가 없으면 흰색(1,1,1)으로 가정, 있으면 텍스처 사용
    float3 emissiveColor = (length(emissiveSample) > 0.01f) ? emissiveSample : float3(1.0, 1.0, 1.0);
    
    // [핵심] C++에서 넘어온 EmissiveFactor 곱하기
    float3 finalEmissive = emissiveColor * EmissiveFactor;

    
    // 5. 조명 계산 및 후처리
    float3 V = normalize(gvCameraPosition.xyz - In.WorldPosition);
    float4 litColor = Lighting(In.WorldPosition, N, V, albedo, metallic, roughness, ao);

    float3 finalColor = litColor.rgb + finalEmissive;

    // Tone Mapping (HDR -> LDR)
    finalColor = finalColor / (finalColor + 1.0f);
    
    // Gamma Correction
    finalColor = pow(finalColor, 1.0f / 2.2f);
    
    return float4(finalColor, diffuseSample.a);
}


//////////////////////// HP 효과 픽셀 셰이더 추가 ////////////////////

cbuffer cbHp : register(b8, space1)
{
    int g_nHp;
};

float4 PS_HP_GLTF(VS_OUTPUT In) : SV_TARGET
{
    float4 albedoMap = g_txDiffuse.Sample(g_samLinear, In.TexCoord);
    float3 normalMap = g_txNormal.Sample(g_samLinear, In.TexCoord).rgb;
    float3 ormMap = g_txORM.Sample(g_samLinear, In.TexCoord).rgb;
    float3 emissiveMap = g_txEmissive.Sample(g_samLinear, In.TexCoord).rgb;

    float3 N = In.Normal;
    if (dot(normalMap, normalMap) > 0.01)
    {
        float3 N_tangent = normalMap * 2.0 - 1.0;
        N_tangent.y = -N_tangent.y; // 언리얼 Exporter의 OpenGL 변환 되돌리기
        float3x3 TBN = float3x3(normalize(In.Tangent), normalize(In.Bitangent), normalize(In.Normal));
        N = normalize(mul(N_tangent, TBN));
    }

    float3 albedo = albedoMap.rgb;
    float ao = (dot(ormMap, ormMap) > 0.001) ? ormMap.r : 1.0f;
    float roughness = (dot(ormMap, ormMap) > 0.001) ? ormMap.g : 0.8f;
    float metallic = (dot(ormMap, ormMap) > 0.001) ? ormMap.b : 0.1f;

    // PBR 라이팅 계산
    float3 V = normalize(gvCameraPosition.xyz - In.WorldPosition);
    float4 litColor = Lighting(In.WorldPosition, N, V, albedo, metallic, roughness, ao);

    // Emissive 추가
    float3 color = litColor.rgb + emissiveMap;

    // HDR to LDR, 감마 보정
    color = color / (color + float3(1.0, 1.0, 1.0));
    color = pow(color, float3(1.0 / 2.2, 1.0 / 2.2, 1.0 / 2.2));

	// HP 효과
    float hp_r = (100 - g_nHp) / 100.0f;
    if (color.r < hp_r)
    {
        color.r = hp_r;
    }

    return float4(color, albedoMap.a);
}