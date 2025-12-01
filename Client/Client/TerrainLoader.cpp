#include "stdafx.h"
#include "TerrainLoader.h"
#include "ResourceManager.h"
#include <fstream>
#include <filesystem>

using namespace DirectX;

TerrainLoader::TerrainLoader(const std::string& heightmap_json_path)
{
    // 1. JSON 파싱하여 Terrain 정보 로드
    parse_heightmap_json(heightmap_json_path);

    // 2. Flat Grid Mesh 생성 (높이는 Shader에서 처리)
    // JSON의 width가 504라면 503x503 Quad (504x504 정점)
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

    // Terrain 정보 추출
    m_terrain_info.bounds = DirectX::XMFLOAT4(
        config["bounds"]["min_x"].get<float>(),
        config["bounds"]["max_x"].get<float>(),
        config["bounds"]["min_z"].get<float>(),
        config["bounds"]["max_z"].get<float>()
    );

    m_terrain_info.size = DirectX::XMFLOAT2(
        static_cast<float>(config["width"].get<int>()),
        static_cast<float>(config["height"].get<int>())
    );

    m_terrain_info.height_scale = config["scale"]["y"].get<float>();
    m_terrain_info.padding = 0.0f;

    // HeightMap 파일 경로 생성 (JSON과 같은 폴더에 있음)
    std::filesystem::path json_dir = std::filesystem::path(json_path).parent_path();
    std::string heightmap_filename = config["heightmap_file"].get<std::string>();
    m_heightmap_texture_key = (json_dir / heightmap_filename).string();

    CLOG("Terrain Config Loaded:");
    CLOG("  Bounds: (" << m_terrain_info.bounds.x << ", " << m_terrain_info.bounds.y
        << "), (" << m_terrain_info.bounds.z << ", " << m_terrain_info.bounds.w << ")");
    CLOG("  Size: " << m_terrain_info.size.x << " x " << m_terrain_info.size.y);
    CLOG("  HeightMap: " << m_heightmap_texture_key);
}

void TerrainLoader::create_flat_grid(int grid_width, int grid_height)
{
    // 정점 개수: (grid_width + 1) * (grid_height + 1)
    int vertex_count_x = grid_width + 1;
    int vertex_count_z = grid_height + 1;

    // World Space 크기
    float world_width = m_terrain_info.bounds.y - m_terrain_info.bounds.x;
    float world_height = m_terrain_info.bounds.w - m_terrain_info.bounds.z;

    // 셀 크기 (World Space)
    float cell_width = world_width / grid_width;
    float cell_height = world_height / grid_height;

    // ========== 정점 생성 ==========
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
                0.0f,  // Shader에서 Displacement로 높이 결정
                m_terrain_info.bounds.z + z * cell_height
            );

            // Normal (기본값: 위쪽)
            v._normal = XMFLOAT3(0.0f, 1.0f, 0.0f);

            // UV 좌표 (0~1 범위로 정규화)
            v._texCoord = XMFLOAT2(
                static_cast<float>(x) / grid_width,
                static_cast<float>(z) / grid_height
            );

            // Tangent (나중에 Normal Mapping 시 필요)
            v._tangent = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);

            // Diffuse (흰색, Material 텍스처로 덮어씌워짐)
            v._diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

            vertices.emplace_back(v);
        }
    }

    // Mesh 클래스의 템플릿 함수로 정점 데이터 저장
    set_vertex_data_buffer(vertices);

    // ========== 인덱스 생성 ==========
    // 각 Quad를 2개의 삼각형으로 분할
    _indices.clear();
    _indices.reserve(grid_width * grid_height * 6);

    for (int z = 0; z < grid_height; ++z)
    {
        for (int x = 0; x < grid_width; ++x)
        {
            // 현재 Quad의 네 모서리 인덱스
            // v0(좌하) --- v1(우하)
            //   |      /      |
            // v2(좌상) --- v3(우상)

            int v0 = z * vertex_count_x + x;
            int v1 = v0 + 1;
            int v2 = (z + 1) * vertex_count_x + x;
            int v3 = v2 + 1;

            // 삼각형 1 (시계 반대 방향: v0 -> v2 -> v1)
            _indices.emplace_back(v0);
            _indices.emplace_back(v2);
            _indices.emplace_back(v1);

            // 삼각형 2 (시계 반대 방향: v1 -> v2 -> v3)
            _indices.emplace_back(v1);
            _indices.emplace_back(v2);
            _indices.emplace_back(v3);
        }
    }

    // ========== Topology 설정 ==========
    _primitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    // ========== Bounding Box 계산 ==========
    // Displacement 후 최대 높이를 고려하여 넉넉하게 설정
    // (실제로는 HeightMap의 최대/최소 높이를 분석해야 정확함)
    float max_height = m_terrain_info.height_scale;  // 예상 최대 높이

    _orientedBoundingBox = CreateOOBB(
        XMFLOAT3(m_terrain_info.bounds.x, -max_height * 0.1f, m_terrain_info.bounds.z),
        XMFLOAT3(m_terrain_info.bounds.y, max_height, m_terrain_info.bounds.w)
    );

    CLOG("Terrain Grid Created: " << vertex_count_x << " x " << vertex_count_z
        << " vertices, " << _indices.size() / 3 << " triangles");
}

