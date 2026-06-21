#include "stdafx.h"
#include "TerrainLoader.h"

#include "FoliageRenderComponent.h"
#include "ObjectManager.h"
#include "PhysicsManager.h"
#include "ResourceManager.h"

std::vector<TerrainLoader*> TerrainLoader::_all_terrain_loaders;
TerrainLoader::TerrainLoader(const std::string& heightmap_json_path)
{
	
	if (!_terrainData.LoadFromJSON(heightmap_json_path, false))
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

	//PhysicsManager::instance()->create_physics_terrain(_terrainData);

	_all_terrain_loaders.push_back(this);
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
	_all_terrain_loaders.push_back(this);
}

TerrainLoader::~TerrainLoader()
{
	auto it = std::find(_all_terrain_loaders.begin(), _all_terrain_loaders.end(), this);
	if (it != _all_terrain_loaders.end()) {
		_all_terrain_loaders.erase(it);
	}
}

float TerrainLoader::get_height_anywhere(float world_x, float world_z)
{
	for (auto loader : _all_terrain_loaders) {
		// TerrainData 내부에서 범위 체크를 수행하여 해당 타일이면 높이를 반환합니다.
		// 범위 밖이면 0(또는 하한값)을 반환하므로 유효한 값을 찾을 때까지 순회합니다.
		const auto& info = loader->_terrainData.GetInfo();
		if (world_x >= info.min_x && world_x <= info.max_x &&
			world_z >= info.min_z && world_z <= info.max_z) {
			return loader->get_height_at(world_x, world_z);
		}
	}
	return -1.0f; // 지형이 없는 곳의 기본 높이
}

namespace
{
 XMFLOAT3 get_normal_at(const common::TerrainData & terrainData, int x, int z)
	 {
		 const auto& info = terrainData.GetInfo();
		 int width = static_cast<int>(info.width);
		 int length = static_cast<int>(info.height);

		 if (x < 0 || z < 0 || x >= width || z >= length)
		 {
			 return XMFLOAT3(0.0f, 1.0f, 0.0f);
		 }

		 int x_plus_1 = std::min(x + 1, width - 1);
		 int x_minus_1 = std::max(x - 1, 0);
		 int z_plus_1 = std::min(z + 1, length - 1);
		 int z_minus_1 = std::max(z - 1, 0);

		 float height_y1 = terrainData.GetHeightAt(info.min_x + x * (info.max_x - info.min_x) / (width - 1), info.min_z + z_minus_1 * (info.max_z - info.min_z) / (length - 1));
		 float height_y2 = terrainData.GetHeightAt(info.min_x + x * (info.max_x - info.min_x) / (width - 1), info.min_z + z_plus_1 * (info.max_z - info.min_z) / (length - 1));
		 float height_x1 = terrainData.GetHeightAt(info.min_x + x_minus_1 * (info.max_x - info.min_x) / (width - 1), info.min_z + z * (info.max_z - info.min_z) / (length - 1));
		 float height_x2 = terrainData.GetHeightAt(info.min_x + x_plus_1 * (info.max_x - info.min_x) / (width - 1), info.min_z + z * (info.max_z - info.min_z) / (length - 1));
 
		 XMFLOAT3 edge1(0.0f, height_y2 - height_y1, 2.0f * (info.max_z - info.min_z) / (length - 1));
		 XMFLOAT3 edge2(2.0f * (info.max_x - info.min_x) / (width - 1), height_x2 - height_x1, 0.0f);
		 
		 XMVECTOR normal = XMVector3Cross(DirectX::XMLoadFloat3(&edge1), XMLoadFloat3(&edge2));
		 normal = XMVector3Normalize(normal);

		 XMFLOAT3 result;
		 XMStoreFloat3(&result, normal);

		 return result;
	}
}


ID3D12Resource* TerrainLoader::get_heightmap_resource() const
{
	return _heightmapResource;
}

D3D12_GPU_DESCRIPTOR_HANDLE TerrainLoader::get_heightmap_srv() const
{
	return _heightmapSRV;
}

