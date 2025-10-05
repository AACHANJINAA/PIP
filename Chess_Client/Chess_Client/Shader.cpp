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

	// [수정] 파생 클래스가 토폴로지를 지정할 수 있도록 가상 함수로 분리하는 것이 좋습니다.
	// virtual D3D12_PRIMITIVE_TOPOLOGY_TYPE primitive_topology_type() const { return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; }
	pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	pso_desc.NumRenderTargets = 1;
	pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	pso_desc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	pso_desc.SampleDesc.Count = 1;

	ID3D12PipelineState* pso = nullptr;
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
	d3dRasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID; // DW수정 : 원래 이걸로 해야한다. 채우기로
	//D3D12_FILL_MODE_WIREFRAME은 프리미티브(삼각형)의 내부를 칠하지 않고 변(Edge)만 그린다. 
	//d3dRasterizerDesc.FillMode = D3D12_FILL_MODE_WIREFRAME; // DW수정 : 따라하기 10에서 잠시 설정함
	d3dRasterizerDesc.CullMode = D3D12_CULL_MODE_NONE;
	//d3dRasterizerDesc.CullMode = D3D12_CULL_MODE_NONE; // DW수정 : 따라하기 9에서 이걸로 잠시 설정함
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
	//d3dDepthStencilDesc.DepthEnable = FALSE; // DW수정 : 따라하기 11번
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


//입력 조립기에게 정점 버퍼의 구조를 알려주기 위한 구조체를 반환한다. (수정) 위치, 법선, 색상 받음[PONG]
//D3D12_INPUT_LAYOUT_DESC Shader::CreateInputLayout()
//{
//	UINT nInputElementDescs = 4; // 4개로 변경 (위치, 법선, 텍스쳐, 색상)
//	D3D12_INPUT_ELEMENT_DESC* pd3dInputElementDescs = new D3D12_INPUT_ELEMENT_DESC[nInputElementDescs];
//
//	pd3dInputElementDescs[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
//	pd3dInputElementDescs[1] = { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }; // NORMAL 추가
//	pd3dInputElementDescs[2] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }; // 오프셋 24로 변경
//	pd3dInputElementDescs[3] = { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }; 
//
//	D3D12_INPUT_LAYOUT_DESC d3dInputLayoutDesc;
//	d3dInputLayoutDesc.pInputElementDescs = pd3dInputElementDescs;
//	d3dInputLayoutDesc.NumElements = nInputElementDescs;
//
//	return(d3dInputLayoutDesc);
//}
//그래픽스 파이프라인 상태 객체를 생성한다. 






//CPlayerShader::CPlayerShader()
//{
//
//}
//CPlayerShader::~CPlayerShader()
//{
//
//}
//
//D3D12_INPUT_LAYOUT_DESC CPlayerShader::CreateInputLayout()
//{
//	UINT nInputElementDescs = 2;
//	D3D12_INPUT_ELEMENT_DESC* pd3dInputElementDescs = new D3D12_INPUT_ELEMENT_DESC[nInputElementDescs];
//	pd3dInputElementDescs[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
//	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
//	pd3dInputElementDescs[1] = { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12,
//	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
//	D3D12_INPUT_LAYOUT_DESC d3dInputLayoutDesc;
//	d3dInputLayoutDesc.pInputElementDescs = pd3dInputElementDescs;
//	d3dInputLayoutDesc.NumElements = nInputElementDescs;
//	return(d3dInputLayoutDesc);
//}
//
//// (수정) 정점이랑 픽셀 받는거는 Diffused말고 Lighting으로 변경 [PONG]
//D3D12_SHADER_BYTECODE CPlayerShader::CreateVertexShader(ID3DBlob** ppd3dShaderBlob)
//{
//	return(Shader::CompileShaderFromFile(L"Shaders.hlsl"/*KJ 수정*/, "VSLighting", "vs_5_1", ppd3dShaderBlob));
//}
//D3D12_SHADER_BYTECODE CPlayerShader::CreatePixelShader(ID3DBlob** ppd3dShaderBlob)
//{
//	return(Shader::CompileShaderFromFile(L"Shaders.hlsl"/*KJ 수정*/, "PSLighting", "ps_5_1", ppd3dShaderBlob));
//}
//
//void CPlayerShader::CreateShader(ID3D12Device* pd3dDevice, ID3D12RootSignature
//	* pd3dGraphicsRootSignature)
//{
//	m_nPipelineStates = 1;
//	m_ppd3dPipelineStates = new ID3D12PipelineState * [m_nPipelineStates];
//	Shader::CreateShader(pd3dDevice, pd3dGraphicsRootSignature);
//}
//
//CObjectsShader::CObjectsShader()
//{
//
//}
//CObjectsShader::~CObjectsShader()
//{
//
//}
//
//D3D12_INPUT_LAYOUT_DESC CObjectsShader::CreateInputLayout()
//{
//	UINT nInputElementDescs = 4; // 4개로 변경 (위치, 법선, 텍스쳐, 법선)
//	D3D12_INPUT_ELEMENT_DESC* pd3dInputElementDescs = new D3D12_INPUT_ELEMENT_DESC[nInputElementDescs];
//
//	pd3dInputElementDescs[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
//	pd3dInputElementDescs[1] = { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }; // NORMAL 약속 추가
//	pd3dInputElementDescs[2] = { "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }; // TEXTURE 
//	pd3dInputElementDescs[3] = { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }; // 오프셋 변경
//
//
//	D3D12_INPUT_LAYOUT_DESC d3dInputLayoutDesc;
//	d3dInputLayoutDesc.pInputElementDescs = pd3dInputElementDescs;
//	d3dInputLayoutDesc.NumElements = nInputElementDescs;
//
//	return(d3dInputLayoutDesc);
//}
//
//D3D12_SHADER_BYTECODE CObjectsShader::CreateVertexShader(ID3DBlob** ppd3dShaderBlob)
//{
//	return(Shader::CompileShaderFromFile(L"Shaders.hlsl", "VSLighting", "vs_5_1", ppd3dShaderBlob));
//}
//D3D12_SHADER_BYTECODE CObjectsShader::CreatePixelShader(ID3DBlob** ppd3dShaderBlob)
//{
//	return(Shader::CompileShaderFromFile(L"Shaders.hlsl", "PSLighting", "ps_5_1", ppd3dShaderBlob));
//}
//
//void CObjectsShader::CreateShader(ID3D12Device* pd3dDevice, ID3D12RootSignature
//	* pd3dGraphicsRootSignature)
//{
//	m_nPipelineStates = 1;
//	m_ppd3dPipelineStates = new ID3D12PipelineState * [m_nPipelineStates];
//	Shader::CreateShader(pd3dDevice, pd3dGraphicsRootSignature);
//}
//
//void CObjectsShader::BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList
//	* pd3dCommandList)
//{
//	CreateShaderVariables(pd3dDevice, pd3dCommandList);
//}
//
//void CObjectsShader::ReleaseObjects()
//{
//	
//}
//
//void CObjectsShader::AnimateObjects(float fTimeElapsed)
//{
//	
//}
//
//void CObjectsShader::ReleaseUploadBuffers()
//{
//	
//}
//
//void CObjectsShader::render(ID3D12GraphicsCommandList* pd3dCommandList, Camera* pCamera)
//{
//	Shader::render(pd3dCommandList, pCamera);
//}
//
//// in Shader.cpp
//
//// --- DebugShader 클래스 구현 ---
//
//DebugShader::DebugShader()
//{
//}
//
//DebugShader::~DebugShader()
//{
//}
//
//D3D12_INPUT_LAYOUT_DESC DebugShader::CreateInputLayout()
//{
//	UINT nInputElementDescs = 2;
//	D3D12_INPUT_ELEMENT_DESC* pd3dInputElementDescs = new D3D12_INPUT_ELEMENT_DESC[nInputElementDescs];
//
//	pd3dInputElementDescs[0] = { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
//	pd3dInputElementDescs[1] = { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 };
//
//	D3D12_INPUT_LAYOUT_DESC d3dInputLayoutDesc;
//	d3dInputLayoutDesc.pInputElementDescs = pd3dInputElementDescs;
//	d3dInputLayoutDesc.NumElements = nInputElementDescs;
//
//	return(d3dInputLayoutDesc);
//}
//
//D3D12_SHADER_BYTECODE DebugShader::CreateVertexShader(ID3DBlob** ppd3dShaderBlob)
//{
//	return(Shader::CompileShaderFromFile(L"Debug.hlsl", "VS_Debug", "vs_5_1", ppd3dShaderBlob));
//}
//
//D3D12_SHADER_BYTECODE DebugShader::CreatePixelShader(ID3DBlob** ppd3dShaderBlob)
//{
//	return(Shader::CompileShaderFromFile(L"Debug.hlsl", "PS_Debug", "ps_5_1", ppd3dShaderBlob));
//}
//
//void DebugShader::CreateShader(ID3D12Device* pd3dDevice, ID3D12RootSignature* pd3dGraphicsRootSignature)
//{
//	m_nPipelineStates = 1;
//	m_ppd3dPipelineStates = new ID3D12PipelineState * [m_nPipelineStates];
//
//	ComPtr<ID3DBlob> pd3dVertexShaderBlob, pd3dPixelShaderBlob;
//
//	D3D12_GRAPHICS_PIPELINE_STATE_DESC d3dPipelineStateDesc;
//	::ZeroMemory(&d3dPipelineStateDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
//	d3dPipelineStateDesc.pRootSignature = pd3dGraphicsRootSignature;
//	d3dPipelineStateDesc.VS = CreateVertexShader(&pd3dVertexShaderBlob);
//	d3dPipelineStateDesc.PS = CreatePixelShader(&pd3dPixelShaderBlob);
//	d3dPipelineStateDesc.RasterizerState = CreateRasterizerState();
//	d3dPipelineStateDesc.BlendState = CreateBlendState();
//	d3dPipelineStateDesc.DepthStencilState = CreateDepthStencilState();
//	d3dPipelineStateDesc.InputLayout = CreateInputLayout();
//	d3dPipelineStateDesc.SampleMask = UINT_MAX;
//	d3dPipelineStateDesc.NumRenderTargets = 1;
//	d3dPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
//	d3dPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
//	d3dPipelineStateDesc.SampleDesc.Count = 1;
//	d3dPipelineStateDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
//
//	d3dPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
//
//	pd3dDevice->CreateGraphicsPipelineState(&d3dPipelineStateDesc, IID_PPV_ARGS(&m_ppd3dPipelineStates[0]));
//
//	if (d3dPipelineStateDesc.InputLayout.pInputElementDescs)
//		delete[] d3dPipelineStateDesc.InputLayout.pInputElementDescs;
//}