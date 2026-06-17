#pragma once
#include "stdafx.h"
#include <vector>
#include <DirectXMath.h>

struct DamageText
{
    DirectX::XMFLOAT3 position; // 3D pos
    float damage;
    bool isSkill;
    float timer;
    float maxTime;
    DirectX::XMFLOAT3 velocity;
};

class DamageTextManager : public Singleton<DamageTextManager>
{
    friend class Singleton<DamageTextManager>;
private:
    DamageTextManager() = default;
    ~DamageTextManager() = default;

public:
    void add_damage_text(const DirectX::XMFLOAT3& worldPos, float damage, bool isSkill = false);
    void update_and_render(float deltaTime);

private:
    std::vector<DamageText> _texts;
};
