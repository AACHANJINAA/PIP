#pragma once
#include "Component.h"

namespace PIP::GAME {

    class InventoryComponent : public Component {
    public:
		InventoryComponent(GameObject* owner) : Component(owner) {}
        ~InventoryComponent() override = default;

        // [재료 아이템 관리]
        void add_material(ItemId item_id, uint32_t count) {
            _materials[item_id] += count;
            _isDirty = true;
        }

        bool remove_material(ItemId item_id, uint32_t count) {
			if (_materials[item_id] < count) return false; // 재료가 충분하지 않음
            _materials[item_id] -= count;
            _isDirty = true;
			return true;// 제거 성공
        }

        // [장비 아이템 관리]
        void add_equipment(const EquipItem& equip) {
            _equipments[equip.item_uid] = equip;
            _isDirty = true;
        }

        void remove_equipment(int64_t item_uid) {
            _equipments.erase(item_uid);
            _isDirty = true;
        }

        // --- 스레드 안전성을 위한 스냅샷 기능 ---
        // DB 스레드로 데이터를 넘길 때 값 복사(Value Copy)를 수행하여 전달합니다.
        std::unordered_map<ItemId, uint32_t> get_materials_snapshot() const {
            return _materials; // NRVO(RVO) 최적화로 복사 오버헤드 최소화
        }

        std::unordered_map<int64_t, EquipItem> get_equipments_snapshot() const {
            return _equipments;
        }

        bool is_dirty() const { return _isDirty; }
        void mark_saved() { _isDirty = false; }

    private:

        // 재료 (스택 가능)
        std::unordered_map<ItemId, uint32_t> _materials;

        // 장비 (스택 불가, 개별 UID 존재)
        std::unordered_map<int64_t, EquipItem> _equipments;
        bool _isDirty = false;
    };
}
