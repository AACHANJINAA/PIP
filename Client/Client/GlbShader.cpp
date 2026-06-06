#include "stdafx.h"
#include "GlbShader.h"

const std::string& GlbShader::pso_name() const
{
    static const std::string name = "skinned";
    return name;
}

// 스키닝된 정점 구조에 맞는 Input Layout을 정의합니다.
// [중요] new로 동적 할당하는 대신, static const 배열로 만들어 메모리 누수를 방지합니다.
D3D12_INPUT_LAYOUT_DESC GlbShader::create_input_layout()
{
    static const D3D12_INPUT_ELEMENT_DESC input_layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
        	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,
        	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24,
        	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BLENDINDICES", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "BLENDWEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 48,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
    return { input_layout, _countof(input_layout) };
}

// 스키닝을 처리하는 Vertex Shader("VSSkinning")를 지정합니다.
D3D12_SHADER_BYTECODE GlbShader::create_vertex_shader(ComPtr<ID3DBlob>& shader_blob)
{
    return compile_shader_from_file(L"GLB_Shader.hlsl", "VSSkinning", "vs_5_1", shader_blob);
}

// Pixel Shader("PSSkinning")를 지정합니다.
D3D12_SHADER_BYTECODE GlbShader::create_pixel_shader(ComPtr<ID3DBlob>& shader_blob)
{
    return compile_shader_from_file(L"GLB_Shader.hlsl", "PSSkinning", "ps_5_1", shader_blob);
}

// 이 셰이더는 "skinned" 라는 이름의 루트 시그니처가 필요하다고 명시합니다.
std::string GlbShader::required_root_signature() const
{
    return "skinned";
}