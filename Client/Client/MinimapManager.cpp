#include "stdafx.h"
#include "MinimapManager.h"

#include "NetworkManager.h"
#include "Renderer.h"
#include "TerrainLoader.h"

// 버텍스 구조체 (POSITION + TEXCOORD)
struct MinimapVertex
{
    XMFLOAT3 position;
    XMFLOAT2 texcoord;
};

MinimapManager* MinimapManager::instance()
{
    static MinimapManager inst;
    return &inst;
}

void MinimapManager::initialize(ID3D12Device* device, ID3D12GraphicsCommandList* cmd_list)
{
    // 1. Quad 지오메트리 생성
    create_quad_geometry(device, cmd_list);

    // 2. 상수 버퍼 생성
    create_constant_buffers(device);

    float minimap_width_height = 300.0f;

    // 3. 초기 화면 설정 (1920x1080 기준, 좌측 상단에 200x200 크기)
    _constants.screen_width = FRAME_BUFFER_WIDTH;
    _constants.screen_height = FRAME_BUFFER_HEIGHT;
    _constants.screen_position = XMFLOAT2(FRAME_BUFFER_WIDTH - minimap_width_height - 20.0f, FRAME_BUFFER_HEIGHT - minimap_width_height - 20.0f); // 좌측 상단 여백
    _constants.minimap_size = XMFLOAT2(minimap_width_height, minimap_width_height);  // 미니맵 크기

    _initialized = true;
}

void MinimapManager::register_landscape_tiles(const std::vector<LandscapeTileInfo>& tiles)
{
    _tiles = tiles;

    // 첫 번째 타일로 초기화 (플레이어가 아직 spawn 안 됐을 때 대비)
    if (!_tiles.empty())
    {
        switch_to_tile(0);
    }
}

void MinimapManager::create_quad_geometry(ID3D12Device* device, ID3D12GraphicsCommandList* cmd_list)
{
    // Quad 버텍스 (0~1 범위, 셰이더에서 화면 좌표로 변환)
    MinimapVertex vertices[] =
    {
        { XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) }, // 좌측 상단
        { XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) }, // 우측 상단
        { XMFLOAT3(1.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) }, // 우측 하단
        { XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) }  // 좌측 하단
    };

    UINT16 indices[] = { 0, 1, 2, 0, 2, 3 };
    _indexCount = _countof(indices);

    // 버텍스 버퍼 생성
    const UINT vbByteSize = sizeof(vertices);
    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(vbByteSize);

    device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&_vertexBuffer)
    );

    // 데이터 복사
    void* pVertexDataBegin;
    CD3DX12_RANGE readRange(0, 0);
    _vertexBuffer->Map(0, &readRange, &pVertexDataBegin);
    memcpy(pVertexDataBegin, vertices, vbByteSize);
    _vertexBuffer->Unmap(0, nullptr);

    // 버텍스 버퍼 뷰 설정
    _vertexBufferView.BufferLocation = _vertexBuffer->GetGPUVirtualAddress();
    _vertexBufferView.StrideInBytes = sizeof(MinimapVertex);
    _vertexBufferView.SizeInBytes = vbByteSize;

    // 인덱스 버퍼 생성
    const UINT ibByteSize = sizeof(indices);
    bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(ibByteSize);

    device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&_indexBuffer)
    );

    // 데이터 복사
    void* pIndexDataBegin;
    _indexBuffer->Map(0, &readRange, &pIndexDataBegin);
    memcpy(pIndexDataBegin, indices, ibByteSize);
    _indexBuffer->Unmap(0, nullptr);

    // 인덱스 버퍼 뷰 설정
    _indexBufferView.BufferLocation = _indexBuffer->GetGPUVirtualAddress();
    _indexBufferView.Format = DXGI_FORMAT_R16_UINT;
    _indexBufferView.SizeInBytes = ibByteSize;
}

void MinimapManager::create_constant_buffers(ID3D12Device* device)
{
    const UINT cbSize = (sizeof(MinimapConstants) + 255) & ~255; // 256바이트 정렬

    CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(cbSize);

    for (UINT i = 0; i < FRAME_COUNT; ++i)
    {
        device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&_constantBuffers[i])
        );

        // 상수 버퍼 매핑 (계속 유지)
        CD3DX12_RANGE readRange(0, 0);
        _constantBuffers[i]->Map(0, &readRange, reinterpret_cast<void**>(&_cbMappedData[i]));
    }
}

