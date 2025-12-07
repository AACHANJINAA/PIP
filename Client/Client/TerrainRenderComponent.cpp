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

void TerrainRenderComponent::pre_render(ID3D12GraphicsCommandList * commandList, Renderer * renderer)
{
    TerrainLoader * terrain_loader = static_cast<TerrainLoader*>(mesh().get());
    if (!terrain_loader)
        {
            CERROR("Failed to cast Mesh to TerrainLoader in TerrainRenderComponent::pre_render!");
            return;
        }
    auto* rm = ResourceManager::instance();
    std::string material_name = terrain_loader->get_material_name();
    auto* mat_info = rm->get_material_info(material_name);

    if (!mat_info)
    {
        CERROR("Material info not found for Terrain in TerrainRenderComponent::pre_render!");
             return;
         }
    
         // [수정] 텍스처 핸들을 담을 벡터를 준비합니다.
         std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> texture_handles;
    
         // 1. Base Texture (t0)
         auto* base_tex = rm->get_texture(mat_info->base_color_texture_path);
     if (base_tex && base_tex->cpu_handle.ptr != 0)
         {
             texture_handles.push_back(base_tex->cpu_handle);
         }
     else
      {
          CERROR("Base texture not found for Terrain, binding default.");
          // TODO: Renderer에 기본 텍스처 핸들을 가져오는 함수가 필요합니다.
              // 임시로 비워두거나, 기본 텍스처를 바인딩해야 합니다.
              // texture_handles.push_back(renderer->get_default_texture_handle());
              return;
      }
    
      // 2. Detail Texture (t1)
      auto* detail_tex = rm->get_texture(mat_info->normal_texture_path);
   if (detail_tex && detail_tex->cpu_handle.ptr != 0)
       {
           texture_handles.push_back(detail_tex->cpu_handle);
       }
    else
        {
            CERROR("Detail texture not found for Terrain, binding default.");
            // TODO: 기본 텍스처 핸들 바인딩
             // texture_handles.push_back(renderer->get_default_texture_handle());
             return;
        }
    
        // [수정] 두 텍스처를 하나의 디스크립터 테이블로 묶어 Root Parameter [4]번에 바인딩합니다.
        renderer->bind_texture_table(commandList, 4, texture_handles);
}
