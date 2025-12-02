#include "stdafx.h"
#include "TerrainLoader.h"
#include "ResourceManager.h"
#include <fstream>
#include <filesystem>

using namespace DirectX;

TerrainLoader::TerrainLoader(const std::string& heightmap_json_path)
    : m_raw_min_height(0.0f)
    , m_raw_max_height(0.0f)
{
    // 1. JSON 파싱하여 Terrain 정보 로드
    parse_heightmap_json(heightmap_json_path);

    // 2. Flat Grid Mesh 생성
    int grid_width = static_cast<int>(m_terrain_info.size.x) - 1;
    int grid_height = static_cast<int>(m_terrain_info.size.y) - 1;
    create_flat_grid(grid_width, grid_height);
}

void TerrainLoader::parse_heightmap_json(const std::string& json_path)
{
    // JSON 파일 열기
    std::ifstream file(json_path);
    if (!file.is_open())
    {
        CERROR("Failed to open heightmap JSON: " << json_path);
        return;
    }

    // JSON 파싱
    nlohmann::json config;
    file >> config;
    file.close();

    // 1. Bounds 정보 (맵 경계)
    m_terrain_info.bounds = XMFLOAT4(
        config["bounds"]["min_x"].get<float>(),
        config["bounds"]["max_x"].get<float>(),
        config["bounds"]["min_z"].get<float>(),
        config["bounds"]["max_z"].get<float>()
    );

    // 2. Size 정보 (너비, 높이)
    m_terrain_info.size = XMFLOAT2(
        static_cast<float>(config["width"].get<int>()),
        static_cast<float>(config["height"].get<int>())
    );

    // 3. Scale 정보 (Y축 스케일)
    m_terrain_info.height_scale = config["scale"]["y"].get<float>();

    // 4. HeightMap 파일 경로 생성
    std::filesystem::path json_dir = std::filesystem::path(json_path).parent_path();
    std::string heightmap_filename = config["heightmap_file"].get<std::string>();
    m_heightmap_texture_key = (json_dir / heightmap_filename).string();

    // 5. HeightMap 데이터를 CPU 메모리에 로드 (충돌 계산용)
    std::ifstream hm_file(m_heightmap_texture_key, std::ios::binary);
    if (!hm_file.is_open())
    {
        CERROR("Failed to open heightmap file: " << m_heightmap_texture_key);
        return;
    }

    int width = static_cast<int>(m_terrain_info.size.x);
    int height = static_cast<int>(m_terrain_info.size.y);
    size_t total_pixels = width * height;

    m_cpu_height_data.resize(total_pixels);

    uint16_t min_val = 65535;
    uint16_t max_val = 0;

    // R16 파일 읽기 (16bit unsigned per pixel)
    for (size_t i = 0; i < total_pixels; ++i)
    {
        uint16_t raw_height;
        if (!hm_file.read(reinterpret_cast<char*>(&raw_height), sizeof(uint16_t)))
        {
            CERROR("Failed to read heightmap at index: " << i);
            return;
        }

        CINFO("TerrainInfo loaded:");
        CINFO("  bounds: (" << m_terrain_info.bounds.x << ", " << m_terrain_info.bounds.y
            << ", " << m_terrain_info.bounds.z << ", " << m_terrain_info.bounds.w << ")");
        CINFO("  size: (" << m_terrain_info.size.x << ", " << m_terrain_info.size.y << ")");
        CINFO("  height_scale: " << m_terrain_info.height_scale);
        CINFO("  min_height: " << m_terrain_info.min_height);

        min_val = min(min_val, raw_height);
        max_val = max(max_val, raw_height);

        // 0~1로 정규화
        float normalized = static_cast<float>(raw_height) / 65535.0f;
        m_cpu_height_data[i] = normalized * m_terrain_info.height_scale;
    }

    hm_file.close();

    // 최소값을 저장 (셰이더에서 사용)
    m_raw_min_height = static_cast<float>(min_val) / 65535.0f;
    m_raw_max_height = static_cast<float>(max_val) / 65535.0f;
    m_terrain_info.min_height = m_raw_min_height;
}

