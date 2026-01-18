#include "stdafx.h"
#include "MonsterHPUIShader.h"

ComPtr<ID3D12PipelineState> MonsterHPUIShader::create_pso(ID3D12Device* device, ID3D12RootSignature* root_signature)
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
	// GS를 위해 vs, ps 외에 gs_blob을 추가로 선언합니다.
	ComPtr<ID3DBlob> vs_blob, gs_blob, ps_blob;

	pso_desc.pRootSignature = root_signature;
	pso_desc.VS = create_vertex_shader(vs_blob);

	// 1. [추가] 기하 쉐이더(GS)를 생성하고 파이프라인에 등록합니다.
	pso_desc.GS = create_geometry_shader(gs_blob);

	pso_desc.PS = create_pixel_shader(ps_blob);
	pso_desc.InputLayout = create_input_layout();

	pso_desc.RasterizerState = create_rasterizer_state();

	// 2. [확인] HP_Bar.dds의 반투명도를 표현하기 위해 블렌딩이 활성화되어 있어야 합니다.
	pso_desc.BlendState = create_blend_state();

	// 3. [확인] UI가 캐릭터 몸 뚫고 나오지 않게 하되, 깊이 쓰기는 꺼두는 설정을 권장합니다.
	pso_desc.DepthStencilState = create_depth_stencil_state();

	pso_desc.SampleMask = UINT_MAX;

	// 4. [수정] 몬스터 위치 '점(Point)' 리스트를 입력으로 받으므로 타입을 POINT로 바꿉니다.
	// GS에서 이 점을 8개의 정점(삼각형 스트립)으로 확장하게 됩니다.
	pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;

	pso_desc.NumRenderTargets = 1;
	pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	pso_desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	pso_desc.SampleDesc.Count = 1;

	ComPtr<ID3D12PipelineState> pso;
	if (!SUCCEEDED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso))))
	{
		CERROR("Monster HP UI PSO 생성 실패");
	}

	return pso;
}

const std::string& MonsterHPUIShader::pso_name() const
{
	static const std::string name = "Monster_HP_UI";
	return name;
}

std::string MonsterHPUIShader::required_root_signature() const
{
	return "Monster_HP_UI";
}

D3D12_INPUT_LAYOUT_DESC MonsterHPUIShader::create_input_layout()
{
	static const D3D12_INPUT_ELEMENT_DESC d3d_input_element_descs[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	return D3D12_INPUT_LAYOUT_DESC{ d3d_input_element_descs, _countof(d3d_input_element_descs) };
}

D3D12_SHADER_BYTECODE MonsterHPUIShader::create_vertex_shader(ComPtr<ID3DBlob>& shader_blob)
{
	return compile_shader_from_file(L"Monster_HP_Shader.hlsl", "VS", "vs_5_1", shader_blob);
}

D3D12_SHADER_BYTECODE MonsterHPUIShader::create_pixel_shader(ComPtr<ID3DBlob>& shader_blob)
{
	return compile_shader_from_file(L"Monster_HP_Shader.hlsl", "PS", "ps_5_1", shader_blob);
}

D3D12_SHADER_BYTECODE MonsterHPUIShader::create_geometry_shader(ComPtr<ID3DBlob>& shader_blob)
{
	return compile_shader_from_file(L"Monster_HP_Shader.hlsl", "GS", "gs_5_1", shader_blob);
}

D3D12_BLEND_DESC MonsterHPUIShader::create_blend_state()
{
	D3D12_BLEND_DESC blend_desc = {};
	blend_desc.AlphaToCoverageEnable = FALSE;
	blend_desc.IndependentBlendEnable = FALSE; // 모든 렌더 타겟에 동일 설정 적용

	// 0번 렌더 타겟(기본 화면)에 대한 블렌드 설정
	D3D12_RENDER_TARGET_BLEND_DESC& rt_blend_desc = blend_desc.RenderTarget[0];

	rt_blend_desc.BlendEnable = TRUE;             // 블렌딩 활성화
	rt_blend_desc.LogicOpEnable = FALSE;

	// 컬러 블렌딩 공식: (Source * SrcAlpha) + (Dest * (1 - SrcAlpha))
	rt_blend_desc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	rt_blend_desc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	rt_blend_desc.BlendOp = D3D12_BLEND_OP_ADD;

	// 알파 채널 블렌딩 공식 (보통 1과 0을 사용하여 결과 알파를 결정)
	rt_blend_desc.SrcBlendAlpha = D3D12_BLEND_ONE;
	rt_blend_desc.DestBlendAlpha = D3D12_BLEND_ZERO;
	rt_blend_desc.BlendOpAlpha = D3D12_BLEND_OP_ADD;

	rt_blend_desc.LogicOp = D3D12_LOGIC_OP_NOOP;
	rt_blend_desc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	return blend_desc;
}

D3D12_DEPTH_STENCIL_DESC MonsterHPUIShader::create_depth_stencil_state()
{
	D3D12_DEPTH_STENCIL_DESC ds_desc = {};

	// 1. 깊이 테스트 비활성화 (Z-Fighting 원천 차단)
	// 이렇게 하면 몬스터 몸체 뒤에 있어도 HP 바가 투명하게 보입니다.
	ds_desc.DepthEnable = FALSE;
	ds_desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	ds_desc.DepthFunc = D3D12_COMPARISON_FUNC_ALWAYS;

	ds_desc.StencilEnable = FALSE;

	return ds_desc;
}