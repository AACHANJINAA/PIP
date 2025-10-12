#pragma once
#include "ScriptComponent.h"
class GltfTestScript : public ScriptComponent
{
public:
    GltfTestScript() = default;
    virtual ~GltfTestScript() = default;

    // [역할] 스크립트가 깨어날 때(최초 1회) 호출됩니다.
    // 여기서 필요한 모든 컴포넌트를 스스로 장착하는 로직을 수행합니다.
    virtual void awake() override;

    // [역할] 매 프레임 호출되어 실시간 로직을 처리합니다.
    virtual void update(float delta_time) override;

	virtual void late_update(float delta_time) override;
};

