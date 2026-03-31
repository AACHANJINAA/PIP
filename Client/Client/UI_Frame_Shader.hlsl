 // UI_Shader.hlsl - 2D UI 렌더링을 위한 셰이더

     // 상수 버퍼: 화면 정보
cbuffer cbScreenInfo : register(b0)
{
	float g_ScreenWidth;
	float g_ScreenHeight;
	float2 g_Padding;
};

     // 상수 버퍼: UI 요소 정보
cbuffer cbUIFrameElement : register(b1)
{
	float2 g_ScreenPosition; // 화면상 위치 (픽셀 단위)
	float2 g_Size; // UI 크기 (픽셀 단위)
	float4 g_Color; // UI 색상 (tint)
	float2 g_UVOffset; // 텍스처 UV 오프셋
	float2 g_UVScale; // 텍스처 UV 스케일
	int g_UseTexture; // 텍스처 사용 여부 (1: 사용, 0: 단색)
	int g_otherplayerid; // 다른 플레이어 ID
	float2 g_padding; // 패딩
};

     // 텍스처 및 샘플러
Texture2D g_Texture : register(t0);
SamplerState g_Sampler : register(s0);

     // 입력 구조체
struct VS_INPUT
{
	float3 position : POSITION;
	float2 texcoord : TEXCOORD;
};

     // 출력 구조체
struct PS_INPUT
{
	float4 position : SV_POSITION;
	float2 texcoord : TEXCOORD;
};

     // 버텍스 셰이더
PS_INPUT VS_UI_FRAME(VS_INPUT input)
{
	PS_INPUT output;

         // 로컬 좌표 (0~1) -> 화면 픽셀 좌표
	float2 pixelPos = g_ScreenPosition + input.position.xy * g_Size;

         // 화면 픽셀 좌표 -> NDC 좌표 (-1 ~ 1)
	float2 ndc;
	ndc.x = (pixelPos.x / g_ScreenWidth) * 2.0f - 1.0f;
	ndc.y = 1.0f - (pixelPos.y / g_ScreenHeight) * 2.0f;

	output.position = float4(ndc, 0.0f, 1.0f);
	output.texcoord = input.texcoord * g_UVScale + g_UVOffset;

	return output;
}


static const float4 PlayerColors[4] =
{
	float4(0.2, 2.0, 2.0, 1.0), // 0: purple
     float4(2.0, 0.2, 0.2, 1.0), // 1: Red
     float4(0.2, 1.5, 0.2, 1.0), // 2: Green
     float4(0.2, 0.2, 2.0, 1.0) // 3: Blue
};
     // 픽셀 셰이더
float4 PS_UI_FRAME(PS_INPUT input) : SV_TARGET
{
	// ID에 맞는 색상 선택 (범위 제한 % 4)
	float4 idColor = PlayerColors[uint(g_otherplayerid) % 4];

	if (g_UseTexture > 0)
	{
		float4 texColor = g_Texture.Sample(g_Sampler, input.texcoord);
		return texColor * g_Color * idColor * 3.0f; // ID 색상 곱하기
	}
	return g_Color * idColor;
}