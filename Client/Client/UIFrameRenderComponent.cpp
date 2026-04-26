#include "stdafx.h"
#include "RenderComponent.h"
#include "UIRenderComponent.h"
#include "UIFrameRenderComponent.h"
#include "GameFramework.h"
#include "Mesh.h"
#include "MainPlayerScript.h"
#include "Renderer.h"

class MainPlayerScript;

UIFrameRenderComponent::UIFrameRenderComponent()
{
    set_name("UIFrameRenderComponent");
    set_pso_name("ui_frame");
    set_frustum_culling_enabled(false);  // UI는 frustum culling 끔

    initialize_constant_buffers();
    initialize_quad_mesh();

    if (!_screen_info_initialized)
    {
        initialize_screen_info();
    }
}

UIFrameRenderComponent::~UIFrameRenderComponent()
{
    if (_cb_ui_frame_element && _mapped_ui_frame_element)
    {
        _cb_ui_frame_element->Unmap(0, nullptr);
        _mapped_ui_frame_element = nullptr;
    }
}

void UIFrameRenderComponent::initialize_constant_buffers()
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

    UINT buffer_size = (sizeof(cbUIFrameElement) + 255) & ~255;

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
        IID_PPV_ARGS(&_cb_ui_frame_element)
    );

    if (SUCCEEDED(hr))
    {
        _cb_ui_frame_element->Map(0, nullptr, reinterpret_cast<void**>(&_mapped_ui_frame_element));
    }
}

void UIFrameRenderComponent::render(ID3D12GraphicsCommandList* commandList, UINT frame_index)
{
    // _mesh는 부모(RenderComponent)로부터 물려받은 변수입니다.
    if (!_mesh || !_mapped_ui_frame_element) return;

    // 부모 클래스의 변수들을 그대로 사용 (이미 protected로 열려있음)
    _mapped_ui_frame_element->screen_position = _screen_position;
    _mapped_ui_frame_element->size = _size;
    _mapped_ui_frame_element->color = _color;
    _mapped_ui_frame_element->uv_offset = _uv_offset;
    _mapped_ui_frame_element->uv_scale = _uv_scale;
    _mapped_ui_frame_element->use_texture = (_texture_info != nullptr) ? 1 : 0;
	_mapped_ui_frame_element->other_player_id = static_cast<int>(_other_player_id); 
	//TODO: 가능하면 _mapped_ui_frame_element->other_player_id도 long long 으로 바꿔야함


    // 상수 버퍼 바인딩 (자신의 버퍼 주소를 전달)
    commandList->SetGraphicsRootConstantBufferView(0, _cb_screen_info->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(1, _cb_ui_frame_element->GetGPUVirtualAddress());

    if (_texture_info)
    {
        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> handles = { _texture_info->cpu_handle };
        Renderer::instance()->bind_texture_table(commandList, 2, handles);
    }

    _mesh->render(commandList);
}