D3D12_CPU_DESCRIPTOR_HANDLE TerrainLoader::get_heightmap_cpu_srv() const
{
	// ResourceManager에서 가져올 때 저장해둔 CPU 핸들이 필요합니다.
// 만약 변수가 없다면 _heightmapCPUHandle 변수를 추가하거나
// ResourceManager에서 텍스처를 찾아서 가져와야 합니다.
	auto* tex = ResourceManager::instance()->get_texture(_heightmapTextureKey);
	if (tex) return tex->cpu_handle;
	return {};
}

void TerrainLoader::generate_grass_chunks(
	const std::string& layer_name,
	const std::string& grass_mesh_path,
	float chunk_size,
	int instances_per_chunk,
	float grass_weight_threshold)
{
	int layer_index = -1;
	for (size_t i = 0; i < _layers.size(); ++i)
	{
		if (_layers[i].name == layer_name)
		{
			layer_index = static_cast<int>(i);
			break;
		}
	}

	if (layer_index < 0 || layer_index >= static_cast<int>(_weightmapCaches.size())) return;

	// Grass 메시 로드
	auto grass_mesh = ResourceManager::instance()->load_mesh(grass_mesh_path);
	if (!grass_mesh) {
		CERROR("Failed to load grass mesh: " << grass_mesh_path);
		return;
	}

	const auto& info = _terrainData.GetInfo();
	float min_x = info.min_x;
	float max_x = info.max_x;
	float min_z = info.min_z;
	float max_z = info.max_z;

	int chunk_count = 0;
	int total_instances = 0;

	// 구역별 처리 (각 타일 내에서만)
	for (float x = min_x; x < max_x; x += chunk_size) {
		for (float z = min_z; z < max_z; z += chunk_size) {
			std::vector<XMMATRIX> instances;
			instances.reserve(instances_per_chunk);

			// 해당 구역 내 랜덤 배치
			for (int i = 0; i < instances_per_chunk; ++i) {
				float rand_x = static_cast<float>(rand())	/ RAND_MAX;
				float rand_z = static_cast<float>(rand()) / RAND_MAX;
				float px = x + rand_x * chunk_size;
				float pz = z + rand_z * chunk_size;

				// 범위 체크
				if (px < min_x || px > max_x || pz < min_z || pz > max_z)
					continue;

				// Weightmap 샘플링
				float grass_weight = get_grass_weight_at(px, pz, layer_index);
				if (grass_weight < grass_weight_threshold)
					continue;

				// 높이 샘플링
				float py = get_height_at(px, pz);

				// 변환 행렬 생성
				XMMATRIX rotation = create_grass_instance_transform(px, pz);
				XMMATRIX translation = XMMatrixTranslation(px, py, pz);
				instances.push_back(XMMatrixMultiply(rotation, translation));
			}

			if (instances.empty())
				continue;

			// ObjectManager를 통해 청크 오브젝트 생성
			auto chunk_obj = ObjectManager::instance()->create_game_object("GrassChunk");
			if (!chunk_obj) {
				CERROR("Failed to create grass chunk object");
				continue;
			}

			chunk_obj->transform()->set_local_position(
				XMFLOAT3(x + chunk_size / 2.0f, 0.0f, z + chunk_size / 2.0f)
			);
			chunk_obj->transform()->set_local_scale({ 1.4f, 1.4f, 1.4f });

			// FoliageRenderComponent 추가
			auto foliage_comp = chunk_obj->add_component<FoliageRenderComponent>();
			foliage_comp->set_mesh(grass_mesh);
			foliage_comp->set_instance_data(instances);
			foliage_comp->set_cast_shadow(false);
			foliage_comp->set_frustum_culling_enabled(true);
			foliage_comp->set_cull_distance(100.0f);

			chunk_count++;
			total_instances += static_cast<int>(instances.size());
		}
	}
}

