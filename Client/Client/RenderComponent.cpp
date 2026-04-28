#include "stdafx.h"
#include "RenderComponent.h"

#include "GameFramework.h"
#include "TransformComponent.h"
#include "GameObject.h"
#include "Renderer.h"
#include "Shader.h"
#include "OtherPlayerScript.h"
#include "MainPlayerScript.h"
#include "OcclusionManager.h"

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

    ComPtr<ID3D12Device> device = GameFramework::instance()->device();
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

    for (int i = 0; i < _cbGameObjectInfo.size(); ++i)
    {
        // 4. 위에서 정의한 속성들을 사용하여 실제 상수 버퍼 리소스를 생성합니다.
        HRESULT hr = device->CreateCommittedResource(
            &heap_props,                    // 힙 속성
            D3D12_HEAP_FLAG_NONE,
            &resource_desc,                 // 리소스 속성
            D3D12_RESOURCE_STATE_GENERIC_READ, // 업로드 힙의 초기 상태는 GENERIC_READ 입니다.
            nullptr,                        // 최적화된 초기화 값 없음
            IID_PPV_ARGS(&_cbGameObjectInfo[i])); // 생성된 리소스는 _cbGameObjectInfo 멤버에 저장됩니다.

        if (FAILED(hr))
        {
            CERROR("RenderComponent: 상수 버퍼 생성에 실패했습니다.");
            return;
        }

        // 5. 생성된 리소스의 가상 주소를 CPU가 쓰기 가능하도록 영구적으로 맵핑(mapping)합니다.
        //    _mappedCbGameObjectInfo 포인터를 통해 CPU에서 이 버퍼에 데이터를 쓸 수 있게 됩니다.
        //    (업로드 힙에 생성한 리소스는 Unmap을 호출할 필요가 없습니다.)
        _cbGameObjectInfo[i]->Map(0, nullptr, reinterpret_cast<void**>(&_mappedCbGameObjectInfo[i]));
    }
}

RenderComponent::~RenderComponent()
{
	for (int i = 0; i < _cbGameObjectInfo.size(); ++i)
	{
		if (_cbGameObjectInfo[i])
		{
			_cbGameObjectInfo[i]->Unmap(0, nullptr);
			_mappedCbGameObjectInfo[i] = nullptr;
			_cbGameObjectInfo[i].Reset();
		}
	}

    if (_occlusionQueryIndex != 0xFFFFFFFF) {
        OcclusionManager::instance()->release_query_index(_occlusionQueryIndex);
    }
}


BoundingOrientedBox RenderComponent::get_world_bounding_box() const
{
    BoundingOrientedBox localObb = _mesh->bounding_box();
    BoundingOrientedBox worldObb; // 결과를 담을 별도의 변수

    if (game_object() && game_object()->transform())
    {
        XMMATRIX worldMatrix = XMLoadFloat4x4(&game_object()->transform()->world_matrix());
        localObb.Transform(worldObb, worldMatrix); // [수정] 첫 번째와 두 번째 파라미터 분리
    }
    else
    {
        worldObb = localObb;
    }

    return worldObb;
}
bool RenderComponent::is_visible(const BoundingFrustum& frustum) const
{
    if (!_mesh) return false;
    if (!_frustumCullingEnabled) return true;

    BoundingOrientedBox obb = get_world_bounding_box();

    // [박스 유효성 검사] 박스가 0이거나 깨졌으면 그리지 않음 (false)
    if (obb.Extents.x <= 0.0f || std::isnan(obb.Center.x))
    {
        return false;
    }

    return frustum.Intersects(obb);
}

void RenderComponent::pre_render(ID3D12GraphicsCommandList* commandList, class Renderer* renderer)
{
}

const std::string& RenderComponent::pso_name() const
{
    return _psoName;
}

