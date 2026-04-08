 // Minimap_Shader.hlsl - 미니맵 렌더링을 위한 셰이더
 
 // 상수 버퍼: 미니맵 설정
cbuffer cbMinimap : register(b0)
{
    float2 g_MapWorldSize; // 맵 월드 크기 (X, Z)
    float2 g_MapWorldOrigin; // 맵 월드 원점 (X, Z)
    float2 g_PlayerWorldPos; // 플레이어 월드 위치 (X, Z)
    float g_MinHeight; // 최소 높이
    float g_MaxHeight; // 최대 높이
    float2 g_ScreenPosition; // 화면 픽셀 위치 (좌측 상단)
    float2 g_MinimapSize; // 미니맵 화면 크기 (픽셀)
    float g_ScreenWidth; // 화면 너비
    float g_ScreenHeight; // 화면 높이
    float g_Padding[2]; // 패딩
};
 
 // 텍스처 및 샘플러
Texture2D g_heightMap : register(t0);
SamplerState g_sampler : register(s0);
 
 // 입력 구조체
struct VS_INPUT
{
    float3 position : POSITION; // Quad 버텍스 (0~1 범위)
    float2 texcoord : TEXCOORD; // UV 좌표
};
 
 // 출력 구조체
struct PS_INPUT
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
};
 
 // 버텍스 셰이더
PS_INPUT VS_Minimap(VS_INPUT input)
{
    PS_INPUT output;
     
     // Quad 좌표 (0~1) -> 화면 픽셀 좌표
    float2 pixelPos = g_ScreenPosition + input.position.xy * g_MinimapSize;
     
     // 화면 픽셀 좌표 -> NDC 좌표 (-1 ~ 1)
    float2 ndc;
    ndc.x = (pixelPos.x / g_ScreenWidth) * 2.0f - 1.0f;
    ndc.y = 1.0f - (pixelPos.y / g_ScreenHeight) * 2.0f;
     
    output.position = float4(ndc, 0.0f, 1.0f);
    output.texcoord = input.texcoord;
     
    return output;
}
 
 // 픽셀 셰이더
float4 PS_Minimap(PS_INPUT input) : SV_TARGET
{
     // 1. Heightmap 샘플링
    float height_raw = g_heightMap.Sample(g_sampler, input.texcoord).r;

     // 2. 고도별 색상 매핑 (낮음: 진한 초록 → 높음: 밝은 갈색)
    float normalizedHeight = saturate(height_raw);
     
    float3 lowColor = float3(0.1f, 0.3f, 0.7f); // 푸른빛
    float3 midColor = float3(0.4f, 0.8f, 0.3f); // 형광 녹색
    float3 highColor = float3(0.8f, 0.2f, 0.2f); // 붉은색
     
    float3 terrainColor;
    if (normalizedHeight < 0.5f)
    {
        terrainColor = lerp(lowColor, midColor, normalizedHeight * 2.0f);
    }
    else
    {
        terrainColor = lerp(midColor, highColor, (normalizedHeight - 0.5f) * 2.0f);
    }
     
     // 3. 격자선 추가 (10% 간격)
    float2 gridUV = frac(input.texcoord * 10.0f);
    float gridLine = step(0.95f, max(gridUV.x, gridUV.y));
    terrainColor = lerp(terrainColor, float3(0.3f, 0.3f, 0.3f), gridLine * 0.3f);
     
     // 4. 플레이어 위치 마커 (빨간 점)
    float2 playerUV = float2(0.5f, 0.5f);
    if (g_MapWorldSize.x > 0.1f && g_MapWorldSize.y > 0.1f)
    {
        playerUV = (g_PlayerWorldPos - g_MapWorldOrigin) / g_MapWorldSize;
        playerUV.y = 1.0f - playerUV.y; // Y축 반전
    }
     
    float2 toPlayer = input.texcoord - playerUV;
    float distToPlayer = length(toPlayer * g_MinimapSize); // 픽셀 단위 거리
     
    if (distToPlayer < 8.0f) // 8픽셀 반경
    {
         // 빨간 원 + 검은 테두리
        if (distToPlayer < 6.0f)
        {
            terrainColor = float3(1.0f, 0.1f, 0.1f); // 빨간색
        }
        else
        {
            terrainColor = float3(0.0f, 0.0f, 0.0f); // 검은 테두리
        }
    }
     
     // 5. 테두리 추가 (미니맵 외곽선)
    float2 border = step(input.texcoord, float2(0.02f, 0.02f)) +
                     step(float2(0.98f, 0.98f), input.texcoord);
    float borderMask = saturate(border.x + border.y);
    terrainColor = lerp(terrainColor, float3(1.0f, 1.0f, 1.0f), borderMask);
     
    return float4(terrainColor, 1.f); // 약간 투명하게
}