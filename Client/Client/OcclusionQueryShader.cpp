#include "stdafx.h"
#include "OcclusionQueryShader.h"

D3D12_INPUT_LAYOUT_DESC OcclusionQueryShader::create_input_layout() {
	static std::vector<D3D12_INPUT_ELEMENT_DESC> inputElementDescs = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
	return { inputElementDescs.data(), (UINT)inputElementDescs.size() };
}

D3D12_SHADER_BYTECODE OcclusionQueryShader::create_vertex_shader(ComPtr<ID3DBlob>& shader_blob) {
	return compile_shader_from_file(L"OcclusionQuery.hlsl", "VS_Main", "vs_5_1", shader_blob);
}

D3D12_SHADER_BYTECODE OcclusionQueryShader::create_pixel_shader(ComPtr<ID3DBlob>& shader_blob) {
	return compile_shader_from_file(L"OcclusionQuery.hlsl", "PS_Main", "ps_5_1", shader_blob);
}

ComPtr<ID3D12PipelineState> OcclusionQueryShader::create_pso(ID3D12Device* device, ID3D12RootSignature* root_sig) {
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = create_input_layout();
	psoDesc.pRootSignature = root_sig;

	ComPtr<ID3DBlob> vsBlob, psBlob;
	psoDesc.VS = create_vertex_shader(vsBlob);
	psoDesc.PS = create_pixel_shader(psBlob);

	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);

	// [수정] 기본 Rasterizer 대신 Wireframe으로 설정 (박스 내부를 보기 위함)
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	// [수정] 0으로 되어있던 마스크를 ALL로 변경 (색상 출력 활성화)
	psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = 0;// D3D12_COLOR_WRITE_ENABLE_ALL;

	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	psoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	psoDesc.SampleDesc.Count = 1;

	ComPtr<ID3D12PipelineState> pso;
	device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso));
	return pso;
}