#pragma once
#include "Mesh.h"
#include "json.hpp"
#include "../../Common/TerrainData.h"

/// Terrain 렌더링을 위한 Grid Mesh 생성 및 관리 클래스
/// - JSON 파일에서 Terrain 설정 파싱
/// - ResourceManager를 통한 HeightMap + Material 텍스처 로딩
/// - Flat Grid Mesh 생성 (Displacement는 Shader에서 처리)

class TerrainLoader : public Mesh
{
public:
    // TerrainInfo는 Common::TerrainInfo를 사용하므로 별도 구조체 정의 제거 가능
    // 하지만 기존 코드 호환성을 위해 using 사용하거나 기존 구조체 유지 후 변환
    using TerrainInfo = common::TerrainInfo;

public:
    TerrainLoader(const std::string& heightmap_json_path);
    virtual ~TerrainLoader() = default;

    /// ResourceManager에 HeightMap과 Material 텍스처 로드 요청
    void load_textures_to_resource_manager(const std::string& material_gltf_path);

    /// Terrain 렌더링
    void render(ID3D12GraphicsCommandList* command_list) override;

    /// Terrain 정보 반환
    const TerrainInfo& get_terrain_info() const { return m_terrain_info; }

    /// 특정 월드 좌표(x, z)에서의 지형 높이 계산
    float get_height_at(float world_x, float world_z) const;

    /// HeightMap 텍스처 키 반환
    const std::string& get_heightmap_key() const { return m_heightmap_texture_key; }

    /// Material 이름 반환
    const std::string& get_material_name() const { return m_material_name; }

private:
    void create_flat_grid(int grid_width, int grid_height);

private:
    std::string m_heightmap_texture_key;  // HeightMap 텍스처 키
    std::string m_material_name;           // Material 이름
    std::string m_base_texture_key;        // Base 텍스처 키
    std::string m_detail_texture_key;      // Detail 텍스처 키

    TerrainInfo m_terrain_info;            // Terrain 정보
    std::vector<float> m_cpu_height_data;  // CPU에서 높이 조회용

    // [수정] Common::TerrainData 사용
    common::TerrainData m_terrainData;
};
