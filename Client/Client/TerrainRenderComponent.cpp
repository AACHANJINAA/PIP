#include "stdafx.h"
#include "TerrainRenderComponent.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "TerrainLoader.h"
#include "Mesh.h"

TerrainRenderComponent::TerrainRenderComponent()
{
    set_pso_name("terrain");
}

TerrainRenderComponent::~TerrainRenderComponent()
{
    if (_terrain_info_cbuffer)
    {
        _terrain_info_cbuffer->Unmap(0, nullptr);
    }
}

void TerrainRenderComponent::pre_render(ID3D12GraphicsCommandList * commandList, Renderer * renderer)
{
    TerrainLoader * terrain_loader = static_cast<TerrainLoader*>(mesh().get());
    if (!terrain_loader)
    {
        CERROR("Failed to cast Mesh to TerrainLoader in TerrainRenderComponent::pre_render!");
        return;
    }

    // --- 1. Create Constant Buffer on first run ---
    if (!_terrain_info_cbuffer)
    {
        ID3D12Device* device = renderer->get_device(); // Assuming Renderer has a get_device() method
        UINT buffer_size = (sizeof(TerrainLoader::TerrainInfo) + 255) & ~255; // Constant buffer alignment
        auto heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(buffer_size);

        HRESULT hr = device->CreateCommittedResource(
            &heap_props,
            D3D12_HEAP_FLAG_NONE,
            &buffer_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&_terrain_info_cbuffer)
        );

        if (FAILED(hr)) { CERROR("Failed to create terrain info cbuffer"); return; }
        _terrain_info_cbuffer->Map(0, nullptr, reinterpret_cast<void**>(&_terrain_info_cbuffer_cpu_address));
    }

    // --- 2. Update and Bind TerrainInfo Constant Buffer (Root Param 2) ---
    const auto& terrain_info = terrain_loader->get_terrain_info();
    memcpy(_terrain_info_cbuffer_cpu_address, &terrain_info, sizeof(TerrainLoader::TerrainInfo));
    commandList->SetGraphicsRootConstantBufferView(2, _terrain_info_cbuffer->GetGPUVirtualAddress());


    // --- 3. Bind PBR Textures manually for Terrain Shader (Root Param 4) ---
    auto* rm = ResourceManager::instance();
    auto* mat_info = rm->get_material_info(terrain_loader->get_material_name());
    if (!mat_info) {
        CERROR("Material info not found for terrain.");
        return;
    }

    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> texture_handles;

    // Helper to get texture or default
    auto get_tex_handle_or_default = [&](const std::string& path, const std::string& default_name) -> D3D12_CPU_DESCRIPTOR_HANDLE {
        // Path can be empty, so check first
        if (path.empty()) {
            return rm->get_texture(default_name)->cpu_handle;
        }
        auto* tex_info = rm->get_texture(path);
        if (tex_info && tex_info->cpu_handle.ptr != 0) {
            return tex_info->cpu_handle;
        }
        // Return default if specific texture not found
        return rm->get_texture(default_name)->cpu_handle;
        };

    texture_handles.push_back(get_tex_handle_or_default(mat_info->base_color_texture_path, "__DEFAULT_WHITE__")); // t0
    texture_handles.push_back(get_tex_handle_or_default(mat_info->normal_texture_path, "__DEFAULT_NORMAL__"));   // t1
    texture_handles.push_back(get_tex_handle_or_default(mat_info->metallic_roughness_texture_path, "__DEFAULT_ORM__")); // t2
    texture_handles.push_back(get_tex_handle_or_default(mat_info->emissive_texture_path, "__DEFAULT_BLACK__"));  // t3
    texture_handles.push_back(get_tex_handle_or_default(terrain_loader->get_detail_texture_key(), "__DEFAULT_WHITE__")); // t4 - detail

    // Bind the table of 4 textures to root parameter 4
    renderer->bind_texture_table(commandList, 4, texture_handles);
}
