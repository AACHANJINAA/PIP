#pragma once
#include "Mesh.h"
#include "json.hpp"

/// Terrain 렌더링을 위한 Grid Mesh 생성 및 관리 클래스
/// - JSON 파일에서 Terrain 설정 파싱
/// - ResourceManager를 통한 HeightMap + Material 텍스처 로딩
/// - Flat Grid Mesh 생성 (Displacement는 Shader에서 처리)

class TerrainLoader : public Mesh
{
public:
    struct TerrainInfo
    {
        DirectX::XMFLOAT4 bounds;    // x: min_x, y: max_x, z: min_z, w: max_z
        DirectX::XMFLOAT2 size;      // x: width, y: height
        float height_scale;          // JSON의 scale.y
        float padding;

        // 기본 생성자
        TerrainInfo()
            : bounds(0.0f, 0.0f, 0.0f, 0.0f)
            , size(0.0f, 0.0f)
            , height_scale(0.0f)
            , padding(0.0f)
        {
        }
    };

public:
    /// <param name="heightmap_json_path">Heightmap.json 파일 경로
    TerrainLoader(const std::string& heightmap_json_path);
    virtual ~TerrainLoader() = default;

    /// ResourceManager에 HeightMap과 Material 텍스처 로딩 요청
    void load_textures_to_resource_manager(const std::string& material_gltf_path);

    /// Terrain 렌더링 (VB/IB + 텍스처 바인딩)
    void render(ID3D12GraphicsCommandList* command_list) override;

    /// Terrain 정보 Constant Buffer 얻기
    const struct TerrainInfo& get_terrain_info() const { return m_terrain_info; }

    /// HeightMap 텍스처 키 얻기
    const std::string& get_heightmap_key() const { return m_heightmap_texture_key; }

    /// Material 이름 얻기
    const std::string& get_material_name() const { return m_material_name; }

private:
    /// Heightmap.json 파싱
    void parse_heightmap_json(const std::string& json_path);

    /// Flat Grid Mesh 생성 (Y=0, UV 좌표 포함)
    void create_flat_grid(int grid_width, int grid_height);

 
private:

    /// HeightMap 텍스처 키 (ResourceManager에서 찾을 키)
    std::string m_heightmap_texture_key;

    /// Material 이름 (ResourceManager의 bind_material에 사용)
    std::string m_material_name;

    // Material 텍스처 키들
    std::string m_base_texture_key;
    std::string m_detail_texture_key;

    // Terrain 범위 정보
    TerrainInfo m_terrain_info;
};