int MinimapManager::find_tile_at_position(const XMFLOAT3& world_pos) const
{
    for (size_t i = 0; i < _tiles.size(); ++i)
    {
        const auto& tile = _tiles[i];

        // 타일 경계 계산
        float min_x = tile.world_origin.x;
        float max_x = tile.world_origin.x + tile.world_bounds.x;
        float min_z = tile.world_origin.y; // world_origin.y는 Z 좌표
        float max_z = tile.world_origin.y + tile.world_bounds.y;

        // 플레이어가 이 타일 내부에 있는지 확인
        if (world_pos.x >= min_x && world_pos.x <= max_x &&
            world_pos.z >= min_z && world_pos.z <= max_z)
        {
            return static_cast<int>(i);
        }
    }

    return -1; // 타일을 찾지 못함
}

void MinimapManager::switch_to_tile(int tile_index)
{
    if (tile_index < 0 || tile_index >= static_cast<int>(_tiles.size()))
    {
        CERROR("MinimapManager::switch_to_tile - Invalid tile index: " << tile_index);
        return;
    }

    if (tile_index == _currentTileIndex)
        return; // 이미 해당 타일 사용 중

    const auto& tile = _tiles[tile_index];

    // 타일 정보 업데이트
    _constants.map_world_size = tile.world_bounds;
    _constants.map_world_origin = tile.world_origin;
    _constants.min_height = tile.min_height;
    _constants.max_height = tile.max_height;
    // 현재 렌더링에 사용할 리소스와 SRV를 타일의 것으로 갱신
    _heightmapTexture = tile.heightmap_texture;
    _heightmapSRV = tile.heightmap_srv;

    _currentTileIndex = tile_index;
}

void MinimapManager::set_terrain_heightmap(
    ID3D12Resource* heightmap_texture,
    D3D12_GPU_DESCRIPTOR_HANDLE heightmap_srv,
    XMFLOAT2 world_bounds,
    XMFLOAT2 world_origin,
    float min_height,
    float max_height)
{
    _heightmapTexture = heightmap_texture;
    _heightmapSRV = heightmap_srv;

    _constants.map_world_size = world_bounds;
    _constants.map_world_origin = world_origin;
    _constants.min_height = min_height;
    _constants.max_height = max_height;
}

void MinimapManager::update_player_position(const XMFLOAT3& pos)
{
    // NetworkManager에서 내 서버 위치 가져오기
    auto network_mgr = NetworkManager::instance();
    XMFLOAT3 my_pos = network_mgr->get_minimap_server_position();
	XMFLOAT2 player_pos_2d = XMFLOAT2(my_pos.x, my_pos.z);
    _constants.player_world_pos = player_pos_2d;

    // [타일 전환] 플레이어 위치에서 타일 탐색
	if (!_tiles.empty())
	{
	    int new_tile_index = find_tile_at_position(my_pos);
	    if (new_tile_index >= 0 && new_tile_index != _currentTileIndex)
	    {
	        switch_to_tile(new_tile_index);
	    }
	}
}

void MinimapManager::render(ID3D12GraphicsCommandList* cmd_list, UINT frame_index)
{
    if (!_initialized)
        return;

    // [타일 시스템] 현재 타일이 유효한지 확인 currentTileIndex가 0임
    if (_tiles.empty() || _currentTileIndex < 0 || _currentTileIndex >= static_cast<int>(_tiles.size())) return; // 유효한 타일이 없으면 렌더링 스킵

    const auto& current_tile = _tiles[_currentTileIndex];
    
    // 포인터를 통해 실시간으로 SRV를 가져옵니다.
    D3D12_CPU_DESCRIPTOR_HANDLE live_cpu_srv = { 0 };
    if (current_tile.terrain_loader)
    {
        live_cpu_srv = current_tile.terrain_loader->get_heightmap_cpu_srv();
    }

    // 만약 여전히 0이라면 아직 GPU 업로드가 안 된 것이므로 다음 프레임을 기다립니다.
    if (live_cpu_srv.ptr == 0)return;

    // 1. 상수 버퍼 업데이트
    memcpy(_cbMappedData[frame_index], &_constants, sizeof(MinimapConstants));

	// 2. 루트 시그니처 설정
    auto renderer = Renderer::instance();
    ID3D12PipelineState* pso = renderer->get_pso("minimap");
    ID3D12RootSignature* root_signature = renderer->get_root_signature("minimap");

    cmd_list->SetPipelineState(pso);
    cmd_list->SetGraphicsRootSignature(root_signature);

    // 3. 상수 버퍼 바인딩 (b0)
    cmd_list->SetGraphicsRootConstantBufferView(0, _constantBuffers[frame_index]->GetGPUVirtualAddress());

    // 4. Heightmap 텍스처 바인딩 (t0)
    renderer->bind_texture_table(cmd_list, 1, { live_cpu_srv });

    // 5. 지오메트리 설정
    cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cmd_list->IASetVertexBuffers(0, 1, &_vertexBufferView);
    cmd_list->IASetIndexBuffer(&_indexBufferView);

    // 6. 드로우 콜
    cmd_list->DrawIndexedInstanced(_indexCount, 1, 0, 0, 0);
}