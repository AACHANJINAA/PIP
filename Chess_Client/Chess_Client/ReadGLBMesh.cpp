#include "stdafx.h"
#include "ReadGlbMesh.h"
#include "ResourceManager.h" // SRV 할당을 위해 포함

// [변경] 생성자: 파일 파싱 및 CPU 데이터 저장만 담당
ReadGlbMesh::ReadGlbMesh(const std::string& file_path)
{
    set_name(file_path);

    // --- 1단계: 파일 읽기 및 JSON/BIN 분리 ---
    std::ifstream file(file_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        CERROR("GLB 파일 열기 실패: " << file_path);
        return;
    }
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    std::vector<char> file_data(size);
    if (!file.read(file_data.data(), size)) {
        CERROR("GLB 파일 읽기 실패: " << file_path);
        file.close();
        return;
    }
    file.close();

    char* pData = file_data.data();
    uint32_t magic = *reinterpret_cast<uint32_t*>(pData); pData += 4;
    if (magic != 0x46546C67) { // "glTF"
        CERROR("유효한 GLB 파일이 아님: " << file_path);
        return;
    }
    uint32_t version = *reinterpret_cast<uint32_t*>(pData); pData += 4;
    uint32_t length = *reinterpret_cast<uint32_t*>(pData); pData += 4;

    std::string json_string;
    while (pData < file_data.data() + length) {
        uint32_t chunkLength = *reinterpret_cast<uint32_t*>(pData); pData += 4;
        uint32_t chunkType = *reinterpret_cast<uint32_t*>(pData); pData += 4;
        if (chunkType == 0x4E4F534A) { // "JSON"
            json_string.assign(pData, chunkLength);
        }
        else if (chunkType == 0x004E4942) { // "BIN"
            _binary_data.assign(pData, pData + chunkLength);
        }
        pData += chunkLength;
    }

    try {
        _json_data = nlohmann::json::parse(json_string);
    }
    catch (nlohmann::json::parse_error& e) {
        CERROR("GLB JSON 파싱 실패: " << e.what());
        return;
    }

    // --- 2단계: 노드 계층 구조 파싱 ---
    if (_json_data.contains("nodes")) {
        const auto& nodes_json = _json_data["nodes"];
        _nodes.resize(nodes_json.size());
        for (size_t i = 0; i < nodes_json.size(); ++i) {
            const auto& node_json = nodes_json[i];
            Node& current_node = _nodes[i];

            if (node_json.contains("name")) current_node.name = node_json["name"];
            if (node_json.contains("mesh")) current_node.meshIndex = node_json["mesh"];
            if (node_json.contains("skin")) current_node.skinIndex = node_json["skin"];

            if (node_json.contains("translation")) {
                current_node.translation = XMFLOAT3(node_json["translation"][0],
                    node_json["translation"][1], node_json["translation"][2]);
            }
            if (node_json.contains("rotation")) {
                current_node.rotation = XMFLOAT4(node_json["rotation"][0], node_json["rotation"][1],
                    node_json["rotation"][2], node_json["rotation"][3]);
            }
            if (node_json.contains("scale")) {
                current_node.scale = XMFLOAT3(node_json["scale"][0], node_json["scale"][1],
                    node_json["scale"][2]);
            }
            if (node_json.contains("children")) {
                for (const auto& child_index : node_json["children"]) {
                    current_node.childrenIndices.push_back(child_index);
                }
            }
        }
        for (size_t i = 0; i < _nodes.size(); ++i) {
            for (int child_index : _nodes[i].childrenIndices) {
                if (child_index >= 0 && child_index < _nodes.size()) {
                    _nodes[child_index].parentIndex = i;
                }
            }
        }
    }

    // --- 3단계: 메시 데이터 파싱 및 _cpu_data_primitives에 저장 ---
    if (!_json_data.contains("meshes")) return;

    for (const auto& mesh : _json_data["meshes"]) {
        for (const auto& primitiveJson : mesh["primitives"]) {
            GlbCpuData cpu_primitive;

            int posAccessorIndex = primitiveJson.value("/attributes/POSITION"_json_pointer, -1);
            int indicesAccessorIndex = primitiveJson.value("/indices"_json_pointer, -1);
            if (posAccessorIndex == -1 || indicesAccessorIndex == -1) continue;

            int normalAccessorIndex = primitiveJson.value("/attributes/NORMAL"_json_pointer, -1);
            int texCoordAccessorIndex = primitiveJson.value("/attributes/TEXCOORD_0"_json_pointer, -1);
            int jointAccessorIndex = primitiveJson.value("/attributes/JOINTS_0"_json_pointer, -1);
            int weightAccessorIndex = primitiveJson.value("/attributes/WEIGHTS_0"_json_pointer, -1);

            auto [positions, posCount] = get_data<XMFLOAT3>(_json_data, _binary_data, posAccessorIndex);
            if (!positions) continue;

            auto [normals, normCount] = (normalAccessorIndex != -1) ? get_data<XMFLOAT3>(_json_data,
                _binary_data, normalAccessorIndex) : std::pair<XMFLOAT3*, size_t>(nullptr, 0);
            auto [texCoords, texCount] = (texCoordAccessorIndex != -1) ? get_data<XMFLOAT2>(_json_data,
                _binary_data, texCoordAccessorIndex) : std::pair<XMFLOAT2*, size_t>(nullptr, 0);
            auto [weights, weightCount] = (weightAccessorIndex != -1) ? get_data<XMFLOAT4>(_json_data,
                _binary_data, weightAccessorIndex) : std::pair<XMFLOAT4*, size_t>(nullptr, 0);
            struct JointType { uint16_t j[4]; };
            auto [joints, jointCount] = (jointAccessorIndex != -1) ? get_data<JointType>(_json_data,
                _binary_data, jointAccessorIndex) : std::pair<JointType*, size_t>(nullptr, 0);

            cpu_primitive.vertices.resize(posCount);
            for (size_t i = 0; i < posCount; ++i) {
                cpu_primitive.vertices[i].m_xmf3Position = positions[i];
                if (normals) cpu_primitive.vertices[i].m_xmf3Normal = normals[i];
                if (texCoords) cpu_primitive.vertices[i].m_xmf2TexCoord = texCoords[i];
                if (joints) cpu_primitive.vertices[i].m_xmf4BoneIndices = XMFLOAT4((float)joints[i].j[0],
                    (float)joints[i].j[1], (float)joints[i].j[2], (float)joints[i].j[3]);
                if (weights) cpu_primitive.vertices[i].m_xmf4BoneWeights = weights[i];
            }

            const auto& indexAccessor = _json_data["accessors"][indicesAccessorIndex];
            size_t indicesCount = indexAccessor["count"];
            cpu_primitive.indices.resize(indicesCount);
            if (indexAccessor["componentType"] == 5123) { // uint16_t
                auto [indices_u16, count] = get_data<uint16_t>(_json_data, _binary_data,
                    indicesAccessorIndex);
                for (size_t i = 0; i < count; ++i) cpu_primitive.indices[i] = indices_u16[i];
            }
            else if (indexAccessor["componentType"] == 5125) { // uint32_t
                auto [indices_u32, count] = get_data<uint32_t>(_json_data, _binary_data,
                    indicesAccessorIndex);
                memcpy(cpu_primitive.indices.data(), indices_u32, count * sizeof(uint32_t));
            }

            if (primitiveJson.contains("material")) {
                cpu_primitive.material_index = primitiveJson["material"];
            }
            _cpu_data_primitives.push_back(std::move(cpu_primitive));
        }
    }
}

