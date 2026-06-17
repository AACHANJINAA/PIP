#pragma once
#include "ScriptComponent.h"
#include "RenderComponent.h"
#include "AnimationComponent.h"

class LeverScript : public ScriptComponent
{
public:
    using required_components = std::tuple<RenderComponent, AnimationComponent>;

    LeverScript() = default;
    virtual ~LeverScript() = default;

    virtual void awake() override;
    virtual void update(float deltaTime) override;

    void interact(); // 상호작용 함수 -> 레버 내리기

private:
    void update_F_interaction_UI(float deltaTime);

    std::shared_ptr<class UIRenderComponent> _uiRenderer;

    float _interactionDistance = 3.0f; // 상호작용 가능한 거리
    float _uiYOffset = 0.0f;           // UI가 뜰 높이 보정값

    float _uiWidth = 40.0f;            // UI 가로 크기
    float _uiHeight = 40.0f;           // UI 세로 크기
    
    float _currentAlpha = 0.0f;        // 현재 투명도
    float _fadeSpeed = 3.0f;           // 알파 페이드 속도

    bool _isInteracted = false;        // 상호작용 완료 여부
};
