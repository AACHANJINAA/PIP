#pragma once
#include "stdafx.h"

class TerrainLoader;

// 미니맵 상수 버퍼 구조체 (Minimap_Shader.hlsl의 cbMinimap과 동일)
struct MinimapConstants
{
    XMFLOAT2 map_world_size;      // 맵 월드 크기 (X, Z)
    XMFLOAT2 map_world_origin;    // 맵 월드 원점 (X, Z)
    XMFLOAT2 player_world_pos;    // 플레이어 월드 위치 (X, Z)
    float min_height;             // 최소 높이
    float max_height;             // 최대 높이
    XMFLOAT2 screen_position;     // 화면 픽셀 위치 (좌측 상단)
    XMFLOAT2 minimap_size;        // 미니맵 화면 크기 (픽셀)
    float screen_width;           // 화면 너비
    float screen_height;          // 화면 높이
    float padding[2];             // 패딩
};

// Landscape 타일 정보
struct LandscapeTileInfo
{
    TerrainLoader* terrain_loader = nullptr; // 원본 지형 객체 참조 추가
    ID3D12Resource* heightmap_texture = nullptr;
    D3D12_GPU_DESCRIPTOR_HANDLE heightmap_srv = {};
    XMFLOAT2 world_bounds;          // 타일 크기
    XMFLOAT2 world_origin;          // 타일 원점
    float min_height = 0.0f;
    float max_height = 100.0f;
    int tile_index = -1;            // 타일 인덱스 (-1: 유효하지 않음)
};


class MinimapManager
{
private:
    MinimapManager() = default;
    ~MinimapManager() = default;

public:
    // 싱글톤 인스턴스
    static MinimapManager* instance();

    // 초기화 (디바이스, 커맨드 리스트 필요)
    void initialize(ID3D12Device* device, ID3D12GraphicsCommandList* cmd_list);

    // 모든 Landscape 타일 등록
    void register_landscape_tiles(const std::vector<LandscapeTileInfo>& tiles);

    // Terrain Heightmap 설정
    void set_terrain_heightmap(
        ID3D12Resource* heightmap_texture,
        D3D12_GPU_DESCRIPTOR_HANDLE heightmap_srv,
        XMFLOAT2 world_bounds,
        XMFLOAT2 world_origin,
        float min_height,
        float max_height
    );

    // 플레이어 위치 업데이트 (매 프레임)
    void update_player_position(const XMFLOAT3& pos);

    // 렌더링
    void render(ID3D12GraphicsCommandList* cmd_list, UINT frame_index);

    // 초기화 여부 확인
    bool is_initialized() const { return _initialized; }

private:
    // Quad 버텍스 버퍼 생성
    void create_quad_geometry(ID3D12Device* device, ID3D12GraphicsCommandList* cmd_list);

    // 상수 버퍼 생성 (프레임 버퍼링)
    void create_constant_buffers(ID3D12Device* device);

    // 플레이어 위치에서 타일 인덱스 계산
    int find_tile_at_position(const XMFLOAT3& world_pos) const;

    // 현재 타일 전환
    void switch_to_tile(int tile_index);

private:
    static constexpr UINT FRAME_COUNT = 2;

    bool _initialized = false;

    // Quad 지오메트리
    ComPtr<ID3D12Resource> _vertexBuffer;
    ComPtr<ID3D12Resource> _indexBuffer;
    D3D12_VERTEX_BUFFER_VIEW _vertexBufferView = {};
    D3D12_INDEX_BUFFER_VIEW _indexBufferView = {};
    UINT _indexCount = 0;

    // 상수 버퍼 (프레임 버퍼링)
    ComPtr<ID3D12Resource> _constantBuffers[FRAME_COUNT];
    MinimapConstants* _cbMappedData[FRAME_COUNT] = { nullptr, nullptr };

    // Heightmap 정보
    ID3D12Resource* _heightmapTexture = nullptr;  // 외부에서 관리되는 리소스
    D3D12_GPU_DESCRIPTOR_HANDLE _heightmapSRV = {};

    // Landscape 타일 정보들
    std::vector<LandscapeTileInfo> _tiles;
    int _currentTileIndex = -1;  // 현재 표시 중인 타일

    // 미니맵 설정
    MinimapConstants _constants = {};
};