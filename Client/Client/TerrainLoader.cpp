#include "stdafx.h"
#include "TerrainLoader.h"
#include "ResourceManager.h"
#include <fstream>
#include <filesystem>

using namespace DirectX;

TerrainLoader::TerrainLoader(const std::string& heightmap_json_path)
{
    
    if (!_terrainData.LoadFromJSON(heightmap_json_path))
    {
        CERROR("Failed to load terrain data from: " << heightmap_json_path);
    }

    const auto& info = _terrainData.GetInfo();
    _heightmapTextureKey = _terrainData.GetHeightMapPath();

    // _terrainInfo 값 채우기 (쉐이더로 전달됨)
    _terrainInfo.bounds = XMFLOAT4(info.min_x, info.max_x, info.min_z, info.max_z);
    _terrainInfo.size = XMFLOAT2(info.width, info.height);
    _terrainInfo.height_scale = info.height_scale;
    _terrainInfo.min_height = info.min_height;

    // 2. Flat Grid Mesh 
    int grid_width = static_cast<int>(info.width) - 1;
    int grid_height = static_cast<int>(info.height) - 1;
    create_flat_grid(grid_width, grid_height);
}

namespace
{
 DirectX::XMFLOAT3 get_normal_at(const common::TerrainData & terrainData, int x, int z)
     {
         const auto& info = terrainData.GetInfo();
         int width = static_cast<int>(info.width);
         int length = static_cast<int>(info.height);

         if (x < 0 || z < 0 || x >= width || z >= length)
         {
             return DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
         }

         // 주변 픽셀 인덱스 (경계 처리)
         int x_plus_1 = min(x + 1, width - 1);
	     int x_minus_1 = max(x - 1, 0);
	     int z_plus_1 = min(z + 1, length - 1);
	     int z_minus_1 = max(z - 1, 0);

         // 주변 높이 값 가져오기
         float height_y1 = terrainData.GetHeightAt(info.min_x + x * (info.max_x - info.min_x) / (width - 1), info.min_z + z_minus_1 * (info.max_z - info.min_z) / (length - 1));
         float height_y2 = terrainData.GetHeightAt(info.min_x + x * (info.max_x - info.min_x) / (width - 1), info.min_z + z_plus_1 * (info.max_z - info.min_z) / (length - 1));
		 float height_x1 = terrainData.GetHeightAt(info.min_x + x_minus_1 * (info.max_x - info.min_x) / (width - 1), info.min_z + z * (info.max_z - info.min_z) / (length - 1));
		 float height_x2 = terrainData.GetHeightAt(info.min_x + x_plus_1 * (info.max_x - info.min_x) / (width - 1), info.min_z + z * (info.max_z - info.min_z) / (length - 1));
 
		 // 두 개의 접선 벡터 계산
		 DirectX::XMFLOAT3 edge1(0.0f, height_y2 - height_y1, 2.0f * (info.max_z - info.min_z) / (length - 1));
		 DirectX::XMFLOAT3 edge2(2.0f * (info.max_x - info.min_x) / (width - 1), height_x2 - height_x1, 0.0f);
		 
		 // 외적을 통해 법선 벡터 계산 및 정규화
         DirectX::XMVECTOR normal = DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&edge1), DirectX::XMLoadFloat3(&edge2));
 		 normal = DirectX::XMVector3Normalize(normal);

         DirectX::XMFLOAT3 result;
 		 DirectX::XMStoreFloat3(&result, normal);

		 return result;
	}
}


