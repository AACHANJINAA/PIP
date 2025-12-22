#include "stdafx.h"
#include "TerrainLoader.h"
#include "ResourceManager.h"
#include "Renderer.h"

using namespace DirectX;

TerrainLoader::TerrainLoader(const std::string& heightmap_json_path)
{
    
    if (!_terrainData.LoadFromJSON(heightmap_json_path))
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
    _terrainInfo.tiling = XMFLOAT2(32.0f, 32.0f);

    // 2. Flat Grid Mesh 
    int grid_width = static_cast<int>(info.width) - 1;
    int grid_height = static_cast<int>(info.height) - 1;
    create_flat_grid(grid_width, grid_height);
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

         int x_plus_1 = min(x + 1, width - 1);
	     int x_minus_1 = max(x - 1, 0);
	     int z_plus_1 = min(z + 1, length - 1);
	     int z_minus_1 = max(z - 1, 0);

         float height_y1 = terrainData.GetHeightAt(info.min_x + x * (info.max_x - info.min_x) / (width - 1), info.min_z + z_minus_1 * (info.max_z - info.min_z) / (length - 1));
         float height_y2 = terrainData.GetHeightAt(info.min_x + x * (info.max_x - info.min_x) / (width - 1), info.min_z + z_plus_1 * (info.max_z - info.min_z) / (length - 1));
		 float height_x1 = terrainData.GetHeightAt(info.min_x + x_minus_1 * (info.max_x - info.min_x) / (width - 1), info.min_z + z * (info.max_z - info.min_z) / (length - 1));
		 float height_x2 = terrainData.GetHeightAt(info.min_x + x_plus_1 * (info.max_x - info.min_x) / (width - 1), info.min_z + z * (info.max_z - info.min_z) / (length - 1));
 
		 DirectX::XMFLOAT3 edge1(0.0f, height_y2 - height_y1, 2.0f * (info.max_z - info.min_z) / (length - 1));
		 DirectX::XMFLOAT3 edge2(2.0f * (info.max_x - info.min_x) / (width - 1), height_x2 - height_x1, 0.0f);
		 
         DirectX::XMVECTOR normal = DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&edge1), DirectX::XMLoadFloat3(&edge2));
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


void TerrainLoader::load_textures_to_resource_manager(const std::string& material_gltf_path)
{
    const auto& info = _terrainData.GetInfo();
    auto* rm = ResourceManager::instance();

    // 1. HeightMap
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

   // 2. glTF Material (ResourceManager handles details)
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
}

void TerrainLoader::render(ID3D12GraphicsCommandList* command_list)
{
    if (!is_uploaded())
    {
        CERROR("TerrainLoader: Mesh not uploaded to GPU!");
        return;
    }

    if (!_materialName.empty())
    {
        ResourceManager::instance()->bind_material(_materialName, command_list);
    }
    else
    {
        CERROR("TerrainLoader: No material to bind!");
    }

    // Vertex/Index Buffer
    command_list->IASetVertexBuffers(0, 1, &_vertexBufferView);
    command_list->IASetIndexBuffer(&_indexBufferView);
    command_list->IASetPrimitiveTopology(_primitiveTopology);

    // Draw Call
    command_list->DrawIndexedInstanced(
        static_cast<UINT>(_indices.size()), 1, 0, 0, 0
    );

    // [수정] 터레인 렌더링 후 텍스처 상태를 즉시 초기화하여 다른 객체에 영향을 주지 않도록 합니다.
    auto* rm = ResourceManager::instance();
    auto* renderer = Renderer::instance();
    
    // 기본 텍스처 핸들을 ResourceManager에서 가져옵니다.
    auto* white_tex_info = rm->get_texture("__DEFAULT_WHITE__");
    auto* normal_tex_info = rm->get_texture("__DEFAULT_NORMAL__");
    auto* orm_tex_info = rm->get_texture("__DEFAULT_ORM__");     // <-- 추가! (ResourceManager에서 만들었어야 함)
    auto* black_tex_info = rm->get_texture("__DEFAULT_BLACK__"); // <-- 추가!

    // 핸들이 유효한 경우에만 (즉, 기본 텍스처가 성공적으로 생성된 경우) 초기화를 진행합니다.
    if (white_tex_info && normal_tex_info && orm_tex_info && black_tex_info)
    {
        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> texture_handles;
        texture_handles.push_back(white_tex_info->cpu_handle);  // t0 (Base Color)
        texture_handles.push_back(normal_tex_info->cpu_handle); // t1 (Normal Map)
        texture_handles.push_back(orm_tex_info->cpu_handle);    // t2 (Metallic/Roughness/AO)
        texture_handles.push_back(black_tex_info->cpu_handle);  // t3 (Emissive)

        // GltfShader에서 사용하는 텍스처 테이블(루트 파라미터 4번)을 기본 텍스처로 덮어씁니다.
        renderer->bind_texture_table(command_list, 4, texture_handles);
    }

}

float TerrainLoader::get_height_at(float world_x, float world_z) const
{
    // Common::TerrainData 
    return _terrainData.GetHeightAt(world_x, world_z);
}