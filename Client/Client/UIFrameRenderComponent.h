#pragma once
#include "ResourceManager.h"
#include "UIRenderComponent.h"

// UI 요소 상수 버퍼 구조체
struct cbUIFrameElement
{
    XMFLOAT2 screen_position;  // 화면상 위치 (픽셀)
    XMFLOAT2 size;              // UI 크기 (픽셀)
    XMFLOAT4 color;             // 색상 tint
    XMFLOAT2 uv_offset;         // UV 오프셋
    XMFLOAT2 uv_scale;          // UV 스케일
    int use_texture;            // 텍스처 사용 여부 (1: 사용, 0: 단색)
    int other_player_id;        // 다른 플레이어
    XMFLOAT2 padding;
};

class UIFrameRenderComponent : public UIRenderComponent
{
public:
    UIFrameRenderComponent();
    virtual ~UIFrameRenderComponent();

    virtual void render(ID3D12GraphicsCommandList* commandList, UINT frame_index) override;

    void set_other_player_id(int id) { _other_player_id = id; }

protected:
    virtual void initialize_constant_buffers() override;

private:
    // 부모와 중복되는 _screen_position, _size, _color 등은 모두 삭제!!

    // 본인 전용 상수 버퍼 리소스와 매핑 포인터 (구조체가 다르므로 이건 필요함)
    ComPtr<ID3D12Resource> _cb_ui_frame_element;
    cbUIFrameElement* _mapped_ui_frame_element = nullptr;

    int _other_player_id = 0;
};