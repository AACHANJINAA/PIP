#include "stdafx.h"
#include "Shader.h"

ComPtr<ID3D12PipelineState> Shader::create_pso(ID3D12Device* device, ID3D12RootSignature* root_signature)
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
	ComPtr<ID3DBlob> vs_blob, ps_blob;

	pso_desc.pRootSignature = root_signature;
	pso_desc.VS = create_vertex_shader(vs_blob);
	pso_desc.PS = create_pixel_shader(ps_blob);
	pso_desc.InputLayout = create_input_layout();

	pso_desc.RasterizerState = create_rasterizer_state();
	pso_desc.BlendState = create_blend_state();
	pso_desc.DepthStencilState = create_depth_stencil_state();

	// DW추가 : MSAA를 사용하지 않더라도 픽셀당 사용하는 샘플의 개수가 1개는 무조건 있어야 한다. -> 지금은 샘플 전부 사용하겠다는 의미인 UINT_MAX 사용
	pso_desc.SampleMask = UINT_MAX;

	// [수정] 파생 클래스가 토폴로지를 지정할 수 있도록 가상 함수로 분리하는 것이 좋습니다.
	// virtual D3D12_PRIMITIVE_TOPOLOGY_TYPE primitive_topology_type() const { return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; }
	pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	pso_desc.NumRenderTargets = 1;
	pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	pso_desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	pso_desc.SampleDesc.Count = 1;

	ComPtr<ID3D12PipelineState> pso;
	if(!SUCCEEDED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso))))
	{
		CERROR("PSO 생성 실패");
	}

	return pso;
}

D3D12_RASTERIZER_DESC Shader::create_rasterizer_state()
{
	D3D12_RASTERIZER_DESC d3dRasterizerDesc;
	::ZeroMemory(&d3dRasterizerDesc, sizeof(D3D12_RASTERIZER_DESC));
	d3dRasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID; 
	d3dRasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	d3dRasterizerDesc.FrontCounterClockwise = FALSE;
	d3dRasterizerDesc.DepthBias = 0;
	d3dRasterizerDesc.DepthBiasClamp = 0.0f;
	d3dRasterizerDesc.SlopeScaledDepthBias = 0.0f;
	d3dRasterizerDesc.DepthClipEnable = TRUE;
	d3dRasterizerDesc.MultisampleEnable = FALSE;
	d3dRasterizerDesc.AntialiasedLineEnable = FALSE;
	d3dRasterizerDesc.ForcedSampleCount = 0;
	d3dRasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	return(d3dRasterizerDesc);
}

D3D12_DEPTH_STENCIL_DESC Shader::create_depth_stencil_state()
{
	D3D12_DEPTH_STENCIL_DESC d3dDepthStencilDesc;
	::ZeroMemory(&d3dDepthStencilDesc, sizeof(D3D12_DEPTH_STENCIL_DESC));
	d3dDepthStencilDesc.DepthEnable = TRUE;
	d3dDepthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	d3dDepthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	d3dDepthStencilDesc.StencilEnable = FALSE;
	d3dDepthStencilDesc.StencilReadMask = 0x00;
	d3dDepthStencilDesc.StencilWriteMask = 0x00;
	d3dDepthStencilDesc.FrontFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	d3dDepthStencilDesc.FrontFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	d3dDepthStencilDesc.FrontFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	d3dDepthStencilDesc.FrontFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	d3dDepthStencilDesc.BackFace.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	d3dDepthStencilDesc.BackFace.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	d3dDepthStencilDesc.BackFace.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	d3dDepthStencilDesc.BackFace.StencilFunc = D3D12_COMPARISON_FUNC_NEVER;
	return(d3dDepthStencilDesc);
}

D3D12_BLEND_DESC Shader::create_blend_state()
{
	D3D12_BLEND_DESC d3dBlendDesc;
	::ZeroMemory(&d3dBlendDesc, sizeof(D3D12_BLEND_DESC));
	d3dBlendDesc.AlphaToCoverageEnable = FALSE;
	d3dBlendDesc.IndependentBlendEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].BlendEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].LogicOpEnable = FALSE;
	d3dBlendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	d3dBlendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	d3dBlendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	d3dBlendDesc.RenderTarget[0].LogicOp = D3D12_LOGIC_OP_NOOP;
	d3dBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	return(d3dBlendDesc);
}

D3D12_SHADER_BYTECODE Shader::compile_shader_from_file(const std::wstring& file_name, LPCSTR shader_name,
	LPCSTR shader_profile, ComPtr<ID3DBlob>& shader_blob)
{
	UINT nCompileFlags = 0;
#if defined(_DEBUG)
	nCompileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
	ComPtr<ID3DBlob> pd3dErrorBlob;

	HRESULT hResult = ::D3DCompileFromFile(file_name.c_str(), NULL,
		D3D_COMPILE_STANDARD_FILE_INCLUDE, shader_name, shader_profile, 
		nCompileFlags, 0, &shader_blob, &pd3dErrorBlob);

	if (FAILED(hResult))
	{
		if (pd3dErrorBlob)
		{
			// 에러 메시지를 디버그 출력창에 표시하고 멈춘다.
			CERROR((char*)pd3dErrorBlob->GetBufferPointer())
		}
		return { 0, NULL }; // 실패했으므로 빈 셰이더 바이트코드를 반환
	}

	D3D12_SHADER_BYTECODE d3dShaderByteCode;
	d3dShaderByteCode.BytecodeLength = shader_blob->GetBufferSize();
	d3dShaderByteCode.pShaderBytecode = shader_blob->GetBufferPointer();
	return(d3dShaderByteCode);
}
