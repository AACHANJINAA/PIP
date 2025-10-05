#include "stdafx.h"
#include "DebugShader.h"
const std::string& DebugShader::pso_name() const
{
    static const std::string name = "debug";
    return name;
}

D3D12_INPUT_LAYOUT_DESC DebugShader::create_input_layout()
{
    // Renderer.cpp에 있던 "debug" PSO의 Input Layout 정보를 그대로 가져옵니다.
    static const D3D12_INPUT_ELEMENT_DESC input_layout[] = {
          { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
              D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };
    return { input_layout, _countof(input_layout) };
}

D3D12_SHADER_BYTECODE DebugShader::create_vertex_shader(ComPtr<ID3DBlob>& shader_blob)
{
    return compile_shader_from_file(L"Debug.hlsl", "VS_Debug", "vs_5_1", shader_blob);
}

D3D12_SHADER_BYTECODE DebugShader::create_pixel_shader(ComPtr<ID3DBlob>& shader_blob)
{
    return compile_shader_from_file(L"Debug.hlsl", "PS_Debug", "ps_5_1", shader_blob);
}

D3D12_PRIMITIVE_TOPOLOGY_TYPE DebugShader::primitive_topology_type() const
{
    // 디버그 셰이더는 라인으로 렌더링합니다.
    return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
}