// [추가] upload_to_gpu: GPU 리소스 생성 담당
void ReadGlbMesh::upload_to_gpu(ID3D12Device* device, ID3D12GraphicsCommandList* command_list)
{
    // 이미 업로드되었다면 중복 실행을 방지합니다.
    if (_isUploaded) return;

    // GPU에 데이터 복사가 완료될 때까지 살아있어야 하는 임시 업로드 버퍼들을 관리합니다.
    std::vector<ComPtr<ID3D12Resource>> temp_upload_buffers;

    // CPU에 저장해둔 데이터를 기반으로 각 primitive의 GPU 리소스를 생성합니다.
    for (const auto& cpu_primitive : _cpu_data_primitives)
    {
        auto gpu_primitive = std::make_unique<MeshPrimitive>();

        ComPtr<ID3D12Resource> vertex_upload_buffer;
        ComPtr<ID3D12Resource> index_upload_buffer;

        // --- 1. Vertex Buffer 생성 ---
        if (!cpu_primitive.vertices.empty())
        {
            UINT vertex_buffer_size = sizeof(SkinnedVertex) * cpu_primitive.vertices.size();
            void* vertex_data = const_cast<void*>(static_cast<const void*>(cpu_primitive.vertices.data()));
            gpu_primitive->_d3dVertexBuffer = ::CreateBufferResource(
                device,
                command_list,
                vertex_data,
                vertex_buffer_size,
                D3D12_HEAP_TYPE_DEFAULT,
                D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
                vertex_upload_buffer.GetAddressOf() // 수정된 부분
            );

            gpu_primitive->_d3dVertexBufferView.BufferLocation =
                gpu_primitive->_d3dVertexBuffer->GetGPUVirtualAddress();
            gpu_primitive->_d3dVertexBufferView.StrideInBytes = sizeof(SkinnedVertex);
            gpu_primitive->_d3dVertexBufferView.SizeInBytes = vertex_buffer_size;
        }

        // --- 2. Index Buffer 생성 ---
        if (!cpu_primitive.indices.empty())
        {
            UINT index_buffer_size = sizeof(UINT) * cpu_primitive.indices.size();
			void* index_data = const_cast<void*>(static_cast<const void*>(cpu_primitive.indices.data()));
            gpu_primitive->m_nIndices = cpu_primitive.indices.size();
            gpu_primitive->_d3dIndexBuffer = ::CreateBufferResource(device, command_list,
                index_data, index_buffer_size, D3D12_HEAP_TYPE_DEFAULT,
                D3D12_RESOURCE_STATE_INDEX_BUFFER, &index_upload_buffer);

            gpu_primitive->_d3dIndexBufferView.BufferLocation =
                gpu_primitive->_d3dIndexBuffer->GetGPUVirtualAddress();
            gpu_primitive->_d3dIndexBufferView.Format = DXGI_FORMAT_R32_UINT;
            gpu_primitive->_d3dIndexBufferView.SizeInBytes = index_buffer_size;
        }

        if (vertex_upload_buffer) temp_upload_buffers.push_back(vertex_upload_buffer);
        if (index_upload_buffer) temp_upload_buffers.push_back(index_upload_buffer);

        // --- 3. 텍스처 생성 및 SRV 할당 ---
        if (cpu_primitive.material_index != -1 && _json_data.contains("materials"))
        {
            const auto& mat = _json_data["materials"][cpu_primitive.material_index];
            if (mat.contains("pbrMetallicRoughness") &&
                mat["pbrMetallicRoughness"].contains("baseColorTexture"))
            {
                int texture_index = mat["pbrMetallicRoughness"]["baseColorTexture"]["index"];

                // WIC 라이브러리를 사용하여 이미지 데이터를 디코딩합니다.
                auto [pixels, width, height] = load_image_from_glb(_json_data, _binary_data,
                    texture_index);

                if (!pixels.empty())
                {
                    // 텍스처 리소스 생성
                    D3D12_RESOURCE_DESC texture_desc = {};
                    texture_desc.MipLevels = 1;
                    texture_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
                    texture_desc.Width = width;
                    texture_desc.Height = height;
                    texture_desc.Flags = D3D12_RESOURCE_FLAG_NONE;
                    texture_desc.DepthOrArraySize = 1;
                    texture_desc.SampleDesc.Count = 1;
                    texture_desc.SampleDesc.Quality = 0;
                    texture_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

                    D3D12_HEAP_PROPERTIES heap_props = {};
                    heap_props.Type = D3D12_HEAP_TYPE_DEFAULT;

                    device->CreateCommittedResource(&heap_props, D3D12_HEAP_FLAG_NONE, &texture_desc,
                        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&gpu_primitive->m_pTexture));

                    // 텍스처 데이터 업로드를 위한 임시 버퍼 생성 및 데이터 복사
                    UINT64 upload_buffer_size;
                    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
                    device->GetCopyableFootprints(&texture_desc, 0, 1, 0, &layout, nullptr, nullptr,
                        &upload_buffer_size);

                    ComPtr<ID3D12Resource> texture_upload_heap;
                    texture_upload_heap = ::CreateBufferResource(device, command_list, nullptr,
                        upload_buffer_size, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);

                    D3D12_SUBRESOURCE_DATA texture_data = {};
                    texture_data.pData = pixels.data();
                    texture_data.RowPitch = width * 4;
                    texture_data.SlicePitch = texture_data.RowPitch * height;

                    ::UpdateSubresources(command_list, gpu_primitive->m_pTexture,
                        texture_upload_heap.Get(), 0, 0, 1, &texture_data);

                    // 텍스처 리소스의 상태를 셰이더에서 읽을 수 있도록 변경
                    D3D12_RESOURCE_BARRIER barrier = {};
                    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
                    barrier.Transition.pResource = gpu_primitive->m_pTexture;
                    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
                    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
                    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
                    command_list->ResourceBarrier(1, &barrier);

                    temp_upload_buffers.push_back(texture_upload_heap);

                    // 텍스처를 위한 SRV(Shader Resource View) 생성
                    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
                    srv_desc.Format = texture_desc.Format;
                    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
                    srv_desc.Texture2D.MipLevels = 1;

                    D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle{};
                    D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle{};
                    ResourceManager::Instance()->allocate_srv_descriptor(cpu_handle, gpu_handle);

                    device->CreateShaderResourceView(gpu_primitive->m_pTexture, &srv_desc,
                        cpu_handle);
                    gpu_primitive->m_d3dGpuSrvHandle = gpu_handle;
                }
            }
        }
        _primitives.push_back(std::move(gpu_primitive));
    }

    _isUploaded = true;

    // TODO: GameFramework 등에서, 현재 CommandList의 실행이 GPU에서 완료된 후에
    // temp_upload_buffers에 있는 모든 리소스를 해제(release)하는 로직이 필요합니다.
}

