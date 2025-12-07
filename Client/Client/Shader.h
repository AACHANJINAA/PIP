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

	// 이 템플릿 메서드는 파생 클래스가 정의한 정보를 바탕으로 PSO를 생성하는 전체 과정을 담당합니다.
	ComPtr<ID3D12PipelineState> create_pso(ID3D12Device* device, ID3D12RootSignature* root_signature);

	// --- 파생 클래스가 반드시 구현해야 할 정보 ---

	// 자신의 PSO를 어떤 이름으로 저장할지 알려줘야 합니다. (예: "default", "skinned")
	virtual const std::string& pso_name() const = 0;

	// 자신에게 어떤 루트 시그니처가 필요한지 이름을 반환합니다. (기본값은 "default")
	// 오버라이드하지 않으면 기본 루트 시그니처를 사용합니다.
	virtual std::string required_root_signature() const { return "default"; }

protected:
	// --- 파생 클래스가 PSO 생성을 위해 반드시 구현해야 할 재료들 ---

	virtual D3D12_INPUT_LAYOUT_DESC create_input_layout() = 0;
	virtual D3D12_SHADER_BYTECODE create_vertex_shader(ComPtr<ID3DBlob>& shader_blob) = 0;
	virtual D3D12_SHADER_BYTECODE create_pixel_shader(ComPtr<ID3DBlob>& shader_blob) = 0;

	// --- 필요 시 파생 클래스가 재정의(override)할 수 있는 옵션들 ---

	// 기본 토폴로지는 삼각형 리스트입니다. (라인 렌더링 등에서 재정의)
	virtual D3D12_PRIMITIVE_TOPOLOGY_TYPE primitive_topology_type() const {
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	}

	// 기본 렌더링 상태를 반환하는 가상 함수들 (필요 시 재정의)
	virtual D3D12_RASTERIZER_DESC create_rasterizer_state();
	virtual D3D12_DEPTH_STENCIL_DESC create_depth_stencil_state();
	virtual D3D12_BLEND_DESC create_blend_state();

public:
	// --- 모든 셰이더가 공용으로 사용하는 헬퍼 함수 ---
	static D3D12_SHADER_BYTECODE compile_shader_from_file(const std::wstring& file_name, LPCSTR
		shader_name, LPCSTR shader_profile, ComPtr<ID3DBlob>& shader_blob);

	//  [추가] 각 객체를 그리기 직전에 호출될 함수. 셰이더가 객체별 리소스 바인딩을 담당합니다.    
	virtual void update_per_object(ID3D12GraphicsCommandList* commandList, class Renderer* renderer, GameObject* object) {}
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


//class CPlayerShader : public Shader
//{
//public:
//	CPlayerShader();
//	virtual ~CPlayerShader();
//	virtual D3D12_INPUT_LAYOUT_DESC CreateInputLayout();
//	virtual D3D12_SHADER_BYTECODE CreateVertexShader(ID3DBlob** ppd3dShaderBlob);
//	virtual D3D12_SHADER_BYTECODE CreatePixelShader(ID3DBlob** ppd3dShaderBlob);
//	virtual void CreateShader(ID3D12Device* pd3dDevice, ID3D12RootSignature* pd3dGraphicsRootSignature);
//};
//
//
////“CObjectsShader” 클래스는 게임 객체들을 포함하는 셰이더 객체이다. 
//class CObjectsShader : public Shader
//{
//public:
//	CObjectsShader();
//	virtual ~CObjectsShader();
//	virtual void BuildObjects(ID3D12Device * pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
//	virtual void AnimateObjects(float fTimeElapsed);
//	virtual void ReleaseObjects();
//	virtual D3D12_INPUT_LAYOUT_DESC CreateInputLayout();
//	virtual D3D12_SHADER_BYTECODE CreateVertexShader(ID3DBlob** ppd3dShaderBlob);
//	virtual D3D12_SHADER_BYTECODE CreatePixelShader(ID3DBlob** ppd3dShaderBlob);
//	virtual void CreateShader(ID3D12Device * pd3dDevice, ID3D12RootSignature* pd3dGraphicsRootSignature);
//	virtual void ReleaseUploadBuffers();
//	virtual void render(ID3D12GraphicsCommandList * pd3dCommandList, Camera * pCamera);
//protected:
//	int m_nObjects = 0;
//};
//
//class DebugShader : public Shader
//{
//public:
//	DebugShader();
//	virtual ~DebugShader();
//
//	// Shader 클래스의 가상 함수들을 오버라이드합니다.
//	virtual D3D12_INPUT_LAYOUT_DESC CreateInputLayout() override;
//
//	// 셰이더 바이트코드를 생성하는 함수들을 오버라이드합니다.
//	virtual D3D12_SHADER_BYTECODE CreateVertexShader(ID3DBlob** ppd3dShaderBlob) override;
//	virtual D3D12_SHADER_BYTECODE CreatePixelShader(ID3DBlob** ppd3dShaderBlob) override;
//
//	// 파이프라인 상태 객체(PSO)를 생성하는 함수를 오버라이드합니다.
//	virtual void CreateShader(ID3D12Device* pd3dDevice, ID3D12RootSignature* pd3dGraphicsRootSignature) override;
//};