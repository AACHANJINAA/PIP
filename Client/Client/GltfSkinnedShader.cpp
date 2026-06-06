#include "stdafx.h"
#include "GltfSkinnedShader.h"

#include "LightManager.h"

const std::string& GltfSkinnedShader::pso_name() const
{
	static const std::string name = "skinned";
	return name;
}

D3D12_INPUT_LAYOUT_DESC GltfSkinnedShader::create_input_layout()
{
	// GltfSkinnedVertex 구조체와 일치해야 합니다.
	static const D3D12_INPUT_ELEMENT_DESC d3d_input_element_descs[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

		// [추가] 뼈대 인덱스 (UINT4) - 바이트 오프셋 48 (float 12개 뒤)
		{ "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_UINT, 0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },

		// [추가] 가중치 (FLOAT4) - 바이트 오프셋 64 (UINT 4개 뒤)
		{ "BLENDWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 64, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	return D3D12_INPUT_LAYOUT_DESC{ d3d_input_element_descs, _countof(d3d_input_element_descs) };
}

D3D12_SHADER_BYTECODE GltfSkinnedShader::create_vertex_shader(ComPtr<ID3DBlob>& shader_blob)
{
	// Gltf_Skinned_Shader.hlsl 파일의 VS_GLTF_SKINNED 함수 진입
	return compile_shader_from_file(L"Gltf_Skinned_Shader.hlsl", "VS_GLTF_SKINNED", "vs_5_1", shader_blob);
}

std::string GltfSkinnedShader::required_root_signature() const
{
	// RootSignature.cpp에 정의된 SkinnedRootSignatureGenerator의 이름
	return "skinned";
}

void GltfSkinnedShader::update_per_object(ID3D12GraphicsCommandList* command_list, class Renderer* renderer,
	GameObject* object)
{
	GltfShader::update_per_object(command_list, renderer, object);

	//LightManager::instance()->bind(command_list, 3);
}
