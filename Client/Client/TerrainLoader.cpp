#include "stdafx.h"
#include "TerrainLoader.h"

#include "PhysicsManager.h"
#include "ResourceManager.h"
#include "Renderer.h"

TerrainLoader::TerrainLoader(const std::string& heightmap_json_path)
{
	
	if (!_terrainData.LoadFromJSON(heightmap_json_path, true))
	{
		CERROR("Failed to load terrain data from: " << heightmap_json_path);
	}

	const auto& info = _terrainData.GetInfo();
	_heightmapTextureKey = _terrainData.GetHeightMapPath();

	// _terrainInfo 
	_terrainInfo.bounds = XMFLOAT4(info.min_x, info.max_x, info.min_z, info.max_z);
	_terrainInfo.size = XMFLOAT2(info.width, info.height);
	_terrainInfo.height_scale = info.height_scale;
	_terrainInfo.min_height = info.min_height;
	_terrainInfo.tiling = XMFLOAT2(16.0f, 16.0f);
	_terrainInfo.detail_tiling = XMFLOAT2(128.0f, 128.0f);

	// 2. Flat Grid Mesh 
	int grid_width = static_cast<int>(info.width) - 1;
	int grid_height = static_cast<int>(info.height) - 1;
	create_flat_grid(grid_width, grid_height);

	PhysicsManager::instance()->create_physics_terrain(_terrainData);
}

TerrainLoader::TerrainLoader(const std::string& metadata_json_path, bool is_landscape_tile)
{

	// 1. JSON 파싱 (TerrainData::LoadFromJSON이 이미 grid_info 지원함)
	if (!_terrainData.LoadFromJSON(metadata_json_path, false))
	{
		CERROR("Failed to load landscape metadata from: " << metadata_json_path);
		return;
	}

	const auto& info = _terrainData.GetInfo();
	_heightmapTextureKey = _terrainData.GetHeightMapPath();

	// 2. TerrainInfo 설정
	_terrainInfo.bounds = XMFLOAT4(info.min_x, info.max_x, info.min_z, info.max_z);
	_terrainInfo.size = XMFLOAT2(info.width, info.height);
	_terrainInfo.height_scale = info.height_scale;
	_terrainInfo.min_height = info.min_height;
	_terrainInfo.tiling = XMFLOAT2(32.0f, 32.0f);      // 기존 16보다 2배
	_terrainInfo.detail_tiling = XMFLOAT2(256.0f, 256.0f); // 기존 128보다 2배

	// 3. Grid 메쉬 생성
	int grid_width = static_cast<int>(info.width) - 1;
	int grid_height = static_cast<int>(info.height) - 1;
	create_flat_grid(grid_width, grid_height);
	CLOG("Terrain Grid Created: " << info.width << " x " << info.height
		<< " vertices, " << (_indices.size() / 3) << " triangles")

	// MainLandscape는 Jolt 물리 지형 일단 제외
	//PhysicsManager::instance()->create_physics_terrain(_terrainData);
}

namespace
{
 DirectX::XMFLOAT3 get_normal_at(const common::TerrainData & terrainData, int x, int z)
	 {
		 const auto& info = terrainData.GetInfo();
		 int width = static_cast<int>(info.width);
		 int length = static_cast<int>(info.height);

		 if (x < 0 || z < 0 || x >= width || z >= length)
		 {
			 return DirectX::XMFLOAT3(0.0f, 1.0f, 0.0f);
		 }

		 int x_plus_1 = std::min(x + 1, width - 1);
		 int x_minus_1 = std::max(x - 1, 0);
		 int z_plus_1 = std::min(z + 1, length - 1);
		 int z_minus_1 = std::max(z - 1, 0);

		 float height_y1 = terrainData.GetHeightAt(info.min_x + x * (info.max_x - info.min_x) / (width - 1), info.min_z + z_minus_1 * (info.max_z - info.min_z) / (length - 1));
		 float height_y2 = terrainData.GetHeightAt(info.min_x + x * (info.max_x - info.min_x) / (width - 1), info.min_z + z_plus_1 * (info.max_z - info.min_z) / (length - 1));
		 float height_x1 = terrainData.GetHeightAt(info.min_x + x_minus_1 * (info.max_x - info.min_x) / (width - 1), info.min_z + z * (info.max_z - info.min_z) / (length - 1));
		 float height_x2 = terrainData.GetHeightAt(info.min_x + x_plus_1 * (info.max_x - info.min_x) / (width - 1), info.min_z + z * (info.max_z - info.min_z) / (length - 1));
 
		 DirectX::XMFLOAT3 edge1(0.0f, height_y2 - height_y1, 2.0f * (info.max_z - info.min_z) / (length - 1));
		 DirectX::XMFLOAT3 edge2(2.0f * (info.max_x - info.min_x) / (width - 1), height_x2 - height_x1, 0.0f);
		 
		 DirectX::XMVECTOR normal = XMVector3Cross(DirectX::XMLoadFloat3(&edge1), DirectX::XMLoadFloat3(&edge2));
		 normal = DirectX::XMVector3Normalize(normal);

		 DirectX::XMFLOAT3 result;
		 DirectX::XMStoreFloat3(&result, normal);

		 return result;
	}
}


