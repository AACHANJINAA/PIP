#include "stdafx.h"
#include "RenderComponent.h"
#include "TransformComponent.h"
#include "GameObject.h"
#include "Shader.h"

Material_Shader::Material_Shader()
{
}
Material_Shader::~Material_Shader()
{
}

void Material_Shader::set_shader(const std::shared_ptr<Shader>& shader)
{
	_shader = shader;
}

void Material_Shader::set_shader_root_signature(ComPtr<ID3D12RootSignature> root_signature)
{
	if (_shader)
	{
		_rootSignature = root_signature;
	}
}

void Material_Shader::set_root_signature(ComPtr<ID3D12GraphicsCommandList> command_list)
{
	if (_rootSignature)
	{
		command_list->SetGraphicsRootSignature(_rootSignature.Get());
	}
}
//------------------------------------------------------------ RenderComponent ------------------------------------------------------------//

RenderComponent::RenderComponent()
{
	set_name("RenderComponent");
}

void RenderComponent::render(ID3D12GraphicsCommandList* commandList, Camera* camera)
{
	if (!_shader || !_mesh) return;

	// --- 지역 UpdateShaderVariable의 역할이 여기로 왔습니다 ---
	// 1. 이 GameObject의 Transform 컴포넌트로부터 월드 행렬을 가져옵니다.
	const XMFLOAT4X4& worldMatrix = game_object()->transform()->world_matrix();

	// 2. 셰이더(또는 머티리얼)에게 월드 행렬을 GPU로 보내라고 명령합니다.
	//    이 역할을 할 새로운 함수가 Shader/Material 클래스에 필요합니다.
	_shader->update_object_constants(commandList, &worldMatrix);
	// ---------------------------------------------------------

	// 3. 메시를 그립니다.
	_mesh->Render(commandList);
}
