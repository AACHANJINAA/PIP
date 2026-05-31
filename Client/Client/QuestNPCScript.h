#pragma once
#include "ScriptComponent.h"
#include "RenderComponent.h"
#include "AnimationComponent.h"
#include "NPCScript.h"

class QuestNPCScript : public NPCScript
{
public:
    using required_components = std::tuple<RenderComponent, AnimationComponent>;

    QuestNPCScript() = default;
    virtual ~QuestNPCScript() = default;

    virtual void init_visual() override;
    virtual void update(float deltaTime) override;

private:
    void update_F_interaction_UI(float deltaTime);

    std::shared_ptr<class UIRenderComponent> _uiRenderer;
    std::shared_ptr<class UIRenderComponent> _markerRenderer;
	float _markerYOffset = 0.0f;          // 마커의 y축 보정


	bool _isTalking = false;            // 플레이어와 대화 중인지 여부

    float _interactionDistance = 2.0f; // 상호작용 가능한 거리
    float _uiYOffset = 0.0f;            // 중앙점 보정 (필요시 조절)
    float _uiWidth = 40.0f;             // UI 가로 크기
    float _uiHeight = 40.0f;            // UI 세로 크기
    
    float _currentAlpha = 0.0f;         // 현재 투명도
    float _fadeSpeed = 3.0f;            // 알파 페이드 속도
};
