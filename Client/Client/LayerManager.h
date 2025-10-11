#pragma once
class LayerManager : public Singleton<LayerManager>
{
    friend Singleton<LayerManager>;
private:
    LayerManager(); // 기본 레이어("Default", "Player" 등)를 등록하기 위해 private
    ~LayerManager() = default;

public:
    // 레이어 이름으로 레이어의 비트 값을 가져옵니다.
    uint32_t get_layer_value(const std::string& name) const;

    // 새로운 레이어를 등록합니다. 성공 시 true 반환.
    bool add_layer(const std::string& name);

private:
    std::map<std::string, uint32_t> _layerMap;
    uint32_t                        _nextLayerBit = 0;
    const uint32_t                  MAX_LAYER_COUNT = 32;
};
