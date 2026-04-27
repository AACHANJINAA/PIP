#pragma once
namespace PIP::GAME
{
    enum class ItemId : uint32_t {
        ITEM_WOOD1 = 1,
        ITEM_ORE1 = 2,
        ITEM_STICK = 3,
        // ... 추가 아이템 ID
    };
    struct EquipItem {
        int64_t item_uid;     // DB에서 발급된 고유 ID (Primary Key)
        ItemId  item_id;      // 원본 아이템 ID (예: 롱소드)
        int     enhance_level;// 강화 수치
        bool    is_equipped;  // 장착 여부
    };
}