void TerrainLoader::create_flat_grid(int grid_width, int grid_height)
{
const auto& info = _terrainData.GetInfo();

    int vertex_count_x = grid_width + 1;
int vertex_count_z = grid_height + 1;

    float world_width = info.max_x - info.min_x;
float world_height = info.max_z - info.min_z;

    float cell_width = world_width / grid_width;
float cell_height = world_height / grid_height;

    std::vector<IlluminatedVertex> vertices;
vertices.reserve(vertex_count_x * vertex_count_z);

    for (int z = 0; z < vertex_count_z; ++z)
    {
        for (int x = 0; x < vertex_count_x; ++x)
         {
        	IlluminatedVertex v;
			float world_x_pos = info.min_x + x * cell_width;
			float world_z_pos = info.min_z + z * cell_height;

	         // CPU에서 높이 계산
	         float height = _terrainData.GetHeightAt(world_x_pos, world_z_pos);

	         v._position = XMFLOAT3(world_x_pos, height, world_z_pos);

	         // CPU에서 법선 계산
	         v._normal = get_normal_at(_terrainData, x, z);

	         v._texCoord = XMFLOAT2(
	              static_cast<float>(x) / grid_width,
	              static_cast<float>(z) / grid_height);

	         v._tangent = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
			 v._diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

	         vertices.emplace_back(v);
      }
    }

    set_vertex_data_buffer(vertices);

    _indices.clear();
	_indices.reserve(grid_width * grid_height * 6);

    for (int z = 0; z < grid_height; ++z)
    {
        for (int x = 0; x < grid_width; ++x)
    {
        int v0 = z * vertex_count_x + x;
        int v1 = v0 + 1;
        int v2 = (z + 1) * vertex_count_x + x;
        int v3 = v2 + 1;
 
             _indices.emplace_back(v0);
         _indices.emplace_back(v2);
         _indices.emplace_back(v1);
 
             _indices.emplace_back(v1);
         _indices.emplace_back(v2);
         _indices.emplace_back(v3);
     }
     }

     _primitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

     float max_height = info.height_scale;
	_orientedBoundingBox = CreateOOBB(
         XMFLOAT3(info.min_x, 0.0f, info.min_z),
         XMFLOAT3(info.max_x, max_height, info.max_z)
	);

     CLOG("Terrain Grid Created: " << vertex_count_x << " x " << vertex_count_z << " vertices, " << _indices.size() / 3 << " triangles");
}


void TerrainLoader::load_textures_to_resource_manager(const std::string& material_gltf_path)
{
    const auto& info = _terrainData.GetInfo();
    auto* rm = ResourceManager::instance();

    // 1. HeightMap
    int width = static_cast<int>(info.width);
    int height = static_cast<int>(info.height);

    auto* heightmap_tex = rm->load_heightmap_from_raw(_heightmapTextureKey, width, height);
    if (!heightmap_tex)
    {
        CERROR("Failed to load heightmap texture: " << _heightmapTextureKey);
    }
    else
    {
        CLOG("HeightMap texture loaded to GPU: " << _heightmapTextureKey);
    }

    // 2. glTF Material  (ResourceManager)
    auto material_names = rm->load_materials_from_gltf(material_gltf_path);
    if (!material_names.empty())
    {
        _materialName = material_names[0];
        CLOG("Material loaded from glTF: " << _materialName);

        // Material 
        auto* mat_info = rm->get_material_info(_materialName);
        if (mat_info)
        {
            // Base Color Texture (T_ground_Moss_D.png)
            if (!mat_info->base_color_texture_path.empty())
            {
                _baseTextureKey = mat_info->base_color_texture_path;
                CLOG("  Base Texture: " << _baseTextureKey);
            }

            // Normal Texture Detail (T_Ground_Moss_N.png)
            if (!mat_info->normal_texture_path.empty())
            {
                _detailTextureKey = mat_info->normal_texture_path;
                CLOG("  Detail Texture: " << _detailTextureKey);
            }
        }
        else
        {
            CERROR("Failed to get material info for: " << _materialName);
        }
    }
    else
    {
        CERROR("No materials found in glTF: " << material_gltf_path);
    }
}

void TerrainLoader::render(ID3D12GraphicsCommandList* command_list)
{
    if (!is_uploaded())
    {
        CERROR("TerrainLoader: Mesh not uploaded to GPU!");
        return;
    }

    // Vertex/Index Buffer 
    command_list->IASetVertexBuffers(0, 1, &_vertexBufferView);
    command_list->IASetIndexBuffer(&_indexBufferView);
    command_list->IASetPrimitiveTopology(_primitiveTopology);

    // Terrain Constant Buffer  (Root Parameter 2)
    // TerrainInfo 32bit constants
   // command_list->SetGraphicsRoot32BitConstants(2, 8, &_terrainInfo, 0);

    // Draw Call
    command_list->DrawIndexedInstanced(
        static_cast<UINT>(_indices.size()), 1, 0, 0, 0
    );
}

float TerrainLoader::get_height_at(float world_x, float world_z) const
{
    // Common::TerrainData 
    return _terrainData.GetHeightAt(world_x, world_z);
}