#include "stdafx.h"
#include "RenderComponent.h"

#include "GameFramework.h"
#include "TransformComponent.h"
#include "GameObject.h"
#include "Renderer.h"
#include "Shader.h"

Material_Shader::Material_Shader()
{
}
Material_Shader::~Material_Shader()
{
}

void Material_Shader::set_shader(const std::shared_ptr<Shader>& shader)
{
	_shader = shader;
}

void Material_Shader::set_shader_root_signature(ComPtr<ID3D12RootSignature> root_signature)
{
	if (_shader)
	{
		_rootSignature = root_signature;
	}
}

void Material_Shader::set_root_signature(ComPtr<ID3D12GraphicsCommandList> command_list)
{
	if (_rootSignature)
	{
		command_list->SetGraphicsRootSignature(_rootSignature.Get());
	}
}
//------------------------------------------------------------ RenderComponent ------------------------------------------------------------//

RenderComponent::RenderComponent()
{
	set_name("RenderComponent");
}
void RenderComponent::awake()
{
    ComPtr<ID3D12Device> device = GameFramework::Instance()->device();
    if (!device)
    {
        CERROR("RenderComponent::awake: Device를 가져올 수 없습니다.");
        return;
    }

    // 2. 상수 버퍼의 힙(Heap) 속성을 정의합니다.
    // CPU에서 매 프레임 업데이트해야 하므로, CPU 쓰기 및 GPU 읽기가 모두 가능한 UPLOAD 힙 타입 사용합니다.
	D3D12_HEAP_PROPERTIES heap_props = {};
    heap_props.Type = D3D12_HEAP_TYPE_UPLOAD;
    heap_props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heap_props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heap_props.CreationNodeMask = 1;
    heap_props.VisibleNodeMask = 1;

    // 3. 상수 버퍼의 리소스(Resource) 속성을 정의합니다.
    // D3D12의 규칙에 따라, 상수 버퍼의 크기는 반드시 256바이트의 배수여야 합니다.
    UINT buffer_size = (sizeof(CbGameObjectInfo) + 255) & ~255;

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

    // 4. 위에서 정의한 속성들을 사용하여 실제 상수 버퍼 리소스를 생성합니다.
    HRESULT hr = device->CreateCommittedResource(
        &heap_props,                    // 힙 속성
        D3D12_HEAP_FLAG_NONE,
        &resource_desc,                 // 리소스 속성
        D3D12_RESOURCE_STATE_GENERIC_READ, // 업로드 힙의 초기 상태는 GENERIC_READ 입니다.
        nullptr,                        // 최적화된 초기화 값 없음
        IID_PPV_ARGS(&_cbGameObjectInfo)); // 생성된 리소스는 _cbGameObjectInfo 멤버에 저장됩니다.

    if (FAILED(hr))
    {
        CERROR("RenderComponent: 상수 버퍼 생성에 실패했습니다.");
        return;
    }

    // 5. 생성된 리소스의 가상 주소를 CPU가 쓰기 가능하도록 영구적으로 맵핑(mapping)합니다.
    //    _mappedCbGameObjectInfo 포인터를 통해 CPU에서 이 버퍼에 데이터를 쓸 수 있게 됩니다.
    //    (업로드 힙에 생성한 리소스는 Unmap을 호출할 필요가 없습니다.)
    _cbGameObjectInfo->Map(0, nullptr, reinterpret_cast<void**>(&_mappedCbGameObjectInfo));
	
}

BoundingOrientedBox RenderComponent::get_world_bounding_box() const
{
    BoundingOrientedBox obb = _mesh->bounding_box();
    if (game_object() && game_object()->transform())
    {
        XMMATRIX worldMatrix = XMLoadFloat4x4(&game_object()->transform()->world_matrix());
        obb.Transform(obb, worldMatrix);
    }
    return obb;
}
bool RenderComponent::is_visible(const BoundingFrustum& frustum) const
{
    if (!_mesh) return false;
    return frustum.Intersects(get_world_bounding_box());
}



void RenderComponent::render(ID3D12GraphicsCommandList* commandList)
{
	// _shader 검사는 더 이상 필요 없습니다.
	if (!_mesh)
	{
		CERROR("메쉬가 렌더 컴포넌트에 없음");
		return;
	}

	// 1. 이 GameObject의 월드 행렬을 가져옵니다.
	const XMFLOAT4X4& worldMatrix = game_object()->transform()->world_matrix();

	// 2. [변경] 이 RenderComponent가 소유한 상수 버퍼의 내용을 월드 행렬로 업데이트합니다.
	_mappedCbGameObjectInfo->_world = worldMatrix;

	// 3. [추가] 업데이트된 상수 버퍼를 루트 시그니처의 0번 슬롯에 직접 바인딩합니다.
	// (루트 시그니처 0번 슬롯은 월드 행렬용 CBV로 미리 약속되어 있습니다)
	commandList->SetGraphicsRootConstantBufferView(0, _cbGameObjectInfo->GetGPUVirtualAddress());

	// 4. 메시를 그립니다.
	_mesh->render(commandList);
}
