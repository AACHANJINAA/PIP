#include "stdafx.h"
#include "ResourceManager.h"
#include "Mesh.h" // ReadObjMesh, ReadGlbMesh 등을 포함해야 함
#include "ReadFBXMesh.h"
#include "ReadGlbMesh.h"
#include "ReadOBJMesh.h"
#include "ReadGLTFMesh.h"
#include "Renderer.h"
#include "DescriptorManager.h"
#include "DDSTextureLoader12.h"


void ResourceManager::initialize(ID3D12Device* device, ID3D12GraphicsCommandList* command_list)
{
    _device = device;
    _command_list = command_list;
}

void ResourceManager::release()
{
    unload_unused_meshes();

    for (auto& pair : _materials) {
        if (pair.second.material_cbuffer_gpu) {
            if (pair.second.material_cbuffer_cpu_address) {
                pair.second.material_cbuffer_gpu->Unmap(0, nullptr);
            }
            pair.second.material_cbuffer_gpu.Reset();
        }
    }
    _materials.clear();
    
    for (auto& pair : _textures) {
        pair.second.resource.Reset();
        pair.second.upload_heap.Reset();
    
    }
    _textures.clear();
}

std::shared_ptr<Mesh> ResourceManager::load_mesh(const std::string& file_path)
{
    // 이미 로드된 메시인지 확인
    auto it = _meshes.find(file_path);
    if (it != _meshes.end()) {
        return it->second;
    }

    // [수정] 파일 확장자에 따라 적절한 메시 로더를 선택
    std::shared_ptr<Mesh> new_mesh = nullptr;
    std::filesystem::path path(file_path);
    std::string extension = path.extension().string();

    //CINFO("Loading mesh: " << file_path << " | Detected extension: [" << extension << "]");

    if (extension == ".obj") new_mesh = std::make_shared<ReadOBJMesh>(file_path);
    else if (extension == ".glb") new_mesh = std::make_shared<ReadGlbMesh>(file_path);
    else if (extension == ".fbx") new_mesh = std::make_shared<ReadFBXMesh>(file_path);
    else if (extension == ".gltf") new_mesh = std::make_shared<ReadGLTFMesh>(file_path);
    else
    {
        CERROR("Unsupported mesh file format: " << file_path);
        return nullptr;
    }

    if (!new_mesh)
    {
        CERROR("Failed to create mesh object for: " << file_path);
        return nullptr;
    }

    _meshes[file_path] = new_mesh;
    _pending_meshes.push_back(new_mesh);

    return new_mesh;
}

void ResourceManager::upload_pending_meshes(ID3D12Device* device, ID3D12GraphicsCommandList* command_list)
{
    // 대기 목록에 있는 모든 메시에 대해 upload_to_gpu를 호출합니다.
    for (const auto& mesh : _pending_meshes)
    {
        if (!mesh->is_uploaded())
        {
            mesh->upload_to_gpu(device, command_list);
        }
    }
    // 업로드가 끝났으므로 대기 목록을 비웁니다.
    _pending_meshes.clear();
}

void ResourceManager::release_upload_buffers()
{
    for (const auto& val : _meshes | std::views::values)
    {
        if (val) val->release_upload_buffers();
    }

    // [추가] 텍스처 업로드 버퍼 해제
    for (auto& pair : _textures)
    {
        if (pair.second.upload_heap)
            {
                pair.second.upload_heap.Reset();
            }
    }
}

void ResourceManager::unload_unused_meshes()
{
    // 맵을 순회하면서 직접 원소를 제거하면 반복자가 무효화되어 위험
    std::vector<std::string> keys_to_unload;
    for (const auto& pair : _meshes)
    {
        const std::string& path = pair.first;
        const std::shared_ptr<Mesh>& mesh_ptr = pair.second;

        // use_count()가 1이라는 것은 오직 이 ResourceManager만이 참조하고 있다는 의미입니다.
        if (mesh_ptr.use_count() == 1)
        {
            keys_to_unload.push_back(path);
        }
    }

    // 수집된 키를 기반으로 맵에서 해당 메시들을 제거합니다.
    for (const std::string& key : keys_to_unload)
    {
        _meshes.erase(key);
        // 맵에서 shared_ptr이 제거되면, 참조 카운트가 0이 되어
        // Mesh 객체의 소멸자가 호출되고, GPU 리소스(ComPtr)도 자동으로 해제됩니다.

        // 로그를 남겨서 확인하면 좋습니다.
        //CINFO("Unloaded unused mesh: " << key);
    }
}


