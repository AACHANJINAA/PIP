#include "stdafx.h"
#include "DamageTextManager.h"
#include "GameFramework.h"
#include "CameraComponent.h"
#include "imgui/imgui.h"
#include "TimerManager.h"

void DamageTextManager::add_damage_text(const DirectX::XMFLOAT3& worldPos, float damage, bool isSkill)
{
    DamageText t;
    t.position = worldPos;
    // Add some random variation to position
    float rx = ((rand() % 100) / 100.0f - 0.5f) * 1.0f;
    float rz = ((rand() % 100) / 100.0f - 0.5f) * 1.0f;
    t.position.x += rx;
    t.position.y += 1.5f; // start a bit higher
    t.position.z += rz;
    
    t.damage = damage;
    t.isSkill = isSkill;
    t.timer = 0.0f;
    t.maxTime = 1.0f; // 1 second duration
    t.velocity = { 0.0f, 3.0f, 0.0f }; // Jump up initially

    _texts.push_back(t);
}

void DamageTextManager::update_and_render(float deltaTime)
{
    if (_texts.empty()) return;

    auto mainCamera = CameraComponent::get_main();
    if (!mainCamera) return;

    DirectX::XMMATRIX view = DirectX::XMLoadFloat4x4(&mainCamera->view_matrix());
    DirectX::XMMATRIX proj = DirectX::XMLoadFloat4x4(&mainCamera->projection_matrix());
    
    int w = GameFramework::instance()->get_window_width();
    int h = GameFramework::instance()->get_window_height();
    
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    
    // We create a full screen overlay window
    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2((float)w, (float)h));
    ImGui::Begin("DamageTextOverlay", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground);
    
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    for (auto it = _texts.begin(); it != _texts.end(); )
    {
        // Update
        it->timer += deltaTime;
        if (it->timer >= it->maxTime)
        {
            it = _texts.erase(it);
            continue;
        }

        // Physics
        it->position.x += it->velocity.x * deltaTime;
        it->position.y += it->velocity.y * deltaTime;
        it->position.z += it->velocity.z * deltaTime;
        
        // Gravity
        it->velocity.y -= 9.8f * deltaTime;

        // Render
        DirectX::XMVECTOR worldPos = DirectX::XMLoadFloat3(&it->position);
        DirectX::XMVECTOR screenPos = DirectX::XMVector3Project(worldPos, 0.0f, 0.0f, (float)w, (float)h, 0.0f, 1.0f, proj, view, DirectX::XMMatrixIdentity());
        
        float screenZ = DirectX::XMVectorGetZ(screenPos);
        if (screenZ > 0.0f && screenZ < 1.0f) // Only draw if in front of camera
        {
            float screenX = DirectX::XMVectorGetX(screenPos);
            float screenY = DirectX::XMVectorGetY(screenPos);

            float alpha = 1.0f - (it->timer / it->maxTime);
            ImU32 color = it->isSkill ? IM_COL32(255, 128, 0, (int)(alpha * 255)) : IM_COL32(255, 255, 255, (int)(alpha * 255));
            float fontSize = it->isSkill ? 36.0f : 24.0f;
            
            std::string dmgStr = std::to_string((int)it->damage);
            
            // Text stroke (shadow)
            drawList->AddText(ImGui::GetFont(), fontSize, ImVec2(screenX + 2, screenY + 2), IM_COL32(0, 0, 0, (int)(alpha * 255)), dmgStr.c_str());
            drawList->AddText(ImGui::GetFont(), fontSize, ImVec2(screenX, screenY), color, dmgStr.c_str());
        }

        ++it;
    }
    
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar(2);
}
