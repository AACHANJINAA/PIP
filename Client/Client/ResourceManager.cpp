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
#include "WICTextureLoader12.h"


void ResourceManager::create_default_textures(ID3D12Device* device, ID3D12GraphicsCommandList* command_list)
{
    // Helper lambda to create a 1x1 texture with specific pixel data
    auto create_1x1_texture = [&](const std::string& name, const void* pixel_data)
    {
        if (_textures.count(name)) return; // Avoid recreating

        TextureInfo new_tex_info;
        new_tex_info.name = name;

        D3D12_RESOURCE_DESC tex_desc = {};
        tex_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        tex_desc.Width = 1;
        tex_desc.Height = 1;
        tex_desc.DepthOrArraySize = 1;
        tex_desc.MipLevels = 1;
        tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        tex_desc.SampleDesc.Count = 1;
        tex_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        tex_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

        auto default_heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        HRESULT hr = device->CreateCommittedResource(&default_heap_props, D3D12_HEAP_FLAG_NONE, &tex_desc, D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&new_tex_info.resource));
        if (FAILED(hr)) { CERROR("Failed to create default texture resource: " + name); return; }
        new_tex_info.resource->SetName(std::wstring(name.begin(), name.end()).c_str());

        const UINT64 upload_buffer_size = GetRequiredIntermediateSize(new_tex_info.resource.Get(), 0, 1);
        auto upload_heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto upload_buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(upload_buffer_size);

        hr = device->CreateCommittedResource(&upload_heap_props, D3D12_HEAP_FLAG_NONE, &upload_buffer_desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&new_tex_info.upload_heap));
        if (FAILED(hr)) { CERROR("Failed to create upload heap for default texture: " + name); return; }
        new_tex_info.upload_heap->SetName((std::wstring(name.begin(), name.end()) + L"_Upload").c_str());

        D3D12_SUBRESOURCE_DATA subresource_data = {};
        subresource_data.pData = pixel_data;
        subresource_data.RowPitch = sizeof(uint8_t) * 4;
        subresource_data.SlicePitch = subresource_data.RowPitch;

        UpdateSubresources(command_list, new_tex_info.resource.Get(), new_tex_info.upload_heap.Get(), 0, 0, 1, &subresource_data);

        auto transition = CD3DX12_RESOURCE_BARRIER::Transition(new_tex_info.resource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        command_list->ResourceBarrier(1, &transition);

        if (!DescriptorManager::instance()->allocate_descriptor(new_tex_info.cpu_handle, new_tex_info.gpu_handle)) { CERROR("Failed to allocate descriptor for default texture: " + name); return; }

        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.Format = tex_desc.Format;
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Texture2D.MipLevels = 1;
        device->CreateShaderResourceView(new_tex_info.resource.Get(), &srv_desc, new_tex_info.cpu_handle);

        _textures[name] = std::move(new_tex_info);
    };

    uint8_t white_pixel[4] = { 0, 0, 0, 255 }; // White
    uint8_t normal_pixel[4] = { 128, 128, 255, 255 }; // Flat normal (R:128, G:128, B:255)
    uint8_t orm_pixel[4] = { 255, 255, 0, 255 }; // R(AO)=255, G(Rough)=255, B(Metal)=0, A=255 <- non-roughness + good light receive
    uint8_t black_pixel[4] = { 0, 0, 0, 255 }; // Black  

    create_1x1_texture("__DEFAULT_WHITE__", white_pixel);
    create_1x1_texture("__DEFAULT_NORMAL__", normal_pixel);
    create_1x1_texture("__DEFAULT_ORM__", orm_pixel);
    create_1x1_texture("__DEFAULT_BLACK__", black_pixel);
}

void ResourceManager::initialize(ID3D12Device* device, ID3D12GraphicsCommandList* command_list)
{
    _device = device;
    _command_list = command_list;

    create_default_textures(device, command_list);
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

std::shared_ptr<Mesh> ResourceManager::load_mesh(const std::string& file_path, bool _isAnimated, std::string animation_name)
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
    else if (extension == ".gltf") new_mesh = std::make_shared<ReadGLTFMesh>(file_path, _isAnimated, animation_name);
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

    if (new_mesh) {
        _meshes[file_path] = new_mesh;
        _pending_meshes.push_back(new_mesh); // 큐에 등록만 함!
    }

    return new_mesh;
}