void TerrainLoader::load_textures_to_resource_manager(const std::string& material_gltf_path)
{
    auto* rm = ResourceManager::instance();

    // 1. HeightMap 텍스처 로드 (R16 포맷)
    int width = static_cast<int>(m_terrain_info.size.x);
    int height = static_cast<int>(m_terrain_info.size.y);

    auto* heightmap_tex = rm->load_heightmap_from_raw(m_heightmap_texture_key, width, height);
    if (!heightmap_tex)
    {
        CERROR("Failed to load heightmap: " << m_heightmap_texture_key);
    }
    else
    {
        CLOG("HeightMap loaded: " << m_heightmap_texture_key);
    }

    // 2. Material 텍스처 로드 (glTF에서 가져옴)
    auto material_names = rm->load_materials_from_gltf(material_gltf_path);
    if (!material_names.empty())
    {
        m_material_name = material_names[0];  // 첫 번째 Material 사용
        CLOG("Material loaded: " << m_material_name);

        // Material에서 텍스처 경로 추출
        auto* mat_info = rm->get_material_info(m_material_name); // 이 함수는 추가 필요
        if (mat_info)
        {
            m_base_texture_key = mat_info->base_color_texture_path;
            m_detail_texture_key = mat_info->normal_texture_path; // Normal을 Detail로 사용

            CLOG("Base Texture: " << m_base_texture_key);
            CLOG("Detail Texture: " << m_detail_texture_key);
        }
    }
    else
    {
        CERROR("No materials found in: " << material_gltf_path);
    }
}

void TerrainLoader::render(ID3D12GraphicsCommandList* command_list)
{
    if (!is_uploaded())
    {
        CERROR("TerrainLoader: Mesh not uploaded to GPU yet!");
        return;
    }

    // 1. Vertex/Index Buffer 바인딩
    command_list->IASetVertexBuffers(0, 1, &_vertexBufferView);
    command_list->IASetIndexBuffer(&_indexBufferView);
    command_list->IASetPrimitiveTopology(_primitiveTopology);

    // 2. Terrain Constant Buffer 바인딩 (Root Parameter 2)
    command_list->SetGraphicsRoot32BitConstants(2,  8,  &m_terrain_info, 0);


    // 4. Draw Call
    command_list->DrawIndexedInstanced(
        static_cast<UINT>(_indices.size()),  // Index Count
        1,                                    // Instance Count
        0,                                    // Start Index
        0,                                    // Base Vertex
        0                                     // Start Instance
    );
}