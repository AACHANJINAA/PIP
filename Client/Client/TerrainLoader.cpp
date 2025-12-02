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

void TerrainLoader::create_flat_grid(int grid_width, int grid_height)
{
    const auto& info = _terrainData.GetInfo();

    int vertex_count_x = grid_width + 1;
    int vertex_count_z = grid_height + 1;

    // World Space 
    float world_width = info.max_x - info.min_x;
    float world_height = info.max_z - info.min_z;

    //(World Space)
    float cell_width = world_width / grid_width;
    float cell_height = world_height / grid_height;

    std::vector<IlluminatedVertex> vertices;
    vertices.reserve(vertex_count_x * vertex_count_z);

    for (int z = 0; z < vertex_count_z; ++z)
    {
        for (int x = 0; x < vertex_count_x; ++x)
        {
            IlluminatedVertex v;

           
            v._position = XMFLOAT3(
                info.min_x + x * cell_width,
                0.0f,  
                info.min_z + z * cell_height
            );

           
            v._normal = XMFLOAT3(0.0f, 1.0f, 0.0f);

            // UV 
            v._texCoord = XMFLOAT2(
                static_cast<float>(x) / grid_width,
                static_cast<float>(z) / grid_height
            );

            // Tangent
            v._tangent = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);

            // Diffuse
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

    // Bounding Box
    float max_height = info.height_scale;
    _orientedBoundingBox = CreateOOBB(
        XMFLOAT3(info.min_x, 0.0f, info.min_z),
        XMFLOAT3(info.max_x, max_height, info.max_z)
    );

    CLOG("Terrain Grid Created: " << vertex_count_x << " x " << vertex_count_z
        << " vertices, " << _indices.size() / 3 << " triangles");
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
    command_list->SetGraphicsRoot32BitConstants(2, 8, &_terrainInfo, 0);

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