// [변경] 함수 이름 수정
void ReadGlbMesh::render(ID3D12GraphicsCommandList* command_list)
{
    if (!_isUploaded) return;

    command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    for (const auto& primitive : _primitives)
    {
        if (primitive->m_pTexture && primitive->m_d3dGpuSrvHandle.ptr != 0)
        {
            // [수정] 루트 시그니처에 따라 DescriptorTable 인덱스가 달라질 수 있습니다.
            // 이 부분은 셰이더/루트시그니처 설계와 맞춰야 합니다.
            command_list->SetGraphicsRootDescriptorTable(3, primitive->m_d3dGpuSrvHandle); // 예시 인덱스
        }

        command_list->IASetVertexBuffers(0, 1, &primitive->_d3dVertexBufferView);
        command_list->IASetIndexBuffer(&primitive->_d3dIndexBufferView);
        command_list->DrawIndexedInstanced(primitive->m_nIndices, 1, 0, 0, 0);
    }
}

std::tuple<std::vector<unsigned char>, UINT, UINT> ReadGlbMesh::load_image_from_glb(const json& j,
	const std::vector<char>& binary_data, int texture_index)
{
    if (!j.contains("textures") || texture_index >= j["textures"].size()) return {};
    const auto& tex = j["textures"][texture_index];

    if (!tex.contains("source")) return {};
    int imageIndex = tex["source"];

    if (!j.contains("images") || imageIndex >= j["images"].size()) return {};
    const auto& img = j["images"][imageIndex];

    if (!img.contains("bufferView")) return {};
    int bufferViewIndex = img["bufferView"];

    if (!j.contains("bufferViews") || bufferViewIndex >= j["bufferViews"].size()) return {};
    const auto& bv = j["bufferViews"][bufferViewIndex];

    size_t byteOffset = bv["byteOffset"];
    size_t byteLength = bv["byteLength"];

    const unsigned char* pImageData = reinterpret_cast<const unsigned char*>(binary_data.data() + byteOffset);

    // WIC를 사용하여 메모리상의 이미지 데이터 디코딩
    HRESULT hr;
    IWICImagingFactory* pFactory = nullptr;
    IWICStream* pStream = nullptr;
    IWICBitmapDecoder* pDecoder = nullptr;
    IWICBitmapFrameDecode* pFrame = nullptr;
    IWICFormatConverter* pConverter = nullptr;

    CoInitialize(NULL);
    hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFactory));
    if (FAILED(hr)) { CoUninitialize(); return {}; }

    hr = pFactory->CreateStream(&pStream);
    if (SUCCEEDED(hr)) hr = pStream->InitializeFromMemory(const_cast<unsigned char*>(pImageData), static_cast<DWORD>(byteLength));
    if (SUCCEEDED(hr)) hr = pFactory->CreateDecoderFromStream(pStream, NULL, WICDecodeMetadataCacheOnDemand, &pDecoder);
    if (SUCCEEDED(hr)) hr = pDecoder->GetFrame(0, &pFrame);

    UINT width, height;
    if (SUCCEEDED(hr)) hr = pFrame->GetSize(&width, &height);

    if (SUCCEEDED(hr)) hr = pFactory->CreateFormatConverter(&pConverter);
    if (SUCCEEDED(hr)) hr = pConverter->Initialize(pFrame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, NULL, 0.f, WICBitmapPaletteTypeMedianCut);

    std::vector<unsigned char> pixels(width * height * 4);
    if (SUCCEEDED(hr)) hr = pConverter->CopyPixels(NULL, width * 4, pixels.size(), pixels.data());

    if (pConverter) pConverter->Release();
    if (pFrame) pFrame->Release();
    if (pDecoder) pDecoder->Release();
    if (pStream) pStream->Release();
    if (pFactory) pFactory->Release();
    CoUninitialize();

    if (FAILED(hr)) return {};

    return { std::move(pixels), width, height };
}
