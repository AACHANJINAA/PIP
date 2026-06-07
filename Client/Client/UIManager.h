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

struct PartySlot {
	bool is_active = false;
	int64_t player_id = -1;
	std::shared_ptr<UIRenderComponent> hp_bar;
	std::shared_ptr<UIRenderComponent> mp_bar;
    std::shared_ptr<UIRenderComponent> id_icon;
	float max_width = 150.0f; // 파티원용 작은 바 크기
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
    void add_ui(UILayer layer, const std::string& name, const std::shared_ptr<GameObject>& ui_object);
    void remove_ui(UILayer layer, const std::string& name);

    // Setter -> 렌더링 키고 끄기
    void set_visible(UILayer layer, const std::string& name, bool is_visible);

	// getter -> 렌더링 켜져있는지 확인
	bool is_visible(UILayer layer, const std::string& name) const;

    // UIRenderComponent를 가져올 수 있도록 하였음 -> UI 속성 설정 변경 가능
    std::shared_ptr<UIRenderComponent> ui_component(UILayer layer, const std::string& name) const;

    void set_render_vector();

    // 렌더링 할 ui vector 가져오기
    std::array<std::vector<std::shared_ptr<GameObject>>, static_cast<int>(UILayer::COUNT)>& ui_render_vector();

	// 파티 슬롯 초기화 (Scene에서 호출)
	void init_party_slots(int index, std::shared_ptr<UIRenderComponent> hp, std::shared_ptr<UIRenderComponent> mp, std::shared_ptr<UIRenderComponent> id_icon);

	// 슬롯 할당 및 해제
	int assign_party_slot(int64_t player_id);
	void free_party_slot(int64_t player_id);

	PartySlot* get_party_slot(int index) { return &_partySlots[index]; }

private:
    PartySlot _partySlots[4]; // 0번: 메인, 1~3번: 타인

    std::array<std::unordered_map<std::string, std::shared_ptr<GameObject>>, static_cast<int>(UILayer::COUNT)> _uiLayers;

    std::array<std::vector<std::shared_ptr<GameObject>>, static_cast<int>(UILayer::COUNT)> _uiRanderVector;
};