#include "stdafx.h"
#include "TerrainLoader.h"
#include "ResourceManager.h"
#include <fstream>
#include <filesystem>

using namespace DirectX;

TerrainLoader::TerrainLoader(const std::string& heightmap_json_path)
{
    // 1. Common::TerrainData를 통해 로드
    if (!m_terrainData.LoadFromJSON(heightmap_json_path))
    {
        CERROR("Failed to load terrain data from: " << heightmap_json_path);
        // 로드 실패 처리 (예외 발생 or 더미 데이터)
        // 일단 진행
    }

    const auto& info = m_terrainData.GetInfo();
    m_heightmap_texture_key = m_terrainData.GetHeightMapPath();

    // 2. Flat Grid Mesh 생성
    int grid_width = static_cast<int>(info.width) - 1;
    int grid_height = static_cast<int>(info.height) - 1;
    create_flat_grid(grid_width, grid_height);
}

void TerrainLoader::create_flat_grid(int grid_width, int grid_height)
{
    const auto& info = m_terrainData.GetInfo();

    int vertex_count_x = grid_width + 1;
    int vertex_count_z = grid_height + 1;

    // World Space 크기
    float world_width = info.max_x - info.min_x;
    float world_height = info.max_z - info.min_z;

    // 셀 크기 (World Space)
    float cell_width = world_width / grid_width;
    float cell_height = world_height / grid_height;

    // 정점 생성
    std::vector<IlluminatedVertex> vertices;
    vertices.reserve(vertex_count_x * vertex_count_z);

    for (int z = 0; z < vertex_count_z; ++z)
    {
        for (int x = 0; x < vertex_count_x; ++x)
        {
            IlluminatedVertex v;

            // 위치 (World Space, Y=0)
            v._position = XMFLOAT3(
                info.min_x + x * cell_width,
                0.0f,  // Shader에서 Displacement로 높이 적용
                info.min_z + z * cell_height
            );

            // Normal (기본값: 위쪽)
            v._normal = XMFLOAT3(0.0f, 1.0f, 0.0f);

            // UV 좌표 (0~1 범위로 정규화)
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

    // 인덱스 생성
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

            // 삼각형 1
            _indices.emplace_back(v0);
            _indices.emplace_back(v2);
            _indices.emplace_back(v1);

            // 삼각형 2
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
    const auto& info = m_terrainData.GetInfo();
    auto* rm = ResourceManager::instance();

    // 1. HeightMap 텍스처 로드 (R16 포맷)
    int width = static_cast<int>(info.width);
    int height = static_cast<int>(info.height);

    auto* heightmap_tex = rm->load_heightmap_from_raw(m_heightmap_texture_key, width, height);
    if (!heightmap_tex)
    {
        CERROR("Failed to load heightmap texture: " << m_heightmap_texture_key);
    }
    else
    {
        CLOG("HeightMap texture loaded to GPU: " << m_heightmap_texture_key);
    }

    // 2. glTF Material 로드 (ResourceManager의 함수 사용)
    auto material_names = rm->load_materials_from_gltf(material_gltf_path);
    if (!material_names.empty())
    {
        m_material_name = material_names[0];
        CLOG("Material loaded from glTF: " << m_material_name);

        // Material 정보 가져오기
        auto* mat_info = rm->get_material_info(m_material_name);
        if (mat_info)
        {
            // Base Color Texture (T_ground_Moss_D.png)
            if (!mat_info->base_color_texture_path.empty())
            {
                m_base_texture_key = mat_info->base_color_texture_path;
                CLOG("  Base Texture: " << m_base_texture_key);
            }

            // Normal Texture는 Detail로 사용 (T_Ground_Moss_N.png)
            if (!mat_info->normal_texture_path.empty())
            {
                m_detail_texture_key = mat_info->normal_texture_path;
                CLOG("  Detail Texture: " << m_detail_texture_key);
            }
        }
        else
        {
            CERROR("Failed to get material info for: " << m_material_name);
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

    // Vertex/Index Buffer 바인딩
    command_list->IASetVertexBuffers(0, 1, &_vertexBufferView);
    command_list->IASetIndexBuffer(&_indexBufferView);
    command_list->IASetPrimitiveTopology(_primitiveTopology);

    // Terrain Constant Buffer 업데이트 (Root Parameter 2)
    // TerrainInfo 구조체 전체를 32bit constants로 전달
    // 주의: Common::TerrainInfo와 셰이더 CB 구조가 일치해야 함 (float 7개)
    // struct TerrainInfo { float min_x, max_x, min_z, max_z, width, height, height_scale, min_height; };
    // 셰이더는 float4 bounds, float2 size, float height_scale, float min_height
    // 셰이더 호환성을 위해 임시 구조체 생성
    struct ShaderTerrainInfo
    {
        XMFLOAT4 bounds;
        XMFLOAT2 size;
        float height_scale;
        float min_height;
    };

    const auto& info = m_terrainData.GetInfo();
    ShaderTerrainInfo cb_info;
    cb_info.bounds = XMFLOAT4(info.min_x, info.max_x, info.min_z, info.max_z);
    cb_info.size = XMFLOAT2(info.width, info.height);
    cb_info.height_scale = info.height_scale;
    cb_info.min_height = info.min_height;

    command_list->SetGraphicsRoot32BitConstants(2, 8, &cb_info, 0);

    // Draw Call
    command_list->DrawIndexedInstanced(
        static_cast<UINT>(_indices.size()), 1, 0, 0, 0
    );
}

float TerrainLoader::get_height_at(float world_x, float world_z) const
{
    // Common::TerrainData 위임
    return m_terrainData.GetHeightAt(world_x, world_z);
}