// --- 1단계 리팩토링 구현부 ---

// 내부 헬퍼 함수: 텍스처를 로드하고 GPU에 업로드합니다.
ResourceManager::TextureInfo * ResourceManager::load_texture(const std::string & file_path)
{
    if (file_path.empty()) {
        return nullptr;
    }
    
    auto it = _textures.find(file_path);
    if (it != _textures.end()) {
        return &it->second;
    }

    TextureInfo new_texture_info;
    new_texture_info.name = file_path;
    std::wstring w_file_path(file_path.begin(), file_path.end());
    
    std::unique_ptr<uint8_t[]> ddsData;
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    // 1. DDS 파일에서 텍스처 데이터를 메모리로 로드합니다.
    HRESULT hr = DirectX::LoadDDSTextureFromFile(_device, w_file_path.c_str(), &new_texture_info.resource, ddsData, subresources);
    
    if (FAILED(hr)) {
        CERROR("Failed to load texture file: " << file_path);
        return nullptr;
    }
    
    // 2. 최종 텍스처가 저장될 기본 힙(Default Heap) 리소스를 생성합니다.
    ComPtr<ID3D12Resource> texture_resource_default_heap;
    D3D12_RESOURCE_DESC texture_desc = new_texture_info.resource->GetDesc();
    auto default_heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    
    hr = _device->CreateCommittedResource(&default_heap_props, D3D12_HEAP_FLAG_NONE, &texture_desc,
             D3D12_RESOURCE_STATE_COPY_DEST, // 복사 대상으로 초기 상태 설정
             nullptr,
             IID_PPV_ARGS(&texture_resource_default_heap));
    
    if (FAILED(hr)) {
        CERROR("Failed to create committed resource for texture: " << file_path);
        return nullptr;
    }
    
    // 3. 데이터 복사를 위한 중간 업로드 힙(Upload Heap) 리소스를 생성합니다.
    const UINT64 upload_buffer_size = GetRequiredIntermediateSize(texture_resource_default_heap.Get(), 0, static_cast
    <UINT>(subresources.size()));
    auto upload_heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto upload_buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(upload_buffer_size);
    
    hr = _device->CreateCommittedResource(&upload_heap_props, D3D12_HEAP_FLAG_NONE, &upload_buffer_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&new_texture_info.upload_heap));
    
    if (FAILED(hr)) {
        CERROR("Failed to create upload heap for texture: " << file_path);
        return nullptr;
    }
    
    // 4. UpdateSubresources 헬퍼 함수를 사용해 메모리의 텍스처 데이터를 업로드 힙을 거쳐 기본 힙으로 복사합니다.
    UpdateSubresources(_command_list, texture_resource_default_heap.Get(), new_texture_info.upload_heap.Get(), 0, 0,
    static_cast<UINT>(subresources.size()), subresources.data());
    
    // 5. 텍스처 리소스의 상태를 셰이더에서 읽을 수 있도록 변경합니다.
    auto transition = CD3DX12_RESOURCE_BARRIER::Transition(texture_resource_default_heap.Get(),
    D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    _command_list->ResourceBarrier(1, &transition);
    
    // 6. 최종적으로 생성된 기본 힙의 리소스를 저장합니다.
    new_texture_info.resource = texture_resource_default_heap;
    
    // 7. 셰이더 리소스 뷰(SRV)를 생성합니다.
    if (!DescriptorManager::instance()->allocate_descriptor(new_texture_info.cpu_handle, new_texture_info.gpu_handle))
    {
        CERROR("Failed to allocate descriptor for texture: " << file_path);
        return nullptr;
    }
    
    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Format = new_texture_info.resource->GetDesc().Format;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MostDetailedMip = 0;
    srv_desc.Texture2D.MipLevels = new_texture_info.resource->GetDesc().MipLevels;
    srv_desc.Texture2D.ResourceMinLODClamp = 0.0f;
    
    _device->CreateShaderResourceView(new_texture_info.resource.Get(), &srv_desc, new_texture_info.cpu_handle);
    
    _textures[file_path] = std::move(new_texture_info);
    
    return &_textures[file_path];
}