void TerrainLoader::create_flat_grid(int grid_width, int grid_height)
{
const auto& info = _terrainData.GetInfo();

	int vertex_count_x = grid_width + 1;
	int vertex_count_z = grid_height + 1;

	float world_width = info.max_x - info.min_x;
	float world_height = info.max_z - info.min_z;

	float cell_width = world_width / grid_width;
	float cell_height = world_height / grid_height;

	std::vector<IlluminatedVertex> vertices;
	vertices.reserve(vertex_count_x * vertex_count_z);

	for (int z = 0; z < vertex_count_z; ++z)
	{
		for (int x = 0; x < vertex_count_x; ++x)
		 {
			IlluminatedVertex v;
			float world_x_pos = info.min_x + x * cell_width;
			float world_z_pos = info.min_z + z * cell_height;

			 float height = _terrainData.GetHeightAt(world_x_pos, world_z_pos);

			 v._position = XMFLOAT3(world_x_pos, height, world_z_pos);

			 v._normal = get_normal_at(_terrainData, x, z);

			 v._texCoord = XMFLOAT2(
				 static_cast<float>(x) / grid_width,
				 static_cast<float>(z) / grid_height);

			 XMFLOAT3 tangent_candidate = XMFLOAT3(1.0f, 0.0f, 0.0f);
			 XMVECTOR N_vec = XMLoadFloat3(&v._normal);
			 XMVECTOR T_candidate_vec = XMLoadFloat3(&tangent_candidate);  
			XMVECTOR dot_product_vec = XMVector3Dot(N_vec, T_candidate_vec);
			float dot_product = XMVectorGetX(dot_product_vec);

			 XMVECTOR T_vec = XMVector3Normalize(
				 XMVectorSubtract(
					 T_candidate_vec,
					 XMVectorScale(N_vec, dot_product))
			);
			 XMStoreFloat3(&v._tangent, T_vec);

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

	 float max_height = info.height_scale;
	_orientedBoundingBox = CreateOOBB(
		 XMFLOAT3(info.min_x, 0.0f, info.min_z),
		 XMFLOAT3(info.max_x, max_height, info.max_z)
	);

	 CLOG("Terrain Grid Created: " << vertex_count_x << " x " << vertex_count_z << " vertices, " << _indices.size() / 3 << " triangles");
}


void TerrainLoader::load_textures_to_resource_manager(const std::string& material_gltf_path, const std::string& detail_texture_path)
{
	const auto& info = _terrainData.GetInfo();
	auto* rm = ResourceManager::instance();

	// 1. HeightMap (기존 코드 유지)
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

	// 2. glTF Material
	auto material_names = rm->load_materials_from_gltf(material_gltf_path);
	if (!material_names.empty())
	{
		_materialName = material_names[0];
		CLOG("Material loaded from glTF: " << _materialName);
		// Individual texture keys are no longer stored in TerrainLoader,
		// as ResourceManager handles binding them based on _materialName.
	}
	else
	{
		CERROR("No materials found in glTF: " << material_gltf_path);
	}

	// 3. Detail Texture 로드
	if (!detail_texture_path.empty())
	{
		_detailTextureKey = detail_texture_path;
		auto* detail_tex = rm->load_texture(_detailTextureKey, false);
		if (!detail_tex)
		{
			CERROR("Failed to load detail texture: " << _detailTextureKey);
		}
		else
		{
			CLOG("Detail texture loaded to GPU: " << _detailTextureKey);
		}
	}
	else
	{
		// 디테일 텍스처 경로가 없으면 기본 흰색 텍스처 사용
		_detailTextureKey = "__DEFAULT_WHITE__";
	}
}

void TerrainLoader::load_landscape_weightmaps(const std::vector<std::string>& weightmap_paths)
{
	// TODO: R8 포맷 텍스처 배열 생성 및 GPU 업로드
	   // 각 weightmap_paths를 순회하며:
	   // 1. R8 바이너리 파일 읽기
	   // 2. D3D12 텍스처 리소스 생성 (DXGI_FORMAT_R8_UNORM)
	   // 3. ResourceManager에 "Weightmap_Rock", "Weightmap_Grass" 등으로 등록

	if (weightmap_paths.empty())
	{
		CERROR("No weightmap paths provided.");
		return;
	}
	
	// 1. LayerInfo 구조체 채우기
	_layers.clear();
	_layers.reserve(weightmap_paths.size());

	std::string sharedTexpath = "Resource/MainLandscape/SharedTextures/";

	for (const auto& weightmap_path : weightmap_paths)
	{
		// 파일명에서 레이어 이름 추출
		std::filesystem::path wpath(weightmap_path);
		std::string filename = wpath.stem().string(); // "Weightmap_Rock"

		// "Weightmap_" 접두사 제거하여 레이어 이름 추출
		std::string layer_name = filename;
		if (layer_name.find("Weightmap_") == 0)
		{
			layer_name = layer_name.substr(10); // "Weightmap_" 길이 = 10
		}

		// Visibility 레이어는 스킵 (렌더링에 사용 안 함)
		if (layer_name.find("LANDSCAPE_VISIBILITY") !=
			std::string::npos)
		{
			CLOG("Skipping visibility layer: " << layer_name);
			continue;
		}

		LayerInfo layer;
		layer.name = layer_name;
		layer.weightmap_file = weightmap_path;

		// SharedTextures에서 해당 레이어의 텍스처 경로 매핑
		std::string base_name = "T_" + layer_name;

		layer.albedo_texture = (sharedTexpath + base_name + "_Albedo.dds");
		layer.normal_texture = (sharedTexpath + base_name + "_Normal.dds");
		layer.roughness_texture = (sharedTexpath + base_name + "_Roughness.dds");

		_layers.emplace_back(layer);
	}

	if (_layers.empty())
	{
		CLOG("No valid layers found after filtering");
		return;
	}

	// 2. Weightmap Texture2DArray 생성
	const auto& info = _terrainData.GetInfo();
	int width = static_cast<int>(info.width);
	int height = static_cast<int>(info.height);

	std::vector<std::string> weightmap_file_paths;
	for (const auto& layer : _layers)
	{
		weightmap_file_paths.push_back(layer.weightmap_file);
	}

	// 고유한 배열 이름 생성 (Landscape 이름 기반)
	std::filesystem::path map_path(_heightmapTextureKey);
	std::string landscape_name = map_path.parent_path().filename().string(); // "Landscape01"
	_weightmapArrayKey = "WeightmapArray_" + landscape_name;

	auto* rm = ResourceManager::instance();
	auto* weightmap_array = rm->create_texture_array_r8(
		_weightmapArrayKey,
		weightmap_file_paths,
		width,
		height
	);

	if (!weightmap_array)
	{
		CERROR("Failed to create weightmap array: " <<
			_weightmapArrayKey);
		return;
	}

	// 3. 각 레이어의 텍스처 로드 (Albedo, Normal, Roughness)
	for (const auto& layer : _layers)
	{
		// Albedo (sRGB)
		rm->load_texture(layer.albedo_texture, true);

		// Normal (Linear)
		rm->load_texture(layer.normal_texture, false);

		// Roughness (Linear)
		rm->load_texture(layer.roughness_texture, false);
	}

	// 4. 레이어 텍스처들을 Texture2DArray로 묶기
	std::vector<std::string> albedo_keys, normal_keys, roughness_keys;
	for (const auto& layer : _layers)
	{
		albedo_keys.push_back(layer.albedo_texture);
		normal_keys.push_back(layer.normal_texture);
		roughness_keys.push_back(layer.roughness_texture);
	}

	_albedoArrayKey = "AlbedoArray_" + landscape_name;
	_normalArrayKey = "NormalArray_" + landscape_name;
	_roughnessArrayKey = "RoughnessArray_" + landscape_name;

	auto* albedo_array = rm->create_texture_array_from_loaded(
		_albedoArrayKey, albedo_keys);

	auto* normal_array = rm->create_texture_array_from_loaded(
		_normalArrayKey, normal_keys);

	auto* roughness_array = rm->create_texture_array_from_loaded(
		_roughnessArrayKey, roughness_keys);

	if (!albedo_array || !normal_array || !roughness_array)
	{
		CERROR("Failed to create layer texture arrays for: " << landscape_name);
		return;
	}

	_hasLayers = true;
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