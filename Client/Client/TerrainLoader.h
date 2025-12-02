#pragma once
#include "Mesh.h"
#include "json.hpp"
#include "../../Common/TerrainData.h"

class TerrainLoader : public Mesh
{
public:
	using TerrainInfo = common::TerrainInfo;

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
	std::vector<float> _cpuHeightData;  // CPU

	common::TerrainData _terrainData;
};
