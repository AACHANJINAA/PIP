#pragma once
#include "RenderComponent.h"
#include "ResourceManager.h"

// UI용 간단한 Quad 메시 클래스
class UIQuadMesh : public Mesh
{
public:
    UIQuadMesh()
    {
        // UI용 간단한 정점 구조체
        struct UIVertex
        {
            XMFLOAT3 position;
            XMFLOAT2 texcoord;
        };

        // Quad 정점 데이터 (0~1 좌표계)
        std::vector<UIVertex> vertices = {
            { XMFLOAT3(0.0f, 0.0f, 0.0f), XMFLOAT2(0.0f, 0.0f) },  // 왼쪽 상단
            { XMFLOAT3(1.0f, 0.0f, 0.0f), XMFLOAT2(1.0f, 0.0f) },  // 오른쪽 상단
            { XMFLOAT3(0.0f, 1.0f, 0.0f), XMFLOAT2(0.0f, 1.0f) },  // 왼쪽 하단
            { XMFLOAT3(1.0f, 1.0f, 0.0f), XMFLOAT2(1.0f, 1.0f) }   // 오른쪽 하단
        };

        // 정점 데이터 설정
        set_vertex_data_buffer(vertices);

        // 인덱스 데이터 설정 (protected 멤버에 접근 가능)
        _indices = { 0, 1, 2, 2, 1, 3 };

        // Topology 설정
        _primitiveTopology = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    }
};

// UI 요소 상수 버퍼 구조체
struct CbUIElement
{
    XMFLOAT2 screen_position;  // 화면상 위치 (픽셀)
    XMFLOAT2 size;              // UI 크기 (픽셀)
    XMFLOAT4 color;             // 색상 tint
    XMFLOAT2 uv_offset;         // UV 오프셋
    XMFLOAT2 uv_scale;          // UV 스케일
    int use_texture;            // 텍스처 사용 여부 (1: 사용, 0: 단색)
    float padding;              // 패딩
};
// 화면 정보 상수 버퍼 구조체
struct CbScreenInfo
{
    float screen_width;
    float screen_height;
    float padding[2];
};

class UIRenderComponent : public RenderComponent
{
public:
    UIRenderComponent();
    virtual ~UIRenderComponent();

    // UI는 frustum culling 불필요
    virtual bool is_visible(const BoundingFrustum& frustum) const override { return true; }

    // UI는 유효하지 않은 bounding box 반환
    virtual BoundingOrientedBox get_world_bounding_box() const override
    {
        BoundingOrientedBox box;
        box.Center = XMFLOAT3(0, 0, 0);
        box.Extents = XMFLOAT3(0, 0, 0);
        box.Orientation = XMFLOAT4(0, 0, 0, 1);
        return box;
    }

    // UI 렌더링
    virtual void render(ID3D12GraphicsCommandList* commandList, UINT frame_index) override;

    // UI 속성 설정
    void set_screen_position(float x, float y) { _screen_position = XMFLOAT2(x, y); }
    void set_size(float width, float height) { _size = XMFLOAT2(width, height); }
    void set_size_x(float width) { _size.x = width; }
    float get_size_x() const { return _size.x; }
    void set_color(const XMFLOAT4& color) { _color = color; }
    void set_texture(const std::string& texture_path);

private:
    // UI 속성
    XMFLOAT2 _screen_position = XMFLOAT2(0.0f, 0.0f);
    XMFLOAT2 _size = XMFLOAT2(100.0f, 100.0f);
    XMFLOAT4 _color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    XMFLOAT2 _uv_offset = XMFLOAT2(0.0f, 0.0f);
    XMFLOAT2 _uv_scale = XMFLOAT2(1.0f, 1.0f);

    // 상수 버퍼
    ComPtr<ID3D12Resource> _cb_ui_element;
    CbUIElement* _mapped_ui_element = nullptr;

    // 화면 정보 버퍼 (static - 모든 UI가 공유)
    static ComPtr<ID3D12Resource> _cb_screen_info;
    static CbScreenInfo* _mapped_screen_info;
    static bool _screen_info_initialized;

    // 텍스처 정보
    ResourceManager::TextureInfo* _texture_info = nullptr;

    // Quad mesh 초기화
    void initialize_quad_mesh();
    void initialize_constant_buffers();
    static void initialize_screen_info();
};