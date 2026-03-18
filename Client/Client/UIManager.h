#pragma once
#include "stdafx.h"
#include "UIRenderComponent.h"

class GameObject;

enum class UILayer
{
    BACKGROUND = 0,
    MIDDLE,
    FRONT,

    COUNT
};

class UIManager : public Singleton<UIManager>
{
    friend Singleton<UIManager>;
    UIManager();
    ~UIManager() override;

public:
    void initialize(ID3D12Device* device, ID3D12GraphicsCommandList* command_list);
    virtual void release() override;

	// UI 추가 및 제거
    void add_ui(UILayer layer, const std::string& name, std::shared_ptr<GameObject> ui_object);
    void remove_ui(UILayer layer, const std::string& name);

    // Setter -> 렌더링 키고 끄기
    void set_visible(UILayer layer, const std::string& name, bool is_visible);

    // UIRenderComponent를 가져올 수 있도록 하였음 -> UI 속성 설정 변경 가능
    std::shared_ptr<UIRenderComponent> ui_component(UILayer layer, const std::string& name) const;

    void set_render_vector();

    // 렌더링 할 ui vector 가져오기
    std::array<std::vector<std::shared_ptr<GameObject>>, static_cast<int>(UILayer::COUNT)>& ui_render_vector();

private:
    std::array<std::unordered_map<std::string, std::shared_ptr<GameObject>>, static_cast<int>(UILayer::COUNT)> _uiLayers;

    std::array<std::vector<std::shared_ptr<GameObject>>, static_cast<int>(UILayer::COUNT)> _uiRanderVector;
};