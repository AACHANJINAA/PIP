#pragma once
#include "Mesh.h"
#include "json.hpp"
#include "../../Common/TerrainData.h"

class TerrainLoader : public Mesh
{
public:
	struct TerrainInfo
	{
		DirectX::XMFLOAT4 bounds;    // x: min_x, y: max_x, z: min_z, w: max_z
		DirectX::XMFLOAT2 size;      // x: width, y: height
		float height_scale;          // JSON의 scale.y
		float min_height;            // 최소 높이 (정규화된 값)
		float padding[2];           // 8byte 패딩

		TerrainInfo()
			: bounds(0.0f, 0.0f, 0.0f, 0.0f)
			, size(0.0f, 0.0f)
			, height_scale(0.0f)
			, min_height(0.0f)
		{
		}
	};
public:
	TerrainLoader(const std::string& heightmap_json_path);
	virtual ~TerrainLoader() = default;

	void load_textures_to_resource_manager(const std::string& material_gltf_path);

	void render(ID3D12GraphicsCommandList* command_list) override;

	const TerrainInfo& get_terrain_info() const { return _terrainInfo; }

	float get_height_at(float world_x, float world_z) const;

	/// HeightMap 
	const std::string& get_heightmap_key() const { return _heightmapTextureKey; }

	/// Material 
	const std::string& get_material_name() const { return _materialName; }

private:
	void create_flat_grid(int grid_width, int grid_height);

private:
	std::string _heightmapTextureKey;  // HeightMap 
	std::string _materialName;           // Material 
	std::string _baseTextureKey;        // Base 
	std::string _detailTextureKey;      // Detail 

	TerrainInfo _terrainInfo;            // Terrain 

	common::TerrainData _terrainData;
};
