#pragma once
#include "Component.h"
#include "ItemHeader.h"

namespace PIP::GAME {

    class InventoryComponent : public Component {
    public:
		InventoryComponent(GameObject* owner) : Component(owner) {}
        ~InventoryComponent() override = default;
        void Initialize() override;
        void Update(float deltaTime) override;
        void Update(float deltaTime, JPH::TempAllocator* allocator) override;
        void PhysicsUpdate(float deltaTime) override;
        void PhysicsUpdate(float deltaTime, JPH::TempAllocator* tempAllocator) override;
        // 아이템 획득
        void add_item(ItemId item_id, uint32_t count) {
            _items[item_id] += count;
            _isDirty = true; // DB에 저장해야 함을 표시
        }

        // 아이템 사용/버리기
        bool remove_item(ItemId item_id, uint32_t count) {
            if (_items[item_id] < count) return false;
            _items[item_id] -= count;
            _isDirty = true;
            return true;
        }

        const auto& get_items() const { return _items; }
        bool is_dirty() const { return _isDirty; }
        void mark_saved() { _isDirty = false; }

    private:
        std::unordered_map<ItemId, uint32_t> _items; // item_id -> count
        bool _isDirty = false;
    };
}
