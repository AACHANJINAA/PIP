#pragma once
#include "ScriptComponent.h"

class OtherPlayerScript : public ScriptComponent
{
public:
    OtherPlayerScript() = default;
    virtual ~OtherPlayerScript() = default;

    // 서버로부터 위치 동기화 패킷을 받았을 때 호출될 함수 (예시)
    void on_sync_position(const XMFLOAT3& newPosition);

    virtual void update(float deltaTime) override;
};

