#pragma once
#include "GltfShader.h"

class GltfSkinnedShader : public GltfShader
{
public:
	GltfSkinnedShader() = default;
	virtual ~GltfSkinnedShader() = default;

	// 1. PSO 이름을 "skinned_gltf"로 변경하여 Renderer에서 구분
	const std::string& pso_name() const override;

	// 2. BLENDINDICES, BLENDWEIGHT가 추가된 Input Layout 정의
	D3D12_INPUT_LAYOUT_DESC create_input_layout() override;

	// 3. 스키닝 VS 컴파일
	D3D12_SHADER_BYTECODE create_vertex_shader(ComPtr<ID3DBlob>& shader_blob) override;

	// 4. 스키닝용 루트 시그니처 사용 ("skinned")
	std::string required_root_signature() const override;
};