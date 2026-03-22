#pragma once
#include "Mesh.h"
#include "../../Common/TerrainData.h"

struct LayerInfo
{
	std::string name;              // "Rock", "Grass", "Dead_Grass" 등
	std::string weightmap_file;    // "Weightmap_Rock.r8"

	// SharedTextures 경로 (로드 시 자동 매핑)
	std::string albedo_texture;   
	std::string normal_texture;   
	std::string roughness_texture;
};

class TerrainLoader : public Mesh
{
public:
	struct TerrainInfo
	{
		DirectX::XMFLOAT4 bounds;    // x: min_x, y: max_x, z: min_z, w: max_z
		DirectX::XMFLOAT2 size;      // x: width, y: height
		float height_scale;          // JSON의 scale.y
		float min_height;            // 최소 높이 (정규화된 값)
		XMFLOAT2 tiling;             // Texture Tiling Factor
		XMFLOAT2 detail_tiling;      // Detail Texture Tiling Factor
		float specular_factor = 0.5f;
		float padding = 0.0f;

		TerrainInfo()
			: bounds(0.0f, 0.0f, 0.0f, 0.0f)
			, size(0.0f, 0.0f)
			, height_scale(0.0f)
			, min_height(0.0f)
			, tiling(1.0f, 1.0f)
			, detail_tiling(1.0f, 1.0f)
		{
		}
	};
public:
	TerrainLoader(const std::string& heightmap_json_path);
	TerrainLoader(const std::string& metadata_json_path, bool is_landscape_tile);

	virtual ~TerrainLoader() = default;

	void load_textures_to_resource_manager(const std::string& material_gltf_path, const std::string& detail_texture_path);
	void load_landscape_weightmaps(const std::vector<std::string>& weightmap_paths);

	void render(ID3D12GraphicsCommandList* command_list) override;

	const TerrainInfo& get_terrain_info() const { return _terrainInfo; }

	float get_height_at(float world_x, float world_z) const;

	/// HeightMap 
	const std::string& get_heightmap_key() const { return _heightmapTextureKey; }

	/// Material 
	const std::string& get_material_name() const { return _materialName; }
	const std::string& get_base_texture_key() const { return _baseTextureKey; }
	const std::string& get_detail_texture_key() const { return _detailTextureKey; }
	const std::string& get_normal_texture_key() const { return _normalTextureKey; }

	//  Layer 시스템 Getter
	bool has_layers() const { return _hasLayers; }
	const std::vector<LayerInfo>& get_layers() const { return _layers; }
	const std::string& get_weightmap_array_key() const {return _weightmapArrayKey;}

	const std::string& get_albedo_array_key() const { return _albedoArrayKey; }
	const std::string& get_normal_array_key() const { return _normalArrayKey; }
	const std::string& get_roughness_array_key() const { return _roughnessArrayKey; }

private:
	void create_flat_grid(int grid_width, int grid_height);

private:
	std::string _heightmapTextureKey;		  // HeightMap 
	std::string _materialName;				  // Material 
	std::string _baseTextureKey;			  // Base 
	std::string _detailTextureKey;			  // Detail
	std::string _normalTextureKey;			  // Normal map
	std::string _metallicRoughnessTextureKey; // Metallic-Roughness map
	std::string _emissiveTextureKey; // Emissive map

	// multi landscape layer 시스템 관련
	std::vector<LayerInfo> _layers;           // 레이어 정보 (최대 8개)
	std::string _weightmapArrayKey;           // Texture2DArray 키(ResourceManager 캐시용)
	bool _hasLayers = false;                  // Layer 시스템 사용 여부

	std::string _albedoArrayKey;    // Albedo Texture2DArray 키
	std::string _normalArrayKey;    // Normal Texture2DArray 키
	std::string _roughnessArrayKey; // Roughness Texture2DArray 키

	TerrainInfo _terrainInfo;            // Terrain 
	common::TerrainData _terrainData;
};