void TerrainLoader::create_flat_grid(int grid_width, int grid_height)
{
    int vertex_count_x = grid_width + 1;
    int vertex_count_z = grid_height + 1;

    // World Space 크기
    float world_width = m_terrain_info.bounds.y - m_terrain_info.bounds.x;
    float world_height = m_terrain_info.bounds.w - m_terrain_info.bounds.z;

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
                m_terrain_info.bounds.x + x * cell_width,
                0.0f,  // Shader에서 Displacement로 높이 적용
                m_terrain_info.bounds.z + z * cell_height
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
    float max_height = m_terrain_info.height_scale;
    _orientedBoundingBox = CreateOOBB(
        XMFLOAT3(m_terrain_info.bounds.x, 0.0f, m_terrain_info.bounds.z),
        XMFLOAT3(m_terrain_info.bounds.y, max_height, m_terrain_info.bounds.w)
    );

    CLOG("Terrain Grid Created: " << vertex_count_x << " x " << vertex_count_z
        << " vertices, " << _indices.size() / 3 << " triangles");
}

void TerrainLoader::load_textures_to_resource_manager(const std::string& material_gltf_path)
{
    auto* rm = ResourceManager::instance();

    // 1. HeightMap 텍스처 로드 (R16 형식)
    int width = static_cast<int>(m_terrain_info.size.x);
    int height = static_cast<int>(m_terrain_info.size.y);

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

            // Normal Texture를 Detail로 사용 (T_Ground_Moss_N.png)
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

    // Terrain Constant Buffer 바인딩 (Root Parameter 2)
    // TerrainInfo 구조체 전체를 32bit constants로 전송
    command_list->SetGraphicsRoot32BitConstants(2, 12, &m_terrain_info, 0);

    // Draw Call
    command_list->DrawIndexedInstanced(
        static_cast<UINT>(_indices.size()), 1, 0, 0, 0
    );
}

float TerrainLoader::get_height_at(float world_x, float world_z) const
{
    // 범위 체크
    if (world_x < m_terrain_info.bounds.x || world_x > m_terrain_info.bounds.y ||
        world_z < m_terrain_info.bounds.z || world_z > m_terrain_info.bounds.w)
    {
        return 0.0f;
    }

    if (m_cpu_height_data.empty())
    {
        CERROR("CPU height data not loaded!");
        return 0.0f;
    }

    // 정규화 (0~1 범위)
    float norm_x = (world_x - m_terrain_info.bounds.x) /
        (m_terrain_info.bounds.y - m_terrain_info.bounds.x);
    float norm_z = (world_z - m_terrain_info.bounds.z) /
        (m_terrain_info.bounds.w - m_terrain_info.bounds.z);

    // 그리드 좌표로 변환
    float grid_x = norm_x * (m_terrain_info.size.x - 1);
    float grid_z = norm_z * (m_terrain_info.size.y - 1);

    // 정수/소수 부분 분리
    int x0 = std::clamp(static_cast<int>(std::floor(grid_x)), 0, static_cast<int>(m_terrain_info.size.x) - 1);
    int z0 = std::clamp(static_cast<int>(std::floor(grid_z)), 0, static_cast<int>(m_terrain_info.size.y) - 1);
    int x1 = min(x0 + 1, static_cast<int>(m_terrain_info.size.x) - 1);
    int z1 = min(z0 + 1, static_cast<int>(m_terrain_info.size.y) - 1);

    float fx = grid_x - x0;
    float fz = grid_z - z0;

    int width = static_cast<int>(m_terrain_info.size.x);

    // 높이값 가져오기
    float h00 = m_cpu_height_data[z0 * width + x0];
    float h10 = m_cpu_height_data[z0 * width + x1];
    float h01 = m_cpu_height_data[z1 * width + x0];
    float h11 = m_cpu_height_data[z1 * width + x1];

    // 이중 선형 보간
    float h0 = h00 * (1.0f - fx) + h10 * fx;
    float h1 = h01 * (1.0f - fx) + h11 * fx;

    return h0 * (1.0f - fz) + h1 * fz;
}
