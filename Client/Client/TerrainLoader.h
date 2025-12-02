#pragma once
#include "Mesh.h"
#include "json.hpp"
#include "../../Common/TerrainData.h"

/// Terrain �������� ���� Grid Mesh ���� �� ���� Ŭ����
/// - JSON ���Ͽ��� Terrain ���� �Ľ�
/// - ResourceManager�� ���� HeightMap + Material �ؽ�ó �ε�
/// - Flat Grid Mesh ���� (Displacement�� Shader���� ó��)

class TerrainLoader : public Mesh
{
public:
    // TerrainInfo는 Common::TerrainInfo를 사용하므로 별도 구조체 정의 제거 가능
    // 하지만 기존 코드 호환성을 위해 using 사용하거나 기존 구조체 유지 후 변환
    using TerrainInfo = common::TerrainInfo;

public:
    TerrainLoader(const std::string& heightmap_json_path);
    virtual ~TerrainLoader() = default;

    /// ResourceManager�� HeightMap�� Material �ؽ�ó �ε� ��û
    void load_textures_to_resource_manager(const std::string& material_gltf_path);

    /// Terrain ������
    void render(ID3D12GraphicsCommandList* command_list) override;

    /// Terrain ���� ��ȯ
    const TerrainInfo& get_terrain_info() const { return m_terrain_info; }

    /// Ư�� ���� ��ǥ(x, z)������ ���� ���� ���
    float get_height_at(float world_x, float world_z) const;

    /// HeightMap �ؽ�ó Ű ��ȯ
    const std::string& get_heightmap_key() const { return m_heightmap_texture_key; }

    /// Material �̸� ��ȯ
    const std::string& get_material_name() const { return m_material_name; }

private:
    void create_flat_grid(int grid_width, int grid_height);

private:
    std::string m_heightmap_texture_key;  // HeightMap �ؽ�ó Ű
    std::string m_material_name;           // Material �̸�
    std::string m_base_texture_key;        // Base �ؽ�ó Ű
    std::string m_detail_texture_key;      // Detail �ؽ�ó Ű

    TerrainInfo m_terrain_info;            // Terrain ����
    std::vector<float> m_cpu_height_data;  // CPU���� ���� ��ȸ��

    // [����] Common::TerrainData ���
    common::TerrainData m_terrainData;
};
