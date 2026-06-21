#include "stdafx.h"
#include "ResourceManager.h"
#include "LightManager.h"
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

        new_tex_info.current_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

        if (!DescriptorManager::instance()->allocate_descriptor(new_tex_info.cpu_handle, new_tex_info.gpu_handle)) { CERROR("Failed to allocate descriptor for default texture: " + name); return; }

        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.Format = tex_desc.Format;
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Texture2D.MipLevels = 1;
        device->CreateShaderResourceView(new_tex_info.resource.Get(), &srv_desc, new_tex_info.cpu_handle);

        _textures[name] = std::move(new_tex_info);
    };

    uint8_t white_pixel[4] = { 255, 255, 255, 255 }; // White
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

    // Skybox/IBL 전용 SHADER_VISIBLE 힙 생성 (4개: skybox + 3 IBL)
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
    heap_desc.NumDescriptors = 4;  // Skybox + IBL 3개
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // [변경] SHADER_VISIBLE 제거 -> CPU 전용으로 사용
    heap_desc.NodeMask = 0;

    HRESULT hr = _device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&_static_srv_heap));
    if (FAILED(hr))
    {
        CERROR("Failed to create static SRV heap!");
        return;
    }

    _static_heap_descriptor_size = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

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

void ResourceManager::process_pending_uploads(ID3D12Device* device, ID3D12GraphicsCommandList* command_list, UINT64 targetFenceValue, size_t maxCount)
{
    if (_pending_meshes.empty()) return;

    int uploadCount = 0;
    while (!_pending_meshes.empty() && uploadCount < maxCount)
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

void ResourceManager::unload_texture(const std::string& name)
{
    auto it = _textures.find(name);
    if (it != _textures.end())
    {
        if (it->second.cpu_handle.ptr != 0)
        {
            DescriptorManager::instance()->free_descriptor(it->second.cpu_handle, it->second.gpu_handle);
        }
        _textures.erase(it);
    }
}

// 내부 헬퍼 함수: 텍스처를 로드하고 GPU에 업로드합니다.
ResourceManager::TextureInfo * ResourceManager::load_texture(const std::string & file_path, bool is_srgb, D3D12_SRV_DIMENSION view_dimension)
{
    if (file_path.empty()) {
        return nullptr;
    }
    
	// 1. 항상 원본 파일 경로를 키로 사용하여 캐시를 확인
    auto it = _textures.find(file_path);
    if (it != _textures.end()) {
       // CLOG("Texture cache hit for: " << file_path);
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
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
    _command_list->ResourceBarrier(1, &transition);
    new_texture_info.current_state = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;

    // 7. 셰이더 리소스 뷰(SRV)를 생성합니다. -> 텍스쳐 등록
    if (!DescriptorManager::instance()->allocate_descriptor(new_texture_info.cpu_handle, new_texture_info.gpu_handle))
    {
        CERROR("Failed to allocate descriptor for texture: " << file_path);
        return nullptr;
    }
    
    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    DXGI_FORMAT resource_format = new_texture_info.resource->GetDesc().Format;


    if (is_srgb) {
        // 컬러용 (BaseColor): sRGB 포맷 사용
        if (resource_format == DXGI_FORMAT_R8G8B8A8_UNORM) resource_format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        else if (resource_format == DXGI_FORMAT_BC1_UNORM) resource_format = DXGI_FORMAT_BC1_UNORM_SRGB;
        else if (resource_format == DXGI_FORMAT_BC2_UNORM) resource_format = DXGI_FORMAT_BC2_UNORM_SRGB;
        else if (resource_format == DXGI_FORMAT_BC3_UNORM) resource_format = DXGI_FORMAT_BC3_UNORM_SRGB;
        else if (resource_format == DXGI_FORMAT_BC7_UNORM) resource_format = DXGI_FORMAT_BC7_UNORM_SRGB;
    }
    else {
        // 데이터용 (ORM, Normal): Linear 포맷 사용 (sRGB 제거)
        if (resource_format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB) resource_format = DXGI_FORMAT_R8G8B8A8_UNORM;
        else if (resource_format == DXGI_FORMAT_BC1_UNORM_SRGB) resource_format = DXGI_FORMAT_BC1_UNORM;
        else if (resource_format == DXGI_FORMAT_BC2_UNORM_SRGB) resource_format = DXGI_FORMAT_BC2_UNORM;
        else if (resource_format == DXGI_FORMAT_BC3_UNORM_SRGB) resource_format = DXGI_FORMAT_BC3_UNORM;
        else if (resource_format == DXGI_FORMAT_BC7_UNORM_SRGB) resource_format = DXGI_FORMAT_BC7_UNORM;
    }

    srv_desc.Format = resource_format;
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
        int mat_index = 0;
        for (const auto& mat_json : gltf_json["materials"]) {
            std::string default_name = "UnnamedMaterial_" + std::filesystem::path(file_path).stem().string() + "_" + std::to_string(mat_index);
            std::string mat_name = mat_json.value("name", default_name);
    
            create_material(mat_name);
            MaterialInfo & new_mat_info = _materials[mat_name];
            mat_index++;
    
            const auto& pbr = mat_json.value("pbrMetallicRoughness", json::object());
    
            // Factors
            if (pbr.contains("baseColorFactor")) {
                new_mat_info.base_color_factor = { pbr["baseColorFactor"][0], pbr["baseColorFactor"][1], pbr["baseColorFactor"][2], pbr["baseColorFactor"][3] };
            }
            new_mat_info.metallic_factor = pbr.value("metallicFactor", 1.0f);
            new_mat_info.roughness_factor = pbr.value("roughnessFactor", 0.7f);
            if (mat_json.contains("emissiveFactor")) {
                    new_mat_info.emissive_factor = { mat_json["emissiveFactor"][0], mat_json["emissiveFactor"][1], mat_json["emissiveFactor"][2] };
            }
            // Textures
            auto assign_texture = [&](const json& texture_info, std::string& path_member, bool is_srgb,
                int& uv_channel, XMFLOAT2& uv_offset, XMFLOAT2& uv_scale, float& uv_rotation) {

            	uv_channel = texture_info.value("texCoord", 0);

                if (texture_info.contains("index")) {
                        int tex_idx = texture_info["index"];
                        if (tex_idx < texture_source_uris.size() && !texture_source_uris[tex_idx].empty()) {
                            //gltf의 상대 경로 및 기본 경로를 조합하여 전체 경로로 만듦
                            std::filesystem::path full_texture_path = base_path / texture_source_uris[tex_idx];
                            path_member = full_texture_path.string();

							// 이제 DDS 우선 로드 및 실패 시 WIC 로드를 수행
                            load_texture(path_member, is_srgb);
                        }
                }
                // KHR_texture_transform 파싱
                if (texture_info.contains("extensions")) {
                    const auto& ext = texture_info["extensions"];
                    if (ext.contains("KHR_texture_transform")) {
                        const auto& transform = ext["KHR_texture_transform"];

                        if (transform.contains("offset")) {
                            uv_offset.x = transform["offset"][0];
                            uv_offset.y = transform["offset"][1];
                        }
                        if (transform.contains("scale")) {
                            uv_scale.x = transform["scale"][0];
                            uv_scale.y = transform["scale"][1];
                        }
                        if (transform.contains("rotation")) {
                            uv_rotation = transform["rotation"];
                        }
                    }
                }
            };

            if (pbr.contains("baseColorTexture"))
                assign_texture(pbr["baseColorTexture"],
                    new_mat_info.base_color_texture_path,
                    true,
					new_mat_info.base_color_uv_channel,
                    new_mat_info.base_color_uv_offset,
                    new_mat_info.base_color_uv_scale,
                    new_mat_info.base_color_uv_rotation);

            if (pbr.contains("metallicRoughnessTexture"))
                assign_texture(pbr["metallicRoughnessTexture"],
                    new_mat_info.metallic_roughness_texture_path,
                    false,
					new_mat_info.metallic_roughness_uv_channel,
                    new_mat_info.metallic_roughness_uv_offset,
                    new_mat_info.metallic_roughness_uv_scale,
                    new_mat_info.metallic_roughness_uv_rotation);

            if (mat_json.contains("normalTexture")) {
                assign_texture(mat_json["normalTexture"],
                    new_mat_info.normal_texture_path,
                    false,
					new_mat_info.normal_uv_channel,
                    new_mat_info.normal_uv_offset,
                    new_mat_info.normal_uv_scale,
                    new_mat_info.normal_uv_rotation);
                new_mat_info.normal_texture_scale = mat_json["normalTexture"].value("scale", 1.0f);
            }

            // 546번 라인 근처 (emissiveTexture UV 버그 수정)
            if (mat_json.contains("emissiveTexture"))
                assign_texture(mat_json["emissiveTexture"],
                    new_mat_info.emissive_texture_path,
                    true,
                    new_mat_info.emissive_uv_channel,
                    new_mat_info.emissive_uv_offset,
                    new_mat_info.emissive_uv_scale,
                    new_mat_info.emissive_uv_rotation);

            if (mat_json.contains("occlusionTexture"))
                assign_texture(mat_json["occlusionTexture"],
                    new_mat_info.occlusion_texture_path,
                    false,
					new_mat_info.metallic_roughness_uv_channel,
                    new_mat_info.metallic_roughness_uv_offset,
                    new_mat_info.metallic_roughness_uv_scale,
                    new_mat_info.metallic_roughness_uv_rotation);
            
            // Other properties
            new_mat_info.alpha_mode = mat_json.value("alphaMode", "OPAQUE");
            new_mat_info.alpha_cutoff = mat_json.value("alphaCutoff", 0.5f);
            new_mat_info.double_sided = mat_json.value("doubleSided", false);

            // KHR_materials_specular extension 파싱
            // glTF specularFactor(0~1, 기본 1.0) -> UE4 스케일(0~1, 기본 0.5) 변환
            if (mat_json.contains("extensions")) {
                const auto& ext = mat_json["extensions"];
                if (ext.contains("KHR_materials_specular")) {
                    float gltf_specular = ext["KHR_materials_specular"].value("specularFactor", 1.0f);
                    new_mat_info.specular_factor = gltf_specular;
                }
            }
            
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
        load_texture(texture_path, true);
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
    // Emissive(RGB) + Metallic(A)를 하나의 XMFLOAT4에 담음
    constants.EmissiveAndMetallicFactor = XMFLOAT4(
        mat_info.emissive_factor.x,
        mat_info.emissive_factor.y,
        mat_info.emissive_factor.z,
        mat_info.metallic_factor
    );
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
    constants.HasOcclusionTexture = !mat_info.occlusion_texture_path.empty();
    constants.SpecularFactor = mat_info.specular_factor;

    constants.BaseColorUVChannel = mat_info.base_color_uv_channel;
    constants.NormalUVChannel = mat_info.normal_uv_channel;
    constants.MetallicRoughnessUVChannel = mat_info.metallic_roughness_uv_channel;
    constants.EmissiveUVChannel = mat_info.emissive_uv_channel;

    // UV Transform 값 복사 추가
    constants.BaseColorUVOffset = mat_info.base_color_uv_offset;
    constants.BaseColorUVScale = mat_info.base_color_uv_scale;
    constants.BaseColorUVRotation = mat_info.base_color_uv_rotation;

    constants.NormalUVOffset = mat_info.normal_uv_offset;
    constants.NormalUVScale = mat_info.normal_uv_scale;
    constants.NormalUVRotation = mat_info.normal_uv_rotation;

    constants.MetallicRoughnessUVOffset = mat_info.metallic_roughness_uv_offset;
    constants.MetallicRoughnessUVScale = mat_info.metallic_roughness_uv_scale;
    constants.MetallicRoughnessUVRotation = mat_info.metallic_roughness_uv_rotation;

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

    D3D12_CPU_DESCRIPTOR_HANDLE default_white_handle = get_cpu_handle("__DEFAULT_WHITE__");
    D3D12_CPU_DESCRIPTOR_HANDLE default_normal_handle = get_cpu_handle("__DEFAULT_NORMAL__");
    D3D12_CPU_DESCRIPTOR_HANDLE default_orm_handle = get_cpu_handle("__DEFAULT_ORM__");
    D3D12_CPU_DESCRIPTOR_HANDLE default_black_handle = get_cpu_handle("__DEFAULT_BLACK__");

    // load_material_fron_gltf에서 바인딩 된 주소들이 실제 GPU를 가르키는 포인터로 바뀐다.

    D3D12_CPU_DESCRIPTOR_HANDLE base_color_handle = get_cpu_handle(mat_info.base_color_texture_path);
    if (base_color_handle.ptr == 0) base_color_handle = default_white_handle;

    D3D12_CPU_DESCRIPTOR_HANDLE normal_handle = get_cpu_handle(mat_info.normal_texture_path);
    if (normal_handle.ptr == 0) normal_handle = default_normal_handle;
    
    D3D12_CPU_DESCRIPTOR_HANDLE orm_handle = get_cpu_handle(mat_info.metallic_roughness_texture_path);
    if (orm_handle.ptr == 0) orm_handle = default_orm_handle;

    D3D12_CPU_DESCRIPTOR_HANDLE emissive_handle = get_cpu_handle(mat_info.emissive_texture_path);
    if (emissive_handle.ptr == 0) emissive_handle = default_black_handle;

    renderer->bind_texture_table(command_list, 4, { base_color_handle }); // t0
    renderer->bind_texture_table(command_list, 5, { normal_handle });     // t1
    renderer->bind_texture_table(command_list, 6, { orm_handle });        // t2
    renderer->bind_texture_table(command_list, 7, { emissive_handle });   // t3

    // Occlusion 전용 슬롯 (params[9] = t4)
    if (!mat_info.occlusion_texture_path.empty())
    {
        D3D12_CPU_DESCRIPTOR_HANDLE occlusion_handle =
            get_cpu_handle(mat_info.occlusion_texture_path);
        if (occlusion_handle.ptr != 0)
        {
            std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> occlusion_handles = { occlusion_handle };
            renderer->bind_texture_table(command_list, 9, occlusion_handles);
        }
    }

    // GLTF 셰이더는 IBL 텍스처가 필요함
     // GLTF 셰이더는 IBL 텍스처가 필요함
    if (mat_info.shader_name == "gltf" || mat_info.shader_name == "skinned")
    {
        // DW설명 : 해당 부분의 srv_cpu 핸들이 셰이더에서 볼 수 없는 경우가 있어 수정해줌
        D3D12_CPU_DESCRIPTOR_HANDLE dummy_handle = get_skybox_srv_cpu();
        //D3D12_CPU_DESCRIPTOR_HANDLE dummy_handle = default_black_handle;

        if (dummy_handle.ptr == 0) {
            dummy_handle = default_black_handle;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE ibl_prefiltered_handle = get_cpu_handle(_ibl_prefiltered_path);
        D3D12_CPU_DESCRIPTOR_HANDLE ibl_brdf_lut_handle = get_cpu_handle(_ibl_brdf_lut_path);

        if (ibl_prefiltered_handle.ptr != 0 && ibl_brdf_lut_handle.ptr != 0)
        {
            std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> ibl_handles;
            ibl_handles.push_back(dummy_handle);       // t8
            ibl_handles.push_back(ibl_prefiltered_handle); // t9
            ibl_handles.push_back(ibl_brdf_lut_handle);    // t10

            renderer->bind_texture_table(command_list, 8, ibl_handles); // params[9~10]
        }
    }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////SkyBox//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void ResourceManager::load_skybox(const std::string& file_path)
{
    _skybox_texture_path = file_path;
    auto* skybox_info = load_cubemap_from_dds(file_path);

    _skybox_cpu_handle = _static_srv_heap->GetCPUDescriptorHandleForHeapStart();

	// DW수정 : gpu주소는 heap flag를 NONE로 설정되어 gpu 주소를 받는 함수를 호출하면 에러가 발생
	// 이제 gpu 주소는 사용안함
    //_skybox_gpu_handle = _static_srv_heap->GetGPUDescriptorHandleForHeapStart();
	_skybox_gpu_handle.ptr = 0;

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Format = skybox_info->resource->GetDesc().Format;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srv_desc.TextureCube.MipLevels = skybox_info->resource->GetDesc().MipLevels;
    srv_desc.TextureCube.MostDetailedMip = 0;
    srv_desc.TextureCube.ResourceMinLODClamp = 0.0f;

    _device->CreateShaderResourceView(skybox_info->resource.Get(), &srv_desc, _skybox_cpu_handle);
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

D3D12_GPU_DESCRIPTOR_HANDLE ResourceManager::get_skybox_srv_gpu() const
{
    return _skybox_gpu_handle; 
}

D3D12_CPU_DESCRIPTOR_HANDLE ResourceManager::get_skybox_srv_cpu() const
{
    return _skybox_cpu_handle;
}

void ResourceManager::load_ibl_maps(const std::string specular_path, const std::string diffuse_path, const std::string brdf_path)
{
    // 1. Irradiance Map (인덱스 1)
    _ibl_irradiance_path = diffuse_path;
    std::ifstream sh_file(_ibl_irradiance_path);

    if (sh_file.is_open())
    {
        std::string line;
        int index = 0;

        // 파일에서 9줄을 읽어옵니다. (각 줄은 R G B 숫자로 되어있음)
        while (std::getline(sh_file, line) && index < 9)
        {
            std::stringstream ss(line);
            // sh.txt를 열어보면 괄호나 쉼표 없이 숫자만 띄어쓰기로 구분되어 있습니다.
            ss >> _ibl_sh_data.coefficients[index].x
                >> _ibl_sh_data.coefficients[index].y
                >> _ibl_sh_data.coefficients[index].z;
            _ibl_sh_data.coefficients[index].w = 1.0f;
            index++;
        }
        sh_file.close();
        LightManager::instance()->set_ibl_diffuse_sh(_ibl_sh_data.coefficients);
    }

    // 2. Prefiltered Environment Map (인덱스 2)
	_ibl_prefiltered_path = specular_path;
    auto prefiltered_info = load_cubemap_from_dds(_ibl_prefiltered_path);
    if (prefiltered_info)
    {
        _ibl_specular_cpu_handle = prefiltered_info->cpu_handle;
        _ibl_specular_gpu_handle = prefiltered_info->gpu_handle;

        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.Format = prefiltered_info->resource->GetDesc().Format;
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
        srv_desc.TextureCube.MipLevels = prefiltered_info->resource->GetDesc().MipLevels;
        srv_desc.TextureCube.MostDetailedMip = 0;
        srv_desc.TextureCube.ResourceMinLODClamp = 0.0f;
        _device->CreateShaderResourceView(prefiltered_info->resource.Get(), &srv_desc, _ibl_specular_cpu_handle);
    }

    // 3. BRDF LUT (인덱스 3, 2D 텍스처)
    _ibl_brdf_lut_path = brdf_path;
    auto brdf_info = load_texture(_ibl_brdf_lut_path, false);
    if (brdf_info)
    {
        _ibl_brdf_cpu_handle = brdf_info->cpu_handle;
        _ibl_brdf_gpu_handle = brdf_info->gpu_handle;

        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.Format = brdf_info->resource->GetDesc().Format;
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Texture2D.MipLevels = brdf_info->resource->GetDesc().MipLevels;
        srv_desc.Texture2D.MostDetailedMip = 0;
        srv_desc.Texture2D.ResourceMinLODClamp = 0.0f;
        _device->CreateShaderResourceView(brdf_info->resource.Get(), &srv_desc, _ibl_brdf_cpu_handle);
    }
}

D3D12_GPU_DESCRIPTOR_HANDLE ResourceManager::get_ibl_irradiance_srv()
{
    // ========== 방법 1: _ibl_irradiance_path로 찾기 ==========
    if (!_ibl_irradiance_path.empty()) {
        auto it = _textures.find(_ibl_irradiance_path);
        if (it != _textures.end()) {
            return it->second.gpu_handle;
        }
    }

    // ========== 방법 2: 직접 경로로 찾기 (fallback) ==========
    auto it2 = _textures.find("Resource\\SkyBox\\IBL_diffuse.dds");
    if (it2 != _textures.end()) {
        CLOG("Found via direct path!");
        return it2->second.gpu_handle;
    }

    // ========== 방법 3: 맵 전체 검색 ==========
    for (const auto& [path, tex] : _textures) {
        if (path.find("IBL_diffuse") != std::string::npos) {
            CLOG("Found via search: " << path);
            return tex.gpu_handle;
        }
    }

    CERROR("IBL Irradiance not found anywhere!");
    CLOG("_ibl_irradiance_path = '" << _ibl_irradiance_path << "'");
    return {};
}

//동일하게** get_ibl_prefiltered_srv()** 와** get_ibl_brdf_lut_srv()** 도 수정 :

D3D12_GPU_DESCRIPTOR_HANDLE ResourceManager::get_ibl_prefiltered_srv()
{
    if (!_ibl_prefiltered_path.empty()) {
        auto it = _textures.find(_ibl_prefiltered_path);
        if (it != _textures.end()) return it->second.gpu_handle;
    }

    auto it2 = _textures.find("Resource\\SkyBox\\IBL_specular.dds");
    if (it2 != _textures.end()) return it2->second.gpu_handle;

    for (const auto& [path, tex] : _textures) {
        if (path.find("IBL_specular") != std::string::npos) {
            return tex.gpu_handle;
        }
    }

    CERROR("IBL Prefiltered not found!");
    return {};
}

D3D12_GPU_DESCRIPTOR_HANDLE ResourceManager::get_ibl_brdf_lut_srv()
{
    if (!_ibl_brdf_lut_path.empty()) {
        auto it = _textures.find(_ibl_brdf_lut_path);
        if (it != _textures.end()) return it->second.gpu_handle;
    }

    auto it2 = _textures.find("Resource\\SkyBox\\IBL_BRDF_LUT.dds");
    if (it2 != _textures.end()) return it2->second.gpu_handle;

    for (const auto& [path, tex] : _textures) {
        if (path.find("IBL_BRDF_LUT") != std::string::npos) {
            return tex.gpu_handle;
        }
    }

    CERROR("IBL BRDF LUT not found!");
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

    new_texture_info.current_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

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
        D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
    _command_list->ResourceBarrier(1, &barrier);

    new_tex_info.current_state = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;

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

ResourceManager::TextureInfo* ResourceManager::load_texture_r8(const std::string& file_path, int width, int height)
{
    // 1. 캐시 확인
    auto it = _textures.find(file_path);
    if (it != _textures.end())
        return &it->second;

    // 2. 파일 읽기
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open())
    {
        CERROR("R8 texture file not found: " << file_path);
        return nullptr;
    }

    // 3. 데이터 크기 계산 및 읽기
    size_t total_bytes = static_cast<size_t>(width) * height * 1; // R8= 1 byte per pixel
	std::vector<uint8_t> pixel_data(total_bytes);

    file.read(reinterpret_cast<char*>(pixel_data.data()), total_bytes);
    file.close();

    if (file.gcount() != static_cast<std::streamsize>(total_bytes))
    {
        CERROR("R8 file size mismatch: " << file_path
            << " (expected: " << total_bytes << " bytes)");
        return nullptr;
    }

    // 4. GPU 텍스처 리소스 생성 (DXGI_FORMAT_R8_UNORM)
    TextureInfo new_texture_info;
    new_texture_info.name = file_path;

    D3D12_RESOURCE_DESC texture_desc = {};
    texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texture_desc.Width = width;
    texture_desc.Height = height;
    texture_desc.DepthOrArraySize = 1;
    texture_desc.MipLevels = 1;
    texture_desc.Format = DXGI_FORMAT_R8_UNORM; // R8 포맷
    texture_desc.SampleDesc.Count = 1;
    texture_desc.SampleDesc.Quality = 0;
    texture_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texture_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    auto default_heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    HRESULT hr = _device->CreateCommittedResource(
        &default_heap_props,
        D3D12_HEAP_FLAG_NONE,
        &texture_desc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&new_texture_info.resource)
    );

    if (FAILED(hr))
    {
        CERROR("Failed to create R8 texture resource: " << file_path);
        return nullptr;
    }

    // 5. 업로드 힙 생성 및 데이터 복사
    D3D12_SUBRESOURCE_DATA subresource_data = {};
    subresource_data.pData = pixel_data.data();
    subresource_data.RowPitch = width * 1; // R8 = 1 byte
    subresource_data.SlicePitch = total_bytes;

    const UINT64 upload_buffer_size = GetRequiredIntermediateSize(
        new_texture_info.resource.Get(),
        0,
        1
    );

    auto upload_heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto upload_buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(upload_buffer_size);

    hr = _device->CreateCommittedResource(
        &upload_heap_props,
        D3D12_HEAP_FLAG_NONE,
        &upload_buffer_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&new_texture_info.upload_heap)
    );

    if (FAILED(hr))
    {
        CERROR("Failed to create upload heap for R8 texture: " <<
            file_path);
        return nullptr;
    }

    // 6. GPU 업로드 (CommandList 필요)
    if (_command_list)
    {
        UpdateSubresources(
            _command_list,
            new_texture_info.resource.Get(),
            new_texture_info.upload_heap.Get(),
            0, 0, 1,
            &subresource_data
        );

        // Transition to Pixel Shader Resource
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            new_texture_info.resource.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        );
        _command_list->ResourceBarrier(1, &barrier);

        new_texture_info.current_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }
    else
    {
        CERROR("CommandList not set for R8 texture upload: " <<
            file_path);
        return nullptr;
    }

    // 7. SRV 생성
    if (!DescriptorManager::instance()->allocate_descriptor(
        new_texture_info.cpu_handle,
        new_texture_info.gpu_handle))
    {
        CERROR("Failed to allocate descriptor for R8 texture: " <<
            file_path);
        return nullptr;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Format = DXGI_FORMAT_R8_UNORM;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Texture2D.MipLevels = 1;

    _device->CreateShaderResourceView(
        new_texture_info.resource.Get(),
        &srv_desc,
        new_texture_info.cpu_handle
    );

    // 8. 캐시에 저장
    _textures[file_path] = new_texture_info;

    return &_textures[file_path];
}

ResourceManager::TextureInfo* ResourceManager::create_texture_array_r8(const std::string& array_name, const std::vector<std::string>& file_paths, int width, int height)
{
    // 1. 캐시 확인
    auto it = _textures.find(array_name);
    if (it != _textures.end())
        return &it->second;

    if (file_paths.empty())
    {
        CERROR("No file paths provided for Texture2DArray: " <<
            array_name);
        return nullptr;
    }

    const size_t array_size = file_paths.size();
    if (array_size > 16) // 안전 장치
    {
        CERROR("Too many layers for Texture2DArray (max 16): " <<
            array_name);
        return nullptr;
    }

    // 2. 모든 R8 파일 데이터 읽기
    std::vector<std::vector<uint8_t>> all_pixel_data(array_size);
    size_t bytes_per_slice = static_cast<size_t>(width) * height * 1; // R8 = 1 byte

        for (size_t i = 0; i < array_size; ++i)
        {
            std::ifstream file(file_paths[i], std::ios::binary);
            if (!file.is_open())
            {
                CERROR("Failed to open R8 file for array: " <<
                    file_paths[i]);
                return nullptr;
            }

            all_pixel_data[i].resize(bytes_per_slice);
            file.read(reinterpret_cast<char*>(all_pixel_data[i].data()),
                bytes_per_slice);
            file.close();

            if (file.gcount() !=
                static_cast<std::streamsize>(bytes_per_slice))
            {
                CERROR("R8 file size mismatch in array: " << file_paths[i]);
                return nullptr;
            }
        }

    // 3. GPU Texture2DArray 리소스 생성
    TextureInfo new_texture_info;
    new_texture_info.name = array_name;

    D3D12_RESOURCE_DESC texture_desc = {};
    texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texture_desc.Width = width;
    texture_desc.Height = height;
    texture_desc.DepthOrArraySize = static_cast<UINT16>(array_size); // Array 크기
	texture_desc.MipLevels = 1;
    texture_desc.Format = DXGI_FORMAT_R8_UNORM;
    texture_desc.SampleDesc.Count = 1;
    texture_desc.SampleDesc.Quality = 0;
    texture_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texture_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    auto default_heap_props =
        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    HRESULT hr = _device->CreateCommittedResource(
        &default_heap_props,
        D3D12_HEAP_FLAG_NONE,
        &texture_desc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&new_texture_info.resource)
    );

    if (FAILED(hr))
    {
        CERROR("Failed to create Texture2DArray resource: " <<
            array_name);
        return nullptr;
    }

    // 4. 업로드 힙 생성
    const UINT64 upload_buffer_size = GetRequiredIntermediateSize(
        new_texture_info.resource.Get(),
        0,
        static_cast<UINT>(array_size)
    );

    auto upload_heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    auto upload_buffer_desc = CD3DX12_RESOURCE_DESC::Buffer(upload_buffer_size);

    hr = _device->CreateCommittedResource(
        &upload_heap_props,
        D3D12_HEAP_FLAG_NONE,
        &upload_buffer_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&new_texture_info.upload_heap)
    );

    if (FAILED(hr))
    {
        CERROR("Failed to create upload heap for Texture2DArray: " << array_name);
        return nullptr;
    }

    // 5. 각 슬라이스별 SubresourceData 구성
    std::vector<D3D12_SUBRESOURCE_DATA> subresources(array_size);
    for (size_t i = 0; i < array_size; ++i)
    {
        subresources[i].pData = all_pixel_data[i].data();
        subresources[i].RowPitch = width * 1; // R8 = 1 byte
        subresources[i].SlicePitch = bytes_per_slice;
    }

    // 6. GPU 업로드
    if (_command_list)
    {
        UpdateSubresources(
            _command_list,
            new_texture_info.resource.Get(),
            new_texture_info.upload_heap.Get(),
            0, 0,
            static_cast<UINT>(array_size),
            subresources.data()
        );

        // Transition to Pixel Shader Resource
        auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
            new_texture_info.resource.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        );
        _command_list->ResourceBarrier(1, &barrier);
    
        new_texture_info.current_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }
    else
    {
        CERROR("CommandList not set for Texture2DArray upload: " <<
            array_name);
        return nullptr;
    }

    // 7. SRV 생성 (Texture2DArray 뷰)
    if (!DescriptorManager::instance()->allocate_descriptor(
        new_texture_info.cpu_handle,
        new_texture_info.gpu_handle))
    {
        CERROR("Failed to allocate descriptor for Texture2DArray: " << array_name);
        return nullptr;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Shader4ComponentMapping =
        D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Format = DXGI_FORMAT_R8_UNORM;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY; // Array 뷰
        srv_desc.Texture2DArray.MipLevels = 1;
    srv_desc.Texture2DArray.FirstArraySlice = 0;
    srv_desc.Texture2DArray.ArraySize = static_cast<UINT>(array_size);

    _device->CreateShaderResourceView(
        new_texture_info.resource.Get(),
        &srv_desc,
        new_texture_info.cpu_handle
    );

    // 8. 캐시에 저장
    _textures[array_name] = new_texture_info;

	return &_textures[array_name];
}

ResourceManager::TextureInfo* ResourceManager::create_texture_array_from_loaded(const std::string& array_name, const std::vector<std::string>& texture_keys)
{
    // 1. 캐시 확인
    auto it = _textures.find(array_name);
    if (it != _textures.end())
        return &it->second;

    if (texture_keys.empty())
    {
        CERROR("No texture keys provided for Texture2DArray: " << array_name);
        return nullptr;
    }

    const size_t array_size = texture_keys.size();
    if (array_size > 16)
    {
        CERROR("Too many textures for Texture2DArray (max 16): " << array_name);
        return nullptr;
    }

    // 2. 로드된 텍스처들 가져오기
    std::vector<TextureInfo*> source_textures;
    source_textures.reserve(array_size);

    int width = 0, height = 0;
    DXGI_FORMAT source_format = DXGI_FORMAT_UNKNOWN;
    for (const auto& key : texture_keys)
    {
        auto* tex = get_texture(key);
        if (!tex || !tex->resource)
        {
            CERROR("Texture not found or not loaded: " << key);
            return nullptr;
        }
        source_textures.emplace_back(tex);

        // 첫 번째 텍스처에서 크기 가져오기
        if (width == 0)
        {
            D3D12_RESOURCE_DESC desc = tex->resource->GetDesc();
            width = static_cast<int>(desc.Width);
            height = static_cast<int>(desc.Height);
			source_format = desc.Format;
        }
    }
    if (source_format == DXGI_FORMAT_UNKNOWN)
    {
        CERROR("Could not detect source texture format for array: " << array_name);
        return nullptr;
    }


    // 3. Texture2DArray 리소스 생성
    TextureInfo new_texture_info;
    new_texture_info.name = array_name;

    D3D12_RESOURCE_DESC texture_desc = {};
    texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texture_desc.Width = width;
    texture_desc.Height = height;
    texture_desc.DepthOrArraySize = static_cast<UINT16>(array_size);
    texture_desc.MipLevels = 1;
    texture_desc.Format = source_format;
    texture_desc.SampleDesc.Count = 1;
    texture_desc.SampleDesc.Quality = 0;
    texture_desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texture_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    auto default_heap_props = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    HRESULT hr = _device->CreateCommittedResource(
        &default_heap_props,
        D3D12_HEAP_FLAG_NONE,
        &texture_desc,
        D3D12_RESOURCE_STATE_COPY_DEST,
        nullptr,
        IID_PPV_ARGS(&new_texture_info.resource)
    );

    if (FAILED(hr))
    {
        CERROR("Failed to create Texture2DArray resource: " << array_name);
        return nullptr;
    }

    // 4. 각 슬라이스에 원본 텍스처 복사
    if (_command_list)
    {
        for (size_t i = 0; i < array_size; ++i)
        {
            // ===== 저장된 current_state 사용 =====
            D3D12_RESOURCE_STATES original_state = source_textures[i]->current_state;

            // COPY_SOURCE로 전환
            auto barrier_before = CD3DX12_RESOURCE_BARRIER::Transition(
                source_textures[i]->resource.Get(),
                original_state,
                D3D12_RESOURCE_STATE_COPY_SOURCE
            );
            _command_list->ResourceBarrier(1, &barrier_before);

            // 복사
            D3D12_TEXTURE_COPY_LOCATION dst_location = {};
            dst_location.pResource = new_texture_info.resource.Get();
            dst_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            dst_location.SubresourceIndex = static_cast<UINT>(i);

            D3D12_TEXTURE_COPY_LOCATION src_location = {};
            src_location.pResource = source_textures[i]->resource.Get();
            src_location.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
            src_location.SubresourceIndex = 0;

            _command_list->CopyTextureRegion(&dst_location, 0, 0, 0, &src_location, nullptr);

            // 원래 상태로 복원
            auto barrier_after = CD3DX12_RESOURCE_BARRIER::Transition(
                source_textures[i]->resource.Get(),
                D3D12_RESOURCE_STATE_COPY_SOURCE,
                original_state
            );
            _command_list->ResourceBarrier(1, &barrier_after);

            // 상태 복원 확인
            source_textures[i]->current_state = original_state;
        }

        // 배열 전체를 PIXEL_SHADER_RESOURCE로 전환
        auto barrier_final = CD3DX12_RESOURCE_BARRIER::Transition(
            new_texture_info.resource.Get(),
            D3D12_RESOURCE_STATE_COPY_DEST,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
        );
        _command_list->ResourceBarrier(1, &barrier_final);

        // 배열 상태 저장
        new_texture_info.current_state = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    }
    else
    {
        CERROR("CommandList not set for Texture2DArray creation: " << array_name);
        return nullptr;
    }

    // 5. SRV 생성
    if (!DescriptorManager::instance()->allocate_descriptor(
        new_texture_info.cpu_handle,
        new_texture_info.gpu_handle))
    {
        CERROR("Failed to allocate descriptor for Texture2DArray: " << array_name);
        return nullptr;
    }

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Format = source_format;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
    srv_desc.Texture2DArray.MipLevels = 1;
    srv_desc.Texture2DArray.FirstArraySlice = 0;
    srv_desc.Texture2DArray.ArraySize = static_cast<UINT>(array_size);

    _device->CreateShaderResourceView(
        new_texture_info.resource.Get(),
        &srv_desc,
        new_texture_info.cpu_handle
    );

    // 6. 캐시에 저장
    _textures[array_name] = new_texture_info;

    return &_textures[array_name];
}
