#include "stdafx.h"
#include "SkyboxShader.h"

const std::string& SkyboxShader::pso_name() const
{
	static const std::string name = "skybox";
	return name;
}

// skybox는 위치만 필요
D3D12_INPUT_LAYOUT_DESC SkyboxShader::create_input_layout()
{
	static const D3D12_INPUT_ELEMENT_DESC d3d_input_element_descs[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};
	return D3D12_INPUT_LAYOUT_DESC{ d3d_input_element_descs, _countof(d3d_input_element_descs) };
}

D3D12_SHADER_BYTECODE SkyboxShader::create_vertex_shader(ComPtr<ID3DBlob>& shader_blob)
{
	return compile_shader_from_file(L"Skybox_Shader.hlsl", "VS_Skybox", "vs_5_1", shader_blob);
}

D3D12_SHADER_BYTECODE SkyboxShader::create_pixel_shader(ComPtr<ID3DBlob>& shader_blob)
{
	return compile_shader_from_file(L"Skybox_Shader.hlsl", "PS_Skybox", "ps_5_1", shader_blob);
}

D3D12_RASTERIZER_DESC SkyboxShader::create_rasterizer_state()
{
	D3D12_RASTERIZER_DESC rasterizer_desc = Shader::create_rasterizer_state();
	// 스카이박스는 None으로 설정
	rasterizer_desc.CullMode = D3D12_CULL_MODE_NONE;
	return rasterizer_desc;
}

D3D12_DEPTH_STENCIL_DESC SkyboxShader::create_depth_stencil_state()
{
	D3D12_DEPTH_STENCIL_DESC depth_stencil_desc = Shader::create_depth_stencil_state();
	
	// 스카이박스는 깊이 테스트는 하되, 쓰기는 하지 않음
	depth_stencil_desc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
	// 이미 그려진 픽셀과 깊이가 같거나 더 가까울 때만 그림
	depth_stencil_desc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	return depth_stencil_desc;
}