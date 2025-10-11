#include "stdafx.h"
#include "LayerManager.h"

LayerManager::LayerManager()
{
    // 게임 시작 시 기본적으로 필요한 레이어들을 미리 등록합니다.
    add_layer("Default");
    add_layer("Player");
    add_layer("Enemy");
    add_layer("Environment");
    add_layer("PlayerWeapon");
    add_layer("EnemyWeapon");
}

uint32_t LayerManager::get_layer_value(const std::string& name) const
{
    auto it = _layerMap.find(name);
    if (it != _layerMap.end())
    {
        return it->second;
    }
    // 존재하지 않는 레이어는 Default 레이어(0)의 값을 반환하거나 오류 처리
    return 0;
}

bool LayerManager::add_layer(const std::string& name)
{
    // 이미 존재하거나, 최대 레이어 수에 도달했으면 실패
    if (_layerMap.contains(name) || _nextLayerBit >= MAX_LAYER_COUNT)
    {
        return false;
    }

    // 다음 비트 값을 할당 (0, 1, 2, 3 ... -> 1, 2, 4, 8 ...)
    uint32_t layerValue = (1 << _nextLayerBit);
    _layerMap[name] = layerValue;
    _nextLayerBit++;

    return true;
}