float TerrainLoader::get_grass_weight_at(float world_x, float world_z, int layer_index) const
{
	if (!_hasLayers || _layers.empty()) return 0.0f;
	if (layer_index < 0 || layer_index >= static_cast<int>(_weightmapCaches.size()))
		return 0.0f;

	const auto& info = _terrainData.GetInfo();

	// 범위 체크
	if (world_x < info.min_x || world_x > info.max_x ||
		world_z < info.min_z || world_z > info.max_z) {
		return 0.0f;
	}

	float u = (world_x - info.min_x) / (info.max_x - info.min_x);
	float v = (world_z - info.min_z) / (info.max_z - info.min_z);

	u = (u < 0.0f) ? 0.0f : (u > 1.0f) ? 1.0f : u;
	v = (v < 0.0f) ? 0.0f : (v > 1.0f) ? 1.0f : v;

	int width = static_cast<int>(info.width);
	int height = static_cast<int>(info.height);

	int pixel_x = static_cast<int>(u * (width - 1));
	int pixel_y = static_cast<int>(v * (height - 1));

	pixel_x = (pixel_x < 0) ? 0 : (pixel_x >= width) ? width - 1 : pixel_x;
	pixel_y = (pixel_y < 0) ? 0 : (pixel_y >= height) ? height - 1 : pixel_y;

	int index = pixel_y * width + pixel_x;

	const auto& cache = _weightmapCaches[layer_index];
	if (index >= 0 && index < static_cast<int>(cache.size()))
		return cache[index];

	return 0.0f;
}

XMMATRIX TerrainLoader::create_grass_instance_transform(float x, float z) const
{
	float angle = static_cast<float>(rand()) / RAND_MAX * XM_2PI;
	return XMMatrixRotationY(angle);
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
	   // 각 weightmap_paths를 순회하며:
	   // 1. R8 바이너리 파일 읽기
	   // 2. D3D12 텍스처 리소스 생성 (DXGI_FORMAT_R8_UNORM)
	   // 3. ResourceManager에 "Weightmap_Rock", "Weightmap_Grass" 등으로 등록

	if (weightmap_paths.empty())
	{
		CERROR("No weightmap paths provided.");
		return;
	}

	_weightmapFilePaths = weightmap_paths;
	
	// ===== [미니맵용 Heightmap GPU 업로드] =====
	const auto& minimap_info = _terrainData.GetInfo();
	int minimap_width = static_cast<int>(minimap_info.width);
	int minimap_height = static_cast<int>(minimap_info.height);

	auto* minimap_rm = ResourceManager::instance();
	auto* heightmap_tex = minimap_rm->load_heightmap_from_raw(_heightmapTextureKey, minimap_width, minimap_height);

	if (heightmap_tex)
	{
		// TextureInfo에서 resource와 srv를 직접 저장
		_heightmapResource = heightmap_tex->resource.Get();
		_heightmapSRV = heightmap_tex->gpu_handle;
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
		if (layer_name.find("LANDSCAPE_VISIBILITY") != std::string::npos)
		{
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

	auto* albedo_array = rm->create_texture_array_from_loaded(_albedoArrayKey, albedo_keys);

	auto* normal_array = rm->create_texture_array_from_loaded(_normalArrayKey, normal_keys);

	auto* roughness_array = rm->create_texture_array_from_loaded(_roughnessArrayKey, roughness_keys);

	if (!albedo_array || !normal_array || !roughness_array)
	{
		CERROR("Failed to create layer texture arrays for: " << landscape_name);
		return;
	}

	// [추가] Weightmap 데이터를 CPU 메모리에 캐싱
	_weightmapCaches.clear();
	_weightmapCaches.reserve(_layers.size());

	for (size_t layer_idx = 0; layer_idx < _layers.size(); ++layer_idx)
	{
		std::vector<float> cache_data;
		cache_data.reserve(width * height);

		std::string path = _layers[layer_idx].weightmap_file;

		// R8 바이너리 파일 읽기
		std::ifstream file(path, std::ios::binary);
		if (!file.is_open())
		{
			// 파일 없으면 모두 0.0f로 채움
			cache_data.resize(width * height, 0.0f);
			_weightmapCaches.push_back(cache_data);
			continue;
		}

		// 바이너리 데이터 읽기 (R8 형식: uint8 배열)
		std::vector<uint8_t> raw_data(width * height);
		file.read(reinterpret_cast<char*>(raw_data.data()), width * height);
		file.close();

		// uint8 (0~255) → float (0.0~1.0) 변환
		for (uint8_t value : raw_data)
		{
			cache_data.push_back(static_cast<float>(value) / 255.0f);
		}

		_weightmapCaches.push_back(cache_data);
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