void ResourceManager::process_pending_uploads(ID3D12Device* device, ID3D12GraphicsCommandList* command_list, UINT64 targetFenceValue)
{
    if (_pending_meshes.empty()) return;

    int uploadCount = 0;
    while (!_pending_meshes.empty() && uploadCount < 2)
    {
        auto mesh = _pending_meshes.front();
        _pending_meshes.pop_front();

        if (mesh && !mesh->is_uploaded()) {
            // [변경] targetFenceValue를 넘겨줌
            mesh->upload_to_gpu(device, command_list, targetFenceValue);
            uploadCount++;
        }
    }
}

void ResourceManager::register_manual_mesh(const std::string& name, std::shared_ptr<Mesh> mesh)
{
    if (mesh == nullptr || _meshes.contains(name)) return;

    _meshes[name] = mesh;
    _pending_meshes.push_back(mesh); // 대기열에 추가하여 다음 프레임에 업로드 유도
}

//void ResourceManager::upload_pending_meshes(ID3D12Device* device, ID3D12GraphicsCommandList* command_list)
//{
//    // 대기 목록에 있는 모든 메시에 대해 upload_to_gpu를 호출합니다.
//    for (const auto& mesh : _pending_meshes)
//    {
//        if (!mesh->is_uploaded())
//        {
//            mesh->upload_to_gpu(device, command_list, TODO);
//        }
//    }
//    // 업로드가 끝났으므로 대기 목록을 비웁니다.
//    _pending_meshes.clear();
//}