std::vector<std::string> ResourceManager::load_materials_from_gltf(const std::string & file_path)
{
    using json = nlohmann::json;
    std::vector<std::string> loaded_material_names;
    
        std::ifstream gltf_file(file_path);
    if (!gltf_file.is_open()) {
            CERROR("Failed to open glTF file: " << file_path);
            return loaded_material_names;
    
    }

    json gltf_json;
    gltf_file >> gltf_json;
    gltf_file.close();
    
    std::filesystem::path base_path = std::filesystem::path(file_path).parent_path();
    std::vector<std::string> processed_image_paths;
    
    if (gltf_json.contains("images")) {
        for (const auto& image_json : gltf_json["images"]) {
            std::string uri = image_json.value("uri", "");
            if (uri.empty()) {
                processed_image_paths.push_back("");
                continue;
            }

            std::filesystem::path original_path = base_path / uri;
            std::filesystem::path dds_path = original_path;
            dds_path.replace_extension(".dds");
            
            if (std::filesystem::exists(dds_path)) {
                processed_image_paths.push_back(dds_path.string());
                // CINFO("DDS texture found and will be used: " << dds_path.string());
            }
            else {
                processed_image_paths.push_back(original_path.string());
            }
        }
    }
    
    std::vector<std::string> texture_source_paths;
    if (gltf_json.contains("textures")) {
        for (const auto& texture_json : gltf_json["textures"]) {
            int source_idx = texture_json.value("source", -1);
            if (source_idx != -1 && source_idx < processed_image_paths.size()) {
                // glTF URI는 상대 경로일 수 있으므로 base_path와 결합
                texture_source_paths.push_back(processed_image_paths[source_idx]);
            }
            else {
                 texture_source_paths.push_back(""); // 텍스처 소스 없음
            }
        }
    }
    
    if (gltf_json.contains("materials")) {
        for (const auto& mat_json : gltf_json["materials"]) {
            std::string mat_name = mat_json.value("name", "UnnamedMaterial_" + std::to_string(_materials.size()));
    
            create_material(mat_name);
            MaterialInfo & new_mat_info = _materials[mat_name];
    
            const auto& pbr = mat_json.value("pbrMetallicRoughness", json::object());
    
            // Factors
            if (pbr.contains("baseColorFactor")) {
                new_mat_info.base_color_factor = { pbr["baseColorFactor"][0], pbr["baseColorFactor"][1], pbr["baseColorFactor"][2], pbr["baseColorFactor"][3] };
            }
            new_mat_info.metallic_factor = pbr.value("metallicFactor", 1.0f);
            new_mat_info.roughness_factor = pbr.value("roughnessFactor", 1.0f);
            if (mat_json.contains("emissiveFactor")) {
                    new_mat_info.emissive_factor = { mat_json["emissiveFactor"][0], mat_json["emissiveFactor"][1], mat_json["emissiveFactor"][2] };
            }
            // Textures
            auto assign_texture = [&](const json& texture_info, std::string& path_member) {
                    if (texture_info.contains("index")) {
                            int tex_idx = texture_info["index"];
                            if (tex_idx < texture_source_paths.size() && !texture_source_paths[tex_idx].empty()) {
                                    path_member = texture_source_paths[tex_idx];
                                    load_texture(path_member);
                            }
                    }
            };

            if (pbr.contains("baseColorTexture")) assign_texture(pbr["baseColorTexture"], new_mat_info.base_color_texture_path);
            if (pbr.contains("metallicRoughnessTexture")) assign_texture(pbr["metallicRoughnessTexture"], new_mat_info.metallic_roughness_texture_path);
            if (mat_json.contains("normalTexture")) {
                assign_texture(mat_json["normalTexture"], new_mat_info.normal_texture_path);
                new_mat_info.normal_texture_scale = mat_json["normalTexture"].value("scale", 1.0f);
            }
            if (mat_json.contains("emissiveTexture")) assign_texture(mat_json["emissiveTexture"], new_mat_info.emissive_texture_path);
            
            // Other properties
            new_mat_info.alpha_mode = mat_json.value("alphaMode", "OPAQUE");
            new_mat_info.alpha_cutoff = mat_json.value("alphaCutoff", 0.5f);
            new_mat_info.double_sided = mat_json.value("doubleSided", false);
            
            loaded_material_names.push_back(mat_name); 
        }
    }
    return loaded_material_names;
}

void ResourceManager::create_material(const std::string & material_name)
{
    if (_materials.find(material_name) != _materials.end()) {
        //CWARNING("Material with name " + material_name + " already exists.");
        return;
    }
    MaterialInfo new_mat_info;
    new_mat_info.name = material_name;

    UINT buffer_size = (sizeof(GltfMaterialConstantBuffer) + 255) & ~255;

    auto heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(buffer_size);

    _device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &buffer_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&new_mat_info.material_cbuffer_gpu));

    new_mat_info.material_cbuffer_gpu->Map(0, nullptr, reinterpret_cast<void**>(&new_mat_info.material_cbuffer_cpu_address));

    _materials[material_name] = std::move(new_mat_info);
}

