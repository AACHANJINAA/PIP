#pragma once
#include "RenderComponent.h"
#include "ResourceManager.h"
#include "BillboardUIShader.h"

struct alignas(16) BillboardUICB {
    DirectX::XMFLOAT2 size;
    float alpha;
    float padding;
    DirectX::XMFLOAT4 color;
};

class BillboardUIRenderComponent : public RenderComponent
{
public:
    BillboardUIRenderComponent();
    virtual ~BillboardUIRenderComponent();

    virtual void render(ID3D12GraphicsCommandList* commandList, UINT frame_index) override;

    void set_texture(const std::string& texture_path);
    void set_size(float width, float height) { _cbData.size = { width, height }; }
    void set_alpha(float alpha) { _cbData.alpha = alpha; }
    void set_color(const DirectX::XMFLOAT4& color) { _cbData.color = color; }
    void set_y_offset(float offset) { _yOffset = offset; }

    virtual bool is_visible(const BoundingFrustum& frustum) const override { return true; }

    virtual BoundingOrientedBox get_world_bounding_box() const override
    {
        BoundingOrientedBox box;
        box.Center = XMFLOAT3(0, 0, 0);
        box.Extents = XMFLOAT3(0.5f, 0.5f, 0.5f);
        box.Orientation = XMFLOAT4(0, 0, 0, 1);
        return box;
    }

private:
    void initialize_buffers();

    BillboardUICB _cbData;

    ComPtr<ID3D12Resource> _cbResource;
    BillboardUICB* _cbMappedData = nullptr;

    ComPtr<ID3D12Resource> _vertexBuffer;
    BillboardUIVertex* _vbMappedData = nullptr;
    D3D12_VERTEX_BUFFER_VIEW _vertexBufferView;

    ResourceManager::TextureInfo* _textureInfo = nullptr;
    float _yOffset = 0.0f;
};
