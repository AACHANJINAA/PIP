#include "stdafx.h"
#include "MonsterHPUIRenderComponent.h"
#include "MonsterHPComponent.h"
#include "GameFramework.h"
#include "Renderer.h"

#include "ResourceManager.h"
#include "ObjectManager.h"
#include "LayerManager.h"

MonsterHPUIRenderComponent::MonsterHPUIRenderComponent()
{
	set_pso_name("Monster_HP_UI");
    initialize_constant_buffer();
    initialize_vertex_buffer();

    // 우리 프레임워크에서 동작하기 위해 빈 깡통 메쉬 등장!
    auto mesh = std::make_shared<Mesh>();

    set_mesh(mesh);
}

MonsterHPUIRenderComponent::~MonsterHPUIRenderComponent()
{
    if (_cbResource != nullptr)
        _cbResource->Unmap(0, nullptr);

    if (_vertexBuffer != nullptr)
        _vertexBuffer->Unmap(0, nullptr);

    _cbMappedData = nullptr;
    _vbMappedData = nullptr;
}

void MonsterHPUIRenderComponent::initialize_constant_buffer()
{
    // 1. 상수 버퍼 크기 계산 (256바이트 배수 정렬 필수!)
    uint32_t cbSize = (sizeof(HPBarCB) + 255) & ~255;

    // 2. 리소스 속성 설정 (Upload Heap)
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resDesc.Width = cbSize;
    resDesc.Height = 1;
    resDesc.DepthOrArraySize = 1;
    resDesc.MipLevels = 1;
    resDesc.Format = DXGI_FORMAT_UNKNOWN;
    resDesc.SampleDesc.Count = 1;
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    // 3. 리소스 생성
    GameFramework::instance()->device().Get()->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&_cbResource)
    );

    // 최초 1회 Map (CPU 주소 획득)
    // 업로드 힙은 Unmap을 하지 않고 계속 열어두고 써도 무방
    _cbResource->Map(0, nullptr, reinterpret_cast<void**>(&_cbMappedData));

    // 기본값으로 초기 데이터 채우기
    HPBarCB initData;
    initData.size = _size;
    initData.padding[0] = 0.0f;
    initData.padding[1] = 0.0f;
    ::memcpy(_cbMappedData, &initData, sizeof(HPBarCB));
}

void MonsterHPUIRenderComponent::initialize_vertex_buffer()
{
    auto device = GameFramework::instance()->device();

    uint32_t vbSize = sizeof(HPBarVertex) * _maxMonsterCount;

    // 리소스 설정
    CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
    auto vbDesc = CD3DX12_RESOURCE_DESC::Buffer(vbSize);

    device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &vbDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&_vertexBuffer));

    _vertexBuffer->Map(0, nullptr, reinterpret_cast<void**>(&_vbMappedData));
}

void MonsterHPUIRenderComponent::upload_shader()
{
    if (_cbMappedData == nullptr)
        return;

    HPBarCB cbData;
    cbData.size = _size;
    cbData.padding[0] = 0.0f;
    cbData.padding[1] = 0.0f;

    // 멤버 변수인 _mappedData에 직접 복사
    ::memcpy(_cbMappedData, &cbData, sizeof(HPBarCB));
}