void RenderComponent::render(ID3D12GraphicsCommandList* commandList, UINT frame_index)
{
    const XMFLOAT4X4& worldMatrixData = game_object()->transform()->world_matrix();
    XMMATRIX worldMatrix = XMLoadFloat4x4(&worldMatrixData);

    // World Matrix (Transpose해서 저장)
    XMStoreFloat4x4(&_mappedCbGameObjectInfo[frame_index]->_world, XMMatrixTranspose(worldMatrix));

    XMMATRIX worldInverse = XMMatrixInverse(nullptr, worldMatrix);
    XMStoreFloat4x4(&_mappedCbGameObjectInfo[frame_index]->_worldInverseTranspose, worldInverse);


    // OtherPlayer인지 MainPlayer인지 구분하여 ID 설정
    if (auto op_script = game_object()->get_component<OtherPlayerScript>())
    {
        // OtherPlayer: 서버에서 받은 실제 ID를 양수로 전달
        _mappedCbGameObjectInfo[frame_index]->otherplayer_id = static_cast<int>(op_script->id());
    }
    else if (auto mp_script = game_object()->get_component<MainPlayerScript>())
    {
        // MainPlayer: 음수로 표시 (셰이더에서 원본 색상 유지용)
        _mappedCbGameObjectInfo[frame_index]->otherplayer_id = -1;
    }
    else {
        // 플레이어가 아닌 오브젝트
        _mappedCbGameObjectInfo[frame_index]->otherplayer_id = -1;
    }

    // 그림자 수신 토글 - 0 : mesh 표면에 그림자 x , 1 : mesh 표면에 그림자 x <- 이 부분은 그림자 농도 조절로 해결해도될듯?
    if (_psoName == "skinned") {
        _mappedCbGameObjectInfo[frame_index]->bReceiveShadow = 0;
    }
    else {
        _mappedCbGameObjectInfo[frame_index]->bReceiveShadow = 1;
    }

    commandList->SetGraphicsRootConstantBufferView(0, _cbGameObjectInfo[frame_index]->GetGPUVirtualAddress());

    pre_render(commandList, Renderer::instance());

    _mesh->render(commandList);
}

void RenderComponent::render_CascadeShadowMap(ID3D12GraphicsCommandList* commandList, UINT frame_index)
{
    if (!_mesh) return;

    // [강화된 안전 체크]
    if (!_cbGameObjectInfo[0] || !_cbGameObjectInfo[1] ||
        !_mappedCbGameObjectInfo[0] || !_mappedCbGameObjectInfo[1])
    {
        return;  // 아직 완전히 초기화되지 않음
    }

    // frame_index 범위 체크
    if (frame_index >= 2) return;

    // 1. 오브젝트의 월드 행렬 가져오기 및 Transpose 연산
    const XMFLOAT4X4& worldMatrixData = game_object()->transform()->world_matrix();

    XMMATRIX worldMatrix = XMLoadFloat4x4(&worldMatrixData);

    XMStoreFloat4x4(&_mappedCbGameObjectInfo[frame_index]->_world, XMMatrixTranspose(worldMatrix));

    XMMATRIX worldInverse = XMMatrixInverse(nullptr, worldMatrix);
    XMStoreFloat4x4(&_mappedCbGameObjectInfo[frame_index]->_worldInverseTranspose, worldInverse);

    // 2. 루트 시그니처(b0 레지스터)에 상수 버퍼 바인딩
    commandList->SetGraphicsRootConstantBufferView(0, _cbGameObjectInfo[frame_index]->GetGPUVirtualAddress());

    // 3. 메쉬 그리기 (인스턴스 3개)
    _mesh->render_CascadeShadowMap(commandList);
}


UINT RenderComponent::get_occlusion_query_index()
{
    // 처음 호출될 때 매니저로부터 인덱스 할당
    if (_occlusionQueryIndex == 0xFFFFFFFF) {
        _occlusionQueryIndex = OcclusionManager::instance()->allocate_query_index();
        // 어떤 객체의 컴포넌트인지 이름을 같이 출력해보세요.
        if (game_object()) {
            CLOG("Allocated Index: " << _occlusionQueryIndex << " for Object: " << game_object());
        }
    }
    return _occlusionQueryIndex;
}

XMMATRIX RenderComponent::get_occlusion_box_world_matrix() {
    // 1. 객체의 로컬 바운딩 박스 정보 가져오기
    BoundingOrientedBox obb = get_world_bounding_box();

    // 2. 바운딩 박스의 중심점(Center)과 크기(Extents)를 행렬로 변환
    // 쿼리용 Unit Cube가 (-0.5~0.5) 크기라고 가정할 때:
    XMMATRIX scale = XMMatrixScaling(obb.Extents.x * 2.1f, obb.Extents.y * 2.1f, obb.Extents.z * 2.1f);
    XMMATRIX rotation = XMMatrixRotationQuaternion(XMLoadFloat4(&obb.Orientation));
    XMMATRIX translation = XMMatrixTranslation(obb.Center.x, obb.Center.y, obb.Center.z);

    // 3. 최종 박스 월드 행렬 = Scale * Rotation * Translation
    return scale * rotation * translation;
}