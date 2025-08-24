#include "stdafx.h"
#include "GlbShader.h"

GlbShader::GlbShader()
{

}
GlbShader::~GlbShader()
{

}

D3D12_INPUT_LAYOUT_DESC GlbShader::CreateInputLayout()
{
	D3D12_INPUT_ELEMENT_DESC d3dSkinnedInputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "BLENDWEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
	UINT nInputElementDescs = 5; // 5개 (위치, 법선, 텍스쳐, 뼈 인덱스, 가중치)

	D3D12_INPUT_LAYOUT_DESC d3dInputLayoutDesc;
	d3dInputLayoutDesc.pInputElementDescs = d3dSkinnedInputLayout;
	d3dInputLayoutDesc.NumElements = nInputElementDescs;

	return(d3dInputLayoutDesc);
}

D3D12_SHADER_BYTECODE GlbShader::CreateVertexShader(ID3DBlob** ppd3dShaderBlob)
{
	return(Shader::CompileShaderFromFile(L"GLB_Shader.hlsl", "VSSkinning", "vs_5_1", ppd3dShaderBlob));
}
D3D12_SHADER_BYTECODE GlbShader::CreatePixelShader(ID3DBlob** ppd3dShaderBlob)
{
	return(Shader::CompileShaderFromFile(L"GLB_Shader.hlsl", "PSSkinning", "ps_5_1", ppd3dShaderBlob));
}

void GlbShader::CreateShader(ID3D12Device* pd3dDevice, ID3D12RootSignature* pd3dGraphicsRootSignature)
{
	m_nPipelineStates = 1;
	m_ppd3dPipelineStates = new ID3D12PipelineState * [m_nPipelineStates];
	Shader::CreateShader(pd3dDevice, pd3dGraphicsRootSignature);
}

void GlbShader::BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList
	* pd3dCommandList)
{
	CreateShaderVariables(pd3dDevice, pd3dCommandList);
}

void GlbShader::ReleaseObjects()
{

}

void GlbShader::AnimateObjects(float fTimeElapsed)
{

}

void GlbShader::ReleaseUploadBuffers()
{

}

void GlbShader::Render(ID3D12GraphicsCommandList* pd3dCommandList, Camera* pCamera)
{
	Shader::Render(pd3dCommandList, pCamera);
}