void ResourceManager::release_upload_buffers(UINT64 completedFenceValue)
{
    // 큐의 앞부분부터 검사 (오래된 것부터)
    while (!_pendingDeleteBuffers.empty())
    {
        if (_pendingDeleteBuffers.front().targetFenceValue <= completedFenceValue) {
            // GPU 작업 완료됨 -> 큐에서 빼면 ComPtr 소멸자가 호출되며 리소스 해제
            _pendingDeleteBuffers.pop_front();
        }
        else {
            // 아직 사용 중인 버퍼를 만나면 중단 (뒤에 있는 것들도 당연히 사용 중임)
            break;
        }
    }
}
void ResourceManager::register_upload_buffer(ComPtr<ID3D12Resource> buffer, UINT64 targetFenceValue)
{
    if (buffer) {
        _pendingDeleteBuffers.push_back({ buffer, targetFenceValue });
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


// 내부 헬퍼 함수: 텍스처를 로드하고 GPU에 업로드합니다.
ResourceManager::TextureInfo * ResourceManager::load_texture(const std::string & file_path, D3D12_SRV_DIMENSION view_dimension)
{
    if (file_path.empty()) {
        return nullptr;
    }
    
	// 1. 항상 원본 파일 경로를 키로 사용하여 캐시를 확인
    auto it = _textures.find(file_path);
    if (it != _textures.end()) {
        CLOG("Texture cache hit for: " << file_path);
        return &it->second;
    }

    //CINFO("Loading texture (cache miss): " << file_path);

    HRESULT hr = E_FAIL;

    TextureInfo new_texture_info;
    new_texture_info.name = file_path;

    // 2. DDS 경로 생성
    std::filesystem::path original_path(file_path);
    std::filesystem::path dds_path = original_path;
    // 확장자 대체
    dds_path.replace_extension(".dds");
    // DDS 파일에서 텍스처 데이터를 메모리로 로드합니다.
    std::unique_ptr<uint8_t[]> dds_data;
    std::unique_ptr<uint8_t[]> wic_data;
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
   
    // 3. DDS 우선 로드 시도
    if (std::filesystem::exists(dds_path))
    {
        std::wstring w_dds_path = dds_path.wstring();
         hr = DirectX::LoadDDSTextureFromFile(_device, w_dds_path.c_str(), &new_texture_info.resource, dds_data, subresources);
    }

    // 4. DDS 로드 실패 시, 원본 파일(PNG, JPG 등) 로드 시도
    if (FAILED(hr))
    {
        if (!std::filesystem::exists(original_path))
        {
             CERROR("Texture file does not exist: " << file_path);
             return nullptr;
        }
    
        std::wstring w_original_path = original_path.wstring();
        D3D12_SUBRESOURCE_DATA wic_subresource_data;
        
        hr = DirectX::LoadWICTextureFromFile(
            _device,
            w_original_path.c_str(),
            &new_texture_info.resource, // 최종 리소스 (Default Heap)
            wic_data,           // 디코딩된 이미지 데이터
            wic_subresource_data        // 서브리소스 정보
        );

        if (SUCCEEDED(hr)) {
           // subresources 벡터에 WIC에서 얻은 서브리소스를 추가
           subresources.push_back(wic_subresource_data);
        }
    }
    
    // 5. 모두 실패 시 에러 처리
    if (FAILED(hr)) {
        CERROR("Failed to load texture file: " << file_path);
        return nullptr;
    }
    
    // 6. GPU 업로드 및 리소스 상태 전이 (DDS와 WIC 경우 둘다 공통으로 사용)

    const UINT64 upload_buffer_size = GetRequiredIntermediateSize(new_texture_info.resource.Get(), 0, static_cast<UINT>(subresources.size()));
    auto upload_heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto upload_buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(upload_buffer_size);
   
    hr = _device->CreateCommittedResource(
        &upload_heap_props, D3D12_HEAP_FLAG_NONE, &upload_buffer_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&new_texture_info.upload_heap));
   
    if (FAILED(hr)) { CERROR("Failed to create upload heap for DDS texture."); return nullptr; }
   
    UpdateSubresources(
        _command_list, new_texture_info.resource.Get(), new_texture_info.upload_heap.Get(),
        0, 0, static_cast<UINT>(subresources.size()), subresources.data());
   
    auto transition = CD3DX12_RESOURCE_BARRIER::Transition(
        new_texture_info.resource.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    _command_list->ResourceBarrier(1, &transition);
    
    // 7. 셰이더 리소스 뷰(SRV)를 생성합니다.
    if (!DescriptorManager::instance()->allocate_descriptor(new_texture_info.cpu_handle, new_texture_info.gpu_handle))
    {
        CERROR("Failed to allocate descriptor for texture: " << file_path);
        return nullptr;
    }
    
    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Format = new_texture_info.resource->GetDesc().Format;

    srv_desc.ViewDimension = view_dimension;

    switch (view_dimension)
    {
    case D3D12_SRV_DIMENSION_TEXTURE2D:
            srv_desc.Texture2D.MipLevels = new_texture_info.resource->GetDesc().MipLevels;
            srv_desc.Texture2D.MostDetailedMip = 0;
            srv_desc.Texture2D.ResourceMinLODClamp = 0.0f;
            break;
    
    case D3D12_SRV_DIMENSION_TEXTURECUBE:
            srv_desc.TextureCube.MipLevels = new_texture_info.resource->GetDesc().MipLevels;
            srv_desc.TextureCube.MostDetailedMip = 0;
            srv_desc.TextureCube.ResourceMinLODClamp = 0.0f;
            break;
    
            // 다른 뷰 차원(Texture2DArray 등)이 필요하면 여기에 case를 추가할 수 있습니다.
    default:
            CERROR("Unsupported SRV dimension for texture: " << file_path);
            return nullptr; // 처리할 수 없는 경우
    }

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
    
    std::vector<std::string> images_uris;
    if (gltf_json.contains("images")) {
        for (const auto& image_json : gltf_json["images"]) {
            std::string uri = image_json.value("uri", "");
            images_uris.push_back(uri);
        }
    }
    
    std::vector<std::string> texture_source_uris;
    if (gltf_json.contains("textures")) {
        for (const auto& texture_json : gltf_json["textures"]) {
            int source_idx = texture_json.value("source", -1);
            if (source_idx != -1 && source_idx < images_uris.size()) {
                // glTF URI는 상대 경로일 수 있으므로 base_path와 결합
                texture_source_uris.push_back(images_uris[source_idx]);
            }
            else {
                texture_source_uris.push_back(""); // 텍스처 소스 없음
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
                            if (tex_idx < texture_source_uris.size() && !texture_source_uris[tex_idx].empty()) {
                                //gltf의 상대 경로 및 기본 경로를 조합하여 전체 경로로 만듦
                                std::filesystem::path full_texture_path = base_path / texture_source_uris[tex_idx];
                                path_member = full_texture_path.string();

								// 이제 DDS 우선 로드 및 실패 시 WIC 로드를 수행
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

    MaterialInfo& mat_info = it->second;
    auto renderer = Renderer::instance();

    // 1. 셰이더 이름으로 Renderer에서 PSO와 Root Signature를 직접 가져와 바인딩
    //    (규칙: PSO와 Root Signature는 같은 이름을 공유)
    auto root_signature = renderer->get_root_signature(mat_info.shader_name);
    auto pso = renderer->get_pso(mat_info.shader_name);

    if (!root_signature || !pso) {
        return;
    }

    // DW수정 : 아래 두 줄을 주석처리하여 shader_name을 무시한다.
    //          추후에 프리미티브 단위로 쉐이더의 구조나 루트 시그너처 pso의 구조가 바뀌면 여기서도 수정이 필히 필요할 것이다.
    //          그래서 아래 두 줄을 지우지는 않고 남겨두는 것이 좋을 것 같다.
    //command_list->SetGraphicsRootSignature(root_signature);
    //command_list->SetPipelineState(pso);


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

    auto get_cpu_handle = [&](const std::string& path) -> D3D12_CPU_DESCRIPTOR_HANDLE {
        if (!path.empty()) {
            auto tex_it = _textures.find(path);
            if (tex_it != _textures.end() && tex_it->second.cpu_handle.ptr != 0) {
                return tex_it->second.cpu_handle;
            }
        }
        return {};
        };

    D3D12_CPU_DESCRIPTOR_HANDLE base_color_handle = get_cpu_handle(mat_info.base_color_texture_path);

    D3D12_CPU_DESCRIPTOR_HANDLE default_white_handle = get_cpu_handle("__DEFAULT_WHITE__");
    D3D12_CPU_DESCRIPTOR_HANDLE default_normal_handle = get_cpu_handle("__DEFAULT_NORMAL__");
    D3D12_CPU_DESCRIPTOR_HANDLE default_orm_handle = get_cpu_handle("__DEFAULT_ORM__");
    D3D12_CPU_DESCRIPTOR_HANDLE default_black_handle = get_cpu_handle("__DEFAULT_BLACK__");

    // 만약 기본 색상 텍스처조차 없다면, 텍스처를 바인딩하지 않고 종료합니다.
    if (base_color_handle.ptr == 0) {
        base_color_handle = default_white_handle;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE normal_handle = get_cpu_handle(mat_info.normal_texture_path);
    if (normal_handle.ptr == 0) normal_handle = default_normal_handle;
    
    D3D12_CPU_DESCRIPTOR_HANDLE orm_handle = get_cpu_handle(mat_info.metallic_roughness_texture_path);
    if (orm_handle.ptr == 0) orm_handle = default_orm_handle;

    D3D12_CPU_DESCRIPTOR_HANDLE emissive_handle = get_cpu_handle(mat_info.emissive_texture_path);
    if (emissive_handle.ptr == 0) emissive_handle = default_black_handle;

    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> texture_handles;
    texture_handles.push_back(base_color_handle);  // 셰이더의 t0
    texture_handles.push_back(normal_handle);      // 셰이더의 t1
    texture_handles.push_back(orm_handle);         // 셰이더의 t2
    texture_handles.push_back(emissive_handle);    // 셰이더의 t3
    
    // 이 벡터를 루트 파라미터 4번에 테이블로 바인딩합니다.
    renderer->bind_texture_table(command_list, 4, texture_handles);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////SkyBox//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ResourceManager::load_skybox(const std::string& file_path)
{
    _skybox_texture_path = file_path;
    load_cubemap_from_dds(file_path);
}

D3D12_GPU_DESCRIPTOR_HANDLE ResourceManager::get_skybox_srv()
{
    if (_skybox_texture_path.empty())
    {
        CERROR("Skybox texture has not been loaded yet.");
        return {}; // 유효하지 않은 핸들 반환
    }

    auto it = _textures.find(_skybox_texture_path);
    if (it != _textures.end())
    {
        return it->second.gpu_handle;
    }

    CERROR("Skybox texture not found in texture map: " << _skybox_texture_path);
    return {}; // 유효하지 않은 핸들 반환
}

D3D12_CPU_DESCRIPTOR_HANDLE ResourceManager::get_skybox_srv_cpu() const
{
    if (_skybox_texture_path.empty()) return {};
    auto it = _textures.find(_skybox_texture_path);
    if (it != _textures.end()) return it->second.cpu_handle;
    return {};
}

ResourceManager::TextureInfo* ResourceManager::load_cubemap_from_dds(const std::string& file_path)
{
    // 1. 캐시 확인
    auto it = _textures.find(file_path);
    if (it != _textures.end()) {
        return &it->second;
    }

    // Debug: 절대 경로 계산 및 존재성 확인
    std::filesystem::path dds_path = std::filesystem::absolute(file_path);
    CLOG("Attempting to load cubemap DDS: " << file_path << " -> abs: " << dds_path.string());

    if (!std::filesystem::exists(dds_path)) {
        CERROR("Cubemap DDS file not found: " << dds_path.string());
        return nullptr;
    }

    // 파일 읽기 가능성 확인 (권한/락 체크용)
    std::ifstream ifs(dds_path, std::ios::binary | std::ios::ate);
    if (!ifs.is_open()) {
        CERROR("Cannot open cubemap DDS file (permission/locked?): " << dds_path.string());
        return nullptr;
    }
    auto file_size = ifs.tellg();
    ifs.close();

    // Device 가 유효한지 확인
    if (!_device) {
        CERROR("ResourceManager::_device is null. Ensure initialize() was called with valid device.");
        return nullptr;
    }

    TextureInfo new_texture_info;
    std::unique_ptr<uint8_t[]> dds_data;
    std::vector<D3D12_SUBRESOURCE_DATA> subresources;

    std::wstring w_dds_path = dds_path.wstring();
    HRESULT hr = DirectX::LoadDDSTextureFromFile(_device, w_dds_path.c_str(), &new_texture_info.resource, dds_data, subresources);

    if (FAILED(hr)) {
        CERROR("Failed to load cubemap DDS file: " << dds_path.string() << " HRESULT=0x" << std::hex << hr);
        CERROR("If this DDS is not a standard DDS or uses an unsupported header, LoadDDSTextureFromFile may fail.");
        return nullptr;
    }

    // 3. 큐브맵인지 강력하게 확인
    auto desc = new_texture_info.resource->GetDesc();
    if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || desc.DepthOrArraySize != 6) {
        CERROR("The provided DDS file is not a cubemap (Dimension/ArraySize mismatch): " << dds_path.string());
        return nullptr; // 큐브맵이 아니면 확실히 실패 처리
    }

    // 4. GPU 업로드 (load_texture의 로직과 동일)
    const UINT64 upload_buffer_size = GetRequiredIntermediateSize(new_texture_info.resource.Get(), 0, static_cast<UINT>(subresources.size()));
    auto upload_heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto upload_buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(upload_buffer_size);

    hr = _device->CreateCommittedResource(
        &upload_heap_props, D3D12_HEAP_FLAG_NONE, &upload_buffer_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&new_texture_info.upload_heap));

    if (FAILED(hr)) { CERROR("Failed to create upload heap for cubemap."); return nullptr; }

    UpdateSubresources(_command_list, new_texture_info.resource.Get(), new_texture_info.upload_heap.Get(),
        0, 0, static_cast<UINT>(subresources.size()), subresources.data());

    auto transition = CD3DX12_RESOURCE_BARRIER::Transition(
        new_texture_info.resource.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    _command_list->ResourceBarrier(1, &transition);

    // 5. 큐브맵용 SRV 생성 (load_texture의 로직과 동일)
    if (!DescriptorManager::instance()->allocate_descriptor(new_texture_info.cpu_handle, new_texture_info.gpu_handle))
    {
        CERROR("Failed to allocate descriptor for cubemap: " << file_path);
        return nullptr;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Format = new_texture_info.resource->GetDesc().Format;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE; // 큐브맵으로 고정
    srv_desc.TextureCube.MipLevels = new_texture_info.resource->GetDesc().MipLevels;
    srv_desc.TextureCube.MostDetailedMip = 0;
    srv_desc.TextureCube.ResourceMinLODClamp = 0.0f;

    _device->CreateShaderResourceView(new_texture_info.resource.Get(), &srv_desc, new_texture_info.cpu_handle);

    _textures[file_path] = std::move(new_texture_info);
    return &_textures[file_path];
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////HeightMap///////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ResourceManager::TextureInfo* ResourceManager::get_texture(const std::string& file_path)
{
    auto it = _textures.find(file_path);
    if (it != _textures.end())
        return &it->second;

    CERROR("Texture not found: " << file_path);
    return nullptr;
}

ResourceManager::TextureInfo* ResourceManager::load_heightmap_from_raw(const std::string& file_path, int width, int height)
{
    auto it = _textures.find(file_path);
    if (it != _textures.end())
        return &it->second;
    // 1. 파일 읽기
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open())
    {
        CERROR("Heightmap raw file not found: " << file_path);
        return nullptr;
    }

    std::vector<unsigned short> rawData(width * height);
    file.read(reinterpret_cast<char*>(rawData.data()), width * height * 2);
    file.close();

    // 최소/최대 찾기
    unsigned short minVal = *std::min_element(rawData.begin(), rawData.end());
    unsigned short maxVal = *std::max_element(rawData.begin(), rawData.end());
    CLOG("  Min: " << minVal << ", Max: " << maxVal);

    // 2. 텍스처 정보 생성
    TextureInfo new_tex_info;
    new_tex_info.name = file_path;

    // 3. 리소스 생성
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R16_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    HRESULT hr = _device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&new_tex_info.resource));

    if (FAILED(hr))
    {
        CERROR("CreateCommittedResource failed for HeightMap!");
        return nullptr;
    }

    // 4. 업로드
    UINT64 uploadBufferSize = GetRequiredIntermediateSize(new_tex_info.resource.Get(), 0, 1);
    auto uploadHeapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto uploadDesc = CD3DX12_RESOURCE_DESC::Buffer(uploadBufferSize);

    _device->CreateCommittedResource(&uploadHeapProps, D3D12_HEAP_FLAG_NONE, &uploadDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&new_tex_info.upload_heap));

    D3D12_SUBRESOURCE_DATA subData = {};
    subData.pData = rawData.data();
    subData.RowPitch = width * 2;
    subData.SlicePitch = subData.RowPitch * height;

    UpdateSubresources(_command_list, new_tex_info.resource.Get(), new_tex_info.upload_heap.Get(), 0, 0, 1, &subData);

    // 5. 상태 전이
    auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(new_tex_info.resource.Get(),
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    _command_list->ResourceBarrier(1, &barrier);

    // 6. SRV 생성

    bool allocated = DescriptorManager::instance()->allocate_descriptor(
        new_tex_info.cpu_handle,
        new_tex_info.gpu_handle
    );

    if (!allocated)
    {
        CERROR("Failed to allocate descriptor for HeightMap!");
        return nullptr;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R16_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;

    _device->CreateShaderResourceView(new_tex_info.resource.Get(), &srvDesc, new_tex_info.cpu_handle);

    // 7. 맵에 저장 (move 후에도 handle 유지되는지 확인)
    _textures[file_path] = std::move(new_tex_info);

    auto* stored = &_textures[file_path];

    return stored;
}
