#include "stdafx.h"
#include "UIManager.h"
#include "GameObject.h"
#include "UIRenderComponent.h"

UIManager::UIManager()
{
}

UIManager::~UIManager()
{
}

void UIManager::initialize(ID3D12Device* device, ID3D12GraphicsCommandList* command_list)
{
    // 초기화 로직
}

void UIManager::release()
{
    for (auto& layer_map : _uiLayers)
    {
        layer_map.clear();
    }
}

void UIManager::add_ui(UILayer layer, const std::string& name, const std::shared_ptr<GameObject>& ui_object)
{
    if (ui_object == nullptr) return;

    auto layer_idx = static_cast<int>(layer);

    if (_uiLayers[layer_idx].contains(name))
    {
        CERROR("UI addition failed - UI name already exists: " << name);
        return;
    }

    std::shared_ptr<GameObject> data = ui_object;

    _uiLayers[layer_idx][name] = data;
}

void UIManager::remove_ui(UILayer layer, const std::string& name)
{
    auto layer_idx = static_cast<int>(layer);
    _uiLayers[layer_idx].erase(name);
}

void UIManager::set_visible(UILayer layer, const std::string& name, bool is_visible)
{
    auto layer_idx = static_cast<int>(layer);
    auto it = _uiLayers[layer_idx].find(name);

    if (it != _uiLayers[layer_idx].end())
    {
		it->second->get_component<UIRenderComponent>()->set_enabled(is_visible);
    }
    else
    {
        CERROR("UI visibility setting failed - UI not found: " << name);
    }
}

std::shared_ptr<UIRenderComponent> UIManager::ui_component(UILayer layer, const std::string& name) const
{
    auto layer_idx = static_cast<int>(layer);
    auto it = _uiLayers[layer_idx].find(name);

    if (it != _uiLayers[layer_idx].end())
    {
        return it->second->get_component<UIRenderComponent>();
    }
    CERROR("UI component retrieval failed - UI not found: " << name);
    return nullptr;
}

void UIManager::set_render_vector()
{
    for (auto& vec : _uiRanderVector)
    {
        vec.clear();
    }

    for (int i = 0 ; i < _uiLayers.size(); ++i)
    {
        const auto& uimap = _uiLayers[i];
        
        for (const auto& ui : uimap)
        {
            if (ui.second->get_component<UIRenderComponent>()->is_enabled())
            {
                _uiRanderVector[i].push_back(ui.second);
            }
        }
    }
}

std::array<std::vector<std::shared_ptr<GameObject>>, static_cast<int>(UILayer::COUNT)>& UIManager::ui_render_vector()
{
    return _uiRanderVector;
}

