//게임 객체의 정보를 위한 상수 버퍼를 선언한다. 

cbuffer cbGameObjectInfo : register(b0)
{
    matrix gmtxWorld : packoffset(c0);
};

//카메라의 정보를 위한 상수 버퍼를 선언한다. 
cbuffer cbCameraInfo : register(b1)
{
    matrix gmtxView : packoffset(c0);
    matrix gmtxProjection : packoffset(c4);
};

cbuffer cbPongStruct : register(b2)
{
    // 패킹을 위해서 float4로 통일, 이는 CPU에서 데이터를 보낼때도 float4로 통일하면 된다.
    
    float4 BaseLightColor : packoffset(c0); // 기본 빛(환경광)의 색상이 무슨 색인지? ex) 대기에서 반사되어 오는 빛
    float4 DirectLighrColor : packoffset(c1); // 형광등 같이 직접 관여하는 빛의 색은 무슨 색인지? ex) 태양, 형광등 등등
    float4 LightPos : packoffset(c2); // 직접관여하는 빛의 위치
};

//정점 셰이더의 입력을 위한 구조체를 선언한다. 
struct VS_INPUT
{
    float3 position : POSITION;
    float4 color : COLOR;
    float3 normal : NORMAL;
};

//정점 셰이더의 출력(픽셀 셰이더의 입력)을 위한 구조체를 선언한다. 
struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
    float3 normal : NORMAL;
};

//정점 셰이더를 정의한다. 
VS_OUTPUT VSDiffused(VS_INPUT input)
{
    VS_OUTPUT output;
    //정점을 변환(월드 변환, 카메라 변환, 투영 변환)한다. 
    output.position = mul(mul(mul(float4(input.position, 1.0f), gmtxWorld), gmtxView),gmtxProjection);
    output.color = input.color;
    
    // 퐁 쉐이딩을 위한 것 노멀값을 월드 공간으로 이동
    output.normal = normalize(mul(input.normal, (float3x3) gmtxWorld));
    
    return(output);
}

//픽셀 셰이더를 정의한다.
float4 PSDiffused(VS_OUTPUT input) : SV_TARGET
{
    
    // N(노멀벡터), L(픽셀에서 광원으로의 방향벡터), V(픽셀에서 카메라로의 방향벡터) 계산
    
    float3 viewPosition = gmtxView._41_42_43; // 카메라의 위치 뽑아내기
    
    float3 N = normalize(input.normal);
    float3 L = normalize(LightPos.xyz - input.position.xyz);
    float3 V = normalize(viewPosition - input.position.xyz);

    
    // 엠비언트(환경광) 계산
    // 하얀색 빛으로 적용
    float ambientStrong = 0.1f; // 엠비언트 항의 세기
    float4 ambient = { (float3(ambientStrong, ambientStrong, ambientStrong) * BaseLightColor.rgb), 0.f };
    
    // 디퓨즈(난반사) 계산
    float diffuseStrong = max(0.f, dot(N, L)); // 난반사 항의 세기 -> 내적으로 계산 음수는 필요없어서 최소값 0으로 설정
    float3 diffuse = BaseLightColor.rgb * input.color.rgb * diffuseStrong;
    
    // 스펙큘러(정반사) 계산
    float3 Reflect = reflect(-L, N);
    float shininess = 2.f; // 정반사계수 -> 이게 클 수록 하이라이트의 범위는 좁아지고 더 밝아짐
    float specularStrong = pow(max(0.0, dot(Reflect, V)), shininess); // 스펙큘러항 세기
    
    // 비금속인 경우
    float3 specular = BaseLightColor.rgb * float3(1.f, 1.f, 1.f) * specularStrong; // 최종 정반사 값
    
    // 금속인 경우
    // float3 specular = BaseLightColor.rgb * input.color.rgb * specularStrong; // 최종 정반사 값
    
    
    // 기본적인 퐁 쉐이딩 최종 색상 = 엠비언트(환경광) + 디퓨즈(난반사) + 스펙큘러(정반사)
    input.color = ambient.rgb + diffuse.rgb + specular.rgb;

    
    // ★중요★
    // ★최종값 까지 입력받은 a(알파)값은 건들지 않음★
    
    return(input.color);
}