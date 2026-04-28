#include "stdafx.h"
#include "TerrainRenderComponent.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "TerrainLoader.h"
#include "GameObject.h"        
#include "TransformComponent.h"

TerrainRenderComponent::TerrainRenderComponent()
{
    set_pso_name("terrain");
    set_frustum_culling_enabled(true);
    set_skip_occlusion(true);
}

TerrainRenderComponent::~TerrainRenderComponent()
{
    if (_terrain_info_cbuffer)
    {
        _terrain_info_cbuffer->Unmap(0, nullptr);
    }

    if (_layer_info_cbuffer)
    {
        _layer_info_cbuffer->Unmap(0, nullptr);
    }
}

void TerrainRenderComponent::pre_render(ID3D12GraphicsCommandList * commandList, Renderer * renderer)
{
    TerrainLoader* terrain_loader = static_cast<TerrainLoader*>(mesh().get());
    if (!terrain_loader)
    {
        CERROR("Failed to cast Mesh to TerrainLoader");
        return;
    }

    ID3D12Device* device = renderer->get_device();
    auto* rm = ResourceManager::instance();

    // ===== 1. TerrainInfo Constant Buffer (b2) =====
    if (!_terrain_info_cbuffer)
    {
        UINT buffer_size = (sizeof(TerrainLoader::TerrainInfo) + 255) & ~255;
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

    const auto& terrain_info = terrain_loader->get_terrain_info();
    memcpy(_terrain_info_cbuffer_cpu_address, &terrain_info, sizeof(TerrainLoader::TerrainInfo));
    commandList->SetGraphicsRootConstantBufferView(2, _terrain_info_cbuffer->GetGPUVirtualAddress());

    // ===== 2. LayerInfo Constant Buffer (b6) =====
    // [추가] cbLayerInfo 구조체
    struct LayerInfoCB
    {
        int NumLayers;
        float LayerTiling;
        float Padding[2];
    };

    if (!_layer_info_cbuffer)
    {
        UINT buffer_size = (sizeof(LayerInfoCB) + 255) & ~255;
        auto heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(buffer_size);

        HRESULT hr = device->CreateCommittedResource(
            &heap_props,
            D3D12_HEAP_FLAG_NONE,
            &buffer_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&_layer_info_cbuffer)
        );

        if (FAILED(hr)) { CERROR("Failed to create layer info cbuffer"); return; }
        _layer_info_cbuffer->Map(0, nullptr, reinterpret_cast<void**>(&_layer_info_cbuffer_cpu_address));
    }

    LayerInfoCB layer_cb = {};
    layer_cb.NumLayers = terrain_loader->has_layers() ? static_cast<int>(terrain_loader->get_layers().size()) : 0;
    layer_cb.LayerTiling = 128.0f; // 레이어별 타일링 값 
    memcpy(_layer_info_cbuffer_cpu_address, &layer_cb, sizeof(LayerInfoCB));
    commandList->SetGraphicsRootConstantBufferView(8, _layer_info_cbuffer->GetGPUVirtualAddress());

    // ===== 3. 텍스처 바인딩 분기 처리 =====
    if (terrain_loader->has_layers())
    {
        // ===== Multi-Layer 지형: t12~t15 바인딩 =====
        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> layer_texture_handles;

        // t12: Weightmap Array
        auto* weightmap_array = rm->get_texture(terrain_loader->get_weightmap_array_key());
        if (!weightmap_array) {
            CERROR("Weightmap array not found: " << terrain_loader->get_weightmap_array_key());
            return;
        }
        layer_texture_handles.push_back(weightmap_array->cpu_handle);

        // t13~t15: Layer Albedo/Normal/Roughness Arrays
        auto* albedo_array = rm->get_texture(terrain_loader->get_albedo_array_key());
        auto* normal_array = rm->get_texture(terrain_loader->get_normal_array_key());
        auto* roughness_array = rm->get_texture(terrain_loader->get_roughness_array_key());

        if (albedo_array && normal_array && roughness_array)
        {
            layer_texture_handles.push_back(albedo_array->cpu_handle);   // t13
            layer_texture_handles.push_back(normal_array->cpu_handle);   // t14
            layer_texture_handles.push_back(roughness_array->cpu_handle); // t15
        }
        else
        {
            CERROR("Layer texture arrays not found for terrain");
            return;
        }
        renderer->bind_texture_table(commandList, 9, layer_texture_handles);
    }
    else
    {
        // ===== 단일 지형: t0~t4 바인딩 (기존 방식) =====
        auto* mat_info = rm->get_material_info(terrain_loader->get_material_name());
        if (!mat_info) {
            CERROR("Material info not found for terrain");
            return;
        }

        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> texture_handles;

        auto get_tex_handle_or_default = [&](const std::string& path, const std::string& default_name) -> D3D12_CPU_DESCRIPTOR_HANDLE {
            if (path.empty()) {
                return rm->get_texture(default_name)->cpu_handle;
            }
            auto* tex_info = rm->get_texture(path);
            if (tex_info && tex_info->cpu_handle.ptr != 0) {
                return tex_info->cpu_handle;
            }
            return rm->get_texture(default_name)->cpu_handle;
            };

        texture_handles.push_back(get_tex_handle_or_default(mat_info->base_color_texture_path, "__DEFAULT_WHITE__"));
        texture_handles.push_back(get_tex_handle_or_default(mat_info->normal_texture_path, "__DEFAULT_NORMAL__"));
        texture_handles.push_back(get_tex_handle_or_default(mat_info->metallic_roughness_texture_path, "__DEFAULT_ORM__"));
        texture_handles.push_back(get_tex_handle_or_default(mat_info->emissive_texture_path, "__DEFAULT_BLACK__"));
        texture_handles.push_back(get_tex_handle_or_default(terrain_loader->get_detail_texture_key(), "__DEFAULT_WHITE__"));

        renderer->bind_texture_table(commandList, 4, texture_handles);
    }


    //// [추가] Terrain IBL 텍스처 바인딩 (루트 시그니처 인덱스 5)
    //auto* rm = ResourceManager::instance();
    //D3D12_CPU_DESCRIPTOR_HANDLE prefiltered = rm->get_ibl_prefiltered_srv_cpu();
    //D3D12_CPU_DESCRIPTOR_HANDLE brdfLut = rm->get_ibl_brdf_lut_srv_cpu();

    //if (prefiltered.ptr != 0 && brdfLut.ptr != 0)
    //{
    //    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> ibl_handles = { prefiltered, brdfLut };
    //    renderer->bind_texture_table(commandList, 5, ibl_handles);
    //}
}

BoundingOrientedBox TerrainRenderComponent::get_world_bounding_box() const
{
    if (!_mesh)
    {
        return BoundingOrientedBox();
    }

    BoundingOrientedBox localObb = _mesh->bounding_box();
    BoundingOrientedBox worldObb;

    if (game_object() && game_object()->transform())
    {
        XMMATRIX worldMatrix = XMLoadFloat4x4(&game_object()->transform()->world_matrix());
        localObb.Transform(worldObb, worldMatrix);
    }
    else
    {
        worldObb = localObb;
    }

    return worldObb;
}