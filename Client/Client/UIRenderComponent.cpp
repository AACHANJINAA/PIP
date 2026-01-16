#include "stdafx.h"
#include "UIRenderComponent.h"
#include "GameFramework.h"
#include "ResourceManager.h"
#include "Renderer.h"
#include "Mesh.h"

// static 멤버 초기화
ComPtr<ID3D12Resource> UIRenderComponent::_cb_screen_info = nullptr;
CbScreenInfo* UIRenderComponent::_mapped_screen_info = nullptr;
bool UIRenderComponent::_screen_info_initialized = false;

UIRenderComponent::UIRenderComponent()
{
    set_name("UIRenderComponent");
    set_pso_name("ui");
    set_frustum_culling_enabled(false);  // UI는 frustum culling 끔

    initialize_constant_buffers();
    initialize_quad_mesh();

    if (!_screen_info_initialized)
    {
        initialize_screen_info();
    }
}

UIRenderComponent::~UIRenderComponent()
{
    if (_cb_ui_element && _mapped_ui_element)
    {
        _cb_ui_element->Unmap(0, nullptr);
        _mapped_ui_element = nullptr;
    }
}

void UIRenderComponent::initialize_constant_buffers()
{
    auto device = GameFramework::instance()->device();
    if (!device) return;

    // UI 요소 상수 버퍼 생성
    D3D12_HEAP_PROPERTIES heap_props = {};
    heap_props.Type = D3D12_HEAP_TYPE_UPLOAD;
    heap_props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heap_props.CreationNodeMask = 1;
    heap_props.VisibleNodeMask = 1;

    UINT buffer_size = (sizeof(CbUIElement) + 255) & ~255;

    D3D12_RESOURCE_DESC resource_desc = {};
    resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resource_desc.Alignment = 0;
    resource_desc.Width = buffer_size;
    resource_desc.Height = 1;
    resource_desc.DepthOrArraySize = 1;
    resource_desc.MipLevels = 1;
    resource_desc.Format = DXGI_FORMAT_UNKNOWN;
    resource_desc.SampleDesc.Count = 1;
    resource_desc.SampleDesc.Quality = 0;
    resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    HRESULT hr = device->CreateCommittedResource(
        &heap_props,
        D3D12_HEAP_FLAG_NONE,
        &resource_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&_cb_ui_element)
    );

    if (SUCCEEDED(hr))
    {
        _cb_ui_element->Map(0, nullptr, reinterpret_cast<void**>(&_mapped_ui_element));
    }
}

void UIRenderComponent::initialize_screen_info()
{
    auto device = GameFramework::instance()->device();
    if (!device) return;

    D3D12_HEAP_PROPERTIES heap_props = {};
    heap_props.Type = D3D12_HEAP_TYPE_UPLOAD;
    heap_props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heap_props.CreationNodeMask = 1;
    heap_props.VisibleNodeMask = 1;

    UINT buffer_size = (sizeof(CbScreenInfo) + 255) & ~255;

    D3D12_RESOURCE_DESC resource_desc = {};
    resource_desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resource_desc.Alignment = 0;
    resource_desc.Width = buffer_size;
    resource_desc.Height = 1;
    resource_desc.DepthOrArraySize = 1;
    resource_desc.MipLevels = 1;
    resource_desc.Format = DXGI_FORMAT_UNKNOWN;
    resource_desc.SampleDesc.Count = 1;
    resource_desc.SampleDesc.Quality = 0;
    resource_desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resource_desc.Flags = D3D12_RESOURCE_FLAG_NONE;

    HRESULT hr = device->CreateCommittedResource(
        &heap_props,
        D3D12_HEAP_FLAG_NONE,
        &resource_desc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&_cb_screen_info)
    );

    if (SUCCEEDED(hr))
    {
        _cb_screen_info->Map(0, nullptr, reinterpret_cast<void**>(&_mapped_screen_info));

        // 화면 크기 설정 (GameFramework에서 가져올 수 있으면 가져오기)
        _mapped_screen_info->screen_width = 1920.0f;  // TODO: 실제 화면 크기로 수정
        _mapped_screen_info->screen_height = 1080.0f;
        _mapped_screen_info->padding[0] = 0.0f;
        _mapped_screen_info->padding[1] = 0.0f;

        _screen_info_initialized = true;
    }
}

void UIRenderComponent::initialize_quad_mesh()
{
    // UIQuadMesh 생성
    auto mesh = std::make_shared<UIQuadMesh>();

    // 즉시 GPU 업로드
    auto device = GameFramework::instance()->device();
    auto commandList = GameFramework::instance()->command_list();
    if (device && commandList)
    {
        mesh->upload_to_gpu(device.Get(), commandList.Get(), 0);
    }

    set_mesh(mesh);
}
void UIRenderComponent::set_texture(const std::string& texture_path)
{
    _texture_info = ResourceManager::instance()->load_texture(texture_path);
}

void UIRenderComponent::render(ID3D12GraphicsCommandList* commandList, UINT frame_index)
{
    if (!_mesh || !_mapped_ui_element) return;

    // UI 요소 데이터 업데이트
    _mapped_ui_element->screen_position = _screen_position;
    _mapped_ui_element->size = _size;
    _mapped_ui_element->color = _color;
    _mapped_ui_element->uv_offset = _uv_offset;
    _mapped_ui_element->uv_scale = _uv_scale;

    // 상수 버퍼 바인딩
    commandList->SetGraphicsRootConstantBufferView(0, _cb_screen_info->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(1, _cb_ui_element->GetGPUVirtualAddress());

    // 텍스처 바인딩
    if (_texture_info)
    {
        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> handles = { _texture_info->cpu_handle };
        Renderer::instance()->bind_texture_table(commandList, 2, handles);
    }

    // 메시 렌더링
    _mesh->render(commandList);
}