void ResourceManager::set_shader_for_material(const std::string & material_name, const std::string& shader_name)
{
    auto it = _materials.find(material_name);
    if (it != _materials.end()) {
            it->second.shader_name = shader_name;
    }
    else {
        CERROR("Material not found: " << material_name);
    
    }
}

void ResourceManager::add_texture_to_material(const std::string& material_name, const std::string& texture_path)
{
    auto it = _materials.find(material_name);
    if (it != _materials.end()) {
        // 이 함수는 어떤 텍스처를 추가할지 명확하지 않으므로,
        // 필요에 따라 base_color_texture_path를 설정하는 것으로 가정합니다.
        // 실제 사용 시에는 텍스처 유형을 인자로 받아 처리하는 것이 좋습니다.
        it->second.base_color_texture_path = texture_path;
        load_texture(texture_path);
    } else {
        CERROR("Material not found: " << material_name);
    }
}

void ResourceManager::bind_material(const std::string& material_name, ID3D12GraphicsCommandList* command_list)
{
    auto it = _materials.find(material_name);
    if (it == _materials.end()) {
        CERROR("Attempted to bind non-existent material: " << material_name);
        return;
    }

    MaterialInfo & mat_info = it->second;
    auto renderer = Renderer::instance();
    
    // 1. 셰이더 이름으로 Renderer에서 PSO와 Root Signature를 직접 가져와 바인딩
    //    (규칙: PSO와 Root Signature는 같은 이름을 공유)
    auto root_signature = renderer->get_root_signature(mat_info.shader_name);
    auto pso = renderer->get_pso(mat_info.shader_name);

    if (!root_signature || !pso) {
        //렌더러에 해당 이름으로 리소스가 등록되었는지 확인
        //CERROR("Failed to get RootSignature or PSO from Renderer for shader : " << mat_info.shader_name);
        return;
    }
    
    command_list->SetGraphicsRootSignature(root_signature);
    command_list->SetPipelineState(pso);

    // 1. 상수 버퍼 내용 업데이트
    GltfMaterialConstantBuffer constants;
    constants.BaseColorFactor = mat_info.base_color_factor;
    constants.EmissiveFactor = mat_info.emissive_factor;
    constants.MetallicFactor = mat_info.metallic_factor;
    constants.RoughnessFactor = mat_info.roughness_factor;
    constants.NormalTextureScale = mat_info.normal_texture_scale;
    constants.AlphaCutoff = mat_info.alpha_cutoff;

    if (mat_info.alpha_mode == "MASK") constants.AlphaMode = 1;
    else if (mat_info.alpha_mode == "BLEND") constants.AlphaMode = 2;
    else constants.AlphaMode = 0; // OPAQUE

    constants.DoubleSided = mat_info.double_sided ? 1 : 0;
    constants.HasBaseColorTexture = !mat_info.base_color_texture_path.empty();
    constants.HasMetallicRoughnessTexture = !mat_info.metallic_roughness_texture_path.empty();
    constants.HasNormalTexture = !mat_info.normal_texture_path.empty();
    constants.HasEmissiveTexture = !mat_info.emissive_texture_path.empty();

    memcpy(mat_info.material_cbuffer_cpu_address, &constants, sizeof(GltfMaterialConstantBuffer));

    command_list->SetGraphicsRootConstantBufferView(2, mat_info.material_cbuffer_gpu->GetGPUVirtualAddress());

    // 3. 텍스처 핸들 수집 및 Renderer를 통해 바인딩
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> texture_handles;
    auto add_texture_handle = [&](const std::string& path) {
    if (!path.empty()) {
        auto tex_it = _textures.find(path);
            if (tex_it != _textures.end()) {
                texture_handles.push_back(tex_it->second.cpu_handle);
            }
        }
    };
    
    add_texture_handle(mat_info.base_color_texture_path);
    add_texture_handle(mat_info.normal_texture_path);
    add_texture_handle(mat_info.metallic_roughness_texture_path);
    add_texture_handle(mat_info.emissive_texture_path);
    
    if (!texture_handles.empty()) {
        renderer->bind_texture_table(command_list, 4, texture_handles);
    }
}