void MonsterHPUIRenderComponent::render(ID3D12GraphicsCommandList* commandList, UINT frame_index)
{
    upload_shader(); // 공통적인 ui 크기 설정 업로드

    auto allMonsters = ObjectManager::instance()->find_by_layer(LayerManager::instance()->get_layer_value("Enemy"));

    std::vector<HPBarVertex> vtxBuffer;
    vtxBuffer.reserve(allMonsters.size()); // 일단 전체 크기 만큼 예약

    UINT activeCount = 0; // 그리는 개수

    for (const auto& monsterObj : allMonsters)
    {
        auto hpComponent = monsterObj->get_component<MonsterHPComponent>();
        if (hpComponent && hpComponent.get()->get_is_changed_hp()) // hp가 바뀌었으니 표시해야 하는 경우임
        {
            if (activeCount >= _maxMonsterCount)
            {
                // 1000마리 이상은 안그림
                CLOG("HP Bar 버퍼 최대치 초과!"); // 필요 시 경고 로그
                break;
            }

            if (hpComponent.get()->is_dead()) 
            {
                continue; // 죽었으면 렌더링 안함
            }
            // 상수 버퍼 업데이트
            HPBarVertex v;
            // 몬스터 머리 위로 띄우기 위해 Y축에 오프셋(예: 2.0f) 추가
            DirectX::XMFLOAT3 pos = hpComponent.get()->game_object().get()->transform().get()->get_world_position();
			float yOffset = monsterObj->transform()->get_world_scale().y; // 필요에 따라 조절
            v.pos = { pos.x, pos.y + yOffset, pos.z };
            v.hpRatio = hpComponent.get()->get_hp_ratio();
            vtxBuffer.push_back(v);

            ++activeCount;
        }
	}

    // 그릴 게 없으면 리턴
    if (vtxBuffer.empty()) return;

    ::memcpy(_vbMappedData, vtxBuffer.data(), sizeof(HPBarVertex) * activeCount);

    // 4. 루트 시그니처 매개변수 바인딩
    // [슬롯 1] HP바 공통 상수 정보 (b2)
    commandList->SetGraphicsRootConstantBufferView(0, _cbResource->GetGPUVirtualAddress());

    // [슬롯 2] 텍스처 디스크립터 테이블 (t0: 알맹이, t1: 테두리) 바인딩
    // 텍스처를 들고 있는 ResourceManager나 별도의 핸들 관리자를 통해 바인딩하세요.
    // auto handle = _textureTable->get_gpu_handle(); 
    // commandList->SetGraphicsRootDescriptorTable(2, handle);

    // 5. IA(Input Assembler) 단계 설정
    D3D12_VERTEX_BUFFER_VIEW vbView = {};
    vbView.BufferLocation = _vertexBuffer->GetGPUVirtualAddress();
    vbView.StrideInBytes = sizeof(HPBarVertex);
    vbView.SizeInBytes = sizeof(HPBarVertex) * activeCount; // 실제 채워진 개수만큼만 전송

    // [루트 파라미터 슬롯 2] t0: HP 바 알맹이
    if (_texture_HP_Bar)
    {
        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> handles = { _texture_HP_Bar->cpu_handle };
        Renderer::instance()->bind_texture_table(commandList, 2, handles);
    }

    // [루트 파라미터 슬롯 3] t1: HP 바 배경(테두리)
    if (_texture_HP_back)
    {
        std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> handles = { _texture_HP_back->cpu_handle };
        Renderer::instance()->bind_texture_table(commandList, 3, handles);
    }

    commandList->IASetVertexBuffers(0, 1, &vbView);
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);

    // 6. 드로우 콜! (instanceCount는 1로 고정, 점의 개수만 넘김)
    commandList->DrawInstanced(activeCount, 1, 0, 0);
}

void MonsterHPUIRenderComponent::set_hp_back_texture(const std::string& texture_path)
{
    _texture_HP_back = ResourceManager::instance()->load_texture(texture_path, true);

    if (!_texture_HP_back)
    {
        CERROR("Failed to load UI texture: " << texture_path);
    }
    else
    {
        CLOG("UI texture loaded successfully: " << texture_path);
    }
}

void MonsterHPUIRenderComponent::set_hp_bar_texture(const std::string& texture_path)
{
    _texture_HP_Bar = ResourceManager::instance()->load_texture(texture_path, true);

    if (!_texture_HP_Bar)
    {
        CERROR("Failed to load UI texture: " << texture_path);
    }
    else
    {
        CLOG("UI texture loaded successfully: " << texture_path);
    }
}
