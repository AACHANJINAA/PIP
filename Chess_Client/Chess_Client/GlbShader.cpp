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
	UINT nInputElementDescs = 5; // 5개 (위치, 법선, 텍스쳐, 뼈 인덱스, 가중치)
	D3D12_INPUT_ELEMENT_DESC* d3dSkinnedInputLayout = new D3D12_INPUT_ELEMENT_DESC[nInputElementDescs];
	d3dSkinnedInputLayout[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	d3dSkinnedInputLayout[1] = { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	d3dSkinnedInputLayout[2] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	d3dSkinnedInputLayout[3] = { "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	d3dSkinnedInputLayout[4] = { "BLENDWEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
	

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
	// 1. 이 셰이더 클래스는 1개의 PSO만 가질 것이라고 설정합니다.
	m_nPipelineStates = 1;
	// 2. PSO 포인터를 저장할 메모리를 힙에 할당합니다.
	m_ppd3dPipelineStates = new ID3D12PipelineState * [m_nPipelineStates];
	m_ppd3dPipelineStates[0] = nullptr; // nullptr로 초기화

	// --- [핵심 로직 시작] ---

	// 3. 셰이더 바이트코드를 컴파일합니다.
	ComPtr<ID3DBlob> pd3dVertexShaderBlob, pd3dPixelShaderBlob;
	D3D12_SHADER_BYTECODE d3dVertexShaderByteCode = CreateVertexShader(&pd3dVertexShaderBlob);
	D3D12_SHADER_BYTECODE d3dPixelShaderByteCode = CreatePixelShader(&pd3dPixelShaderBlob);

	D3D12_GRAPHICS_PIPELINE_STATE_DESC d3dPipelineStateDesc;
	::ZeroMemory(&d3dPipelineStateDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
	d3dPipelineStateDesc.pRootSignature = pd3dGraphicsRootSignature;
	d3dPipelineStateDesc.VS = d3dVertexShaderByteCode;
	d3dPipelineStateDesc.PS = d3dPixelShaderByteCode;
	d3dPipelineStateDesc.RasterizerState = CreateRasterizerState();
	d3dPipelineStateDesc.BlendState = CreateBlendState();
	d3dPipelineStateDesc.DepthStencilState = CreateDepthStencilState();

	// 4. SkinnedVertex에 맞는 입력 레이아웃을 가져와서 설정합니다.
	d3dPipelineStateDesc.InputLayout = CreateInputLayout();

	d3dPipelineStateDesc.SampleMask = UINT_MAX;
	d3dPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	d3dPipelineStateDesc.NumRenderTargets = 1;
	d3dPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	d3dPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	d3dPipelineStateDesc.SampleDesc.Count = 1;
	d3dPipelineStateDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	// 5. 최종적으로 PSO를 생성하여 m_ppd3dPipelineStates[0]에 저장합니다.
	HRESULT hResult = pd3dDevice->CreateGraphicsPipelineState(&d3dPipelineStateDesc, IID_PPV_ARGS(&m_ppd3dPipelineStates[0]));

	if (FAILED(hResult))
	{
		// 에러 처리
		MessageBox(NULL, L"Failed to create Graphics Pipeline State", L"Error", MB_OK);
	}
	// Shader::CreateShader(pd3dDevice, pd3dGraphicsRootSignature); // 여기 있는 것 사용하면 안됨
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

void GlbShader::render(ID3D12GraphicsCommandList* pd3dCommandList, Camera* pCamera)
{
	Shader::render(pd3dCommandList, pCamera);
}