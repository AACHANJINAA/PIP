#pragma once
#include "GameObject.h"
#include "Camera.h"

//게임 객체의 정보를 셰이더에게 넘겨주기 위한 구조체(상수 버퍼)이다. 
//struct CB_GAMEOBJECT_INFO
//{
//	XMFLOAT4X4 _4x4World;
//};
class Shader
{
public:
	Shader() = default;
	virtual ~Shader() = default;

	// --- Template Method ---
	// PSO 생성의 전체 흐름을 정의하는 '일반' 함수
	ID3D12PipelineState* create_pso(ID3D12Device* device, ID3D12RootSignature* rootSignature);

	// --- 렌더링 시 필요한 함수들 ---
	virtual void update_shader_variables(ID3D12GraphicsCommandList* commandList, void* context);
	virtual void render(ID3D12GraphicsCommandList* commandList, class Camera* camera);

	// --- 유틸리티 함수 (static) ---
	static D3D12_SHADER_BYTECODE compile_shader_from_file(const std::wstring& fileName, LPCSTR shaderName, 
		LPCSTR shaderProfile, ID3DBlob** shaderBlob);
	static D3D12_RASTERIZER_DESC create_rasterizer_state();
	static D3D12_DEPTH_STENCIL_DESC create_depth_stencil_state();
	static D3D12_BLEND_DESC create_blend_state();

protected:
	// --- 파생 클래스가 반드시 오버라이드해야 할 세부 내용들 (순수 가상 함수) ---
	virtual D3D12_INPUT_LAYOUT_DESC create_input_layout() = 0;
	virtual D3D12_SHADER_BYTECODE create_vertex_shader(ID3DBlob** shaderBlob) = 0;
	virtual D3D12_SHADER_BYTECODE create_pixel_shader(ID3DBlob** shaderBlob) = 0;
};
//셰이더 소스 코드를 컴파일하고 그래픽스 상태 객체를 생성한다. 
/*class Shader
{
public:
	Shader();
	virtual ~Shader();

private:
	int m_nReferences = 0;

public:
	void AddRef() { m_nReferences++; }
	void Release() { if (--m_nReferences <= 0) delete this; }

	virtual D3D12_INPUT_LAYOUT_DESC CreateInputLayout();
	virtual D3D12_RASTERIZER_DESC CreateRasterizerState();
	virtual D3D12_BLEND_DESC CreateBlendState();
	virtual D3D12_DEPTH_STENCIL_DESC CreateDepthStencilState();
	virtual D3D12_SHADER_BYTECODE CreateVertexShader(ID3DBlob** ppd3dShaderBlob);
	virtual D3D12_SHADER_BYTECODE CreatePixelShader(ID3DBlob** ppd3dShaderBlob);
	virtual D3D12_SHADER_BYTECODE CompileShaderFromFile(const std::wstring& pszFileName, LPCSTR pszShaderName,
													   LPCSTR pszShaderProfile, ID3DBlob** ppd3dShaderBlob); 

	virtual void CreateShader(ID3D12Device* pd3dDevice, ID3D12RootSignature
		* pd3dGraphicsRootSignature);

	virtual void CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList
		* pd3dCommandList);

	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList);
	virtual void ReleaseShaderVariables();

	virtual void update_shader_variable(ID3D12GraphicsCommandList* pd3dCommandList, XMFLOAT4X4* pxmf4x4World);

	virtual void OnPrepareRender(ID3D12GraphicsCommandList* pd3dCommandList);
	virtual void render(ID3D12GraphicsCommandList* pd3dCommandList, Camera* pCamera);

protected:
	ID3D12PipelineState** m_ppd3dPipelineStates = NULL;
	int m_nPipelineStates = 0;
};*/


class CPlayerShader : public Shader
{
public:
	CPlayerShader();
	virtual ~CPlayerShader();
	virtual D3D12_INPUT_LAYOUT_DESC CreateInputLayout();
	virtual D3D12_SHADER_BYTECODE CreateVertexShader(ID3DBlob** ppd3dShaderBlob);
	virtual D3D12_SHADER_BYTECODE CreatePixelShader(ID3DBlob** ppd3dShaderBlob);
	virtual void CreateShader(ID3D12Device* pd3dDevice, ID3D12RootSignature* pd3dGraphicsRootSignature);
};


//“CObjectsShader” 클래스는 게임 객체들을 포함하는 셰이더 객체이다. 
class CObjectsShader : public Shader
{
public:
	CObjectsShader();
	virtual ~CObjectsShader();
	virtual void BuildObjects(ID3D12Device * pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	virtual void AnimateObjects(float fTimeElapsed);
	virtual void ReleaseObjects();
	virtual D3D12_INPUT_LAYOUT_DESC CreateInputLayout();
	virtual D3D12_SHADER_BYTECODE CreateVertexShader(ID3DBlob** ppd3dShaderBlob);
	virtual D3D12_SHADER_BYTECODE CreatePixelShader(ID3DBlob** ppd3dShaderBlob);
	virtual void CreateShader(ID3D12Device * pd3dDevice, ID3D12RootSignature* pd3dGraphicsRootSignature);
	virtual void ReleaseUploadBuffers();
	virtual void render(ID3D12GraphicsCommandList * pd3dCommandList, Camera * pCamera);
protected:
	int m_nObjects = 0;
};

class DebugShader : public Shader
{
public:
	DebugShader();
	virtual ~DebugShader();

	// Shader 클래스의 가상 함수들을 오버라이드합니다.
	virtual D3D12_INPUT_LAYOUT_DESC CreateInputLayout() override;

	// 셰이더 바이트코드를 생성하는 함수들을 오버라이드합니다.
	virtual D3D12_SHADER_BYTECODE CreateVertexShader(ID3DBlob** ppd3dShaderBlob) override;
	virtual D3D12_SHADER_BYTECODE CreatePixelShader(ID3DBlob** ppd3dShaderBlob) override;

	// 파이프라인 상태 객체(PSO)를 생성하는 함수를 오버라이드합니다.
	virtual void CreateShader(ID3D12Device* pd3dDevice, ID3D12RootSignature* pd3dGraphicsRootSignature) override;
};