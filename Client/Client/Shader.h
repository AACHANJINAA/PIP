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
	virtual ComPtr<ID3D12PipelineState> create_pso(ID3D12Device* device, ID3D12RootSignature* root_signature);

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
	virtual D3D12_SHADER_BYTECODE create_geometry_shader(ComPtr<ID3DBlob>& shader_blob) { return { nullptr, 0 }; } // GS는 적용이 거의 안되니 디폴트 이걸로

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

	virtual DXGI_FORMAT get_dsv_format() const {
		return DXGI_FORMAT_D24_UNORM_S8_UINT;
	}
	virtual UINT get_num_render_targets() const { return 1; }
	virtual DXGI_FORMAT get_rtv_format(UINT index = 0) const {
		return DXGI_FORMAT_R8G8B8A8_UNORM;
	}
};