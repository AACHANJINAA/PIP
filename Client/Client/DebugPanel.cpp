#include "stdafx.h"
#include "DebugPanel.h"
#include "imgui/imgui.h"
#include "GameObject.h"
#include "ObjectManager.h"
#include "ParticleSystemComponent.h"
#include "ServerClock.h"
#include "TransformComponent.h"

namespace
{
    const char* kParticleModeNames[] = {
        "Fountain (additive, stretched, gravity, cone)",
        "Smoke (alpha, rotation, grows, hemisphere)",
        "Explosion (additive, 300 burst / 1s, drag, sphere)",
        "Rain (alpha, stretched, 4m edge, down)",
    };
}

void DebugPanel::draw()
{
    // 서버 위치 유령은 패널이 열려 있을 때만
    ServerClock::instance()->set_debug_visible(_open);
    if (!_open) return;

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Debug (\\)", &_open, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::End();
        return;
    }

    if (ImGui::CollapsingHeader("Network", ImGuiTreeNodeFlags_DefaultOpen))
        ServerClock::instance()->draw_debug_contents();

    if (ImGui::CollapsingHeader("Particle Test"))
        draw_particle_test();

    ImGui::End();
}

void DebugPanel::draw_particle_test()
{
    if (_particleTest && _particleTest->is_destroyed()) _particleTest.reset(); // 씬 전환으로 지워진 경우
    if (!_particleTest)
    {
        _particleTest = ObjectManager::instance()->create_game_object("Debug_ParticleTest");
        auto ps = _particleTest->add_component<ParticleSystemComponent>();
        apply_particle_mode();
    }
    auto ps = _particleTest->get_component<ParticleSystemComponent>();
    if (!ps) return;

    if (ImGui::Combo("Mode", &_particleMode, kParticleModeNames, IM_ARRAYSIZE(kParticleModeNames)))
    {
        const bool playing = ps->is_playing();
        apply_particle_mode();
        if (playing) ps->play();
    }

    if (ps->is_playing())
    {
        if (ImGui::Button("Stop")) ps->stop();
    }
    else if (ImGui::Button("Play in front of player"))
    {
        place_particle_test();
        ps->play();
    }
    ImGui::SameLine();
    if (ImGui::Button("Move here")) place_particle_test();
    ImGui::SameLine();
    if (ImGui::Button("Clear")) ps->clear();

    ImGui::SliderInt("Burst count", &_burstCount, 1, 5000);
    ImGui::SameLine();
    if (ImGui::Button("Burst")) ps->emit(_burstCount);

    ImGui::Text("max_particles %u (over-limit bursts log once)", ps->settings().max_particles);
}

void DebugPanel::place_particle_test()
{
    auto player = ObjectManager::instance()->find_by_name("MainPlayer");
    if (!player || !_particleTest) return;
    XMFLOAT3 pos = player->transform()->get_world_position();
    XMFLOAT3 fwd = player->transform()->forward();
    _particleTest->transform()->set_local_position({ pos.x - fwd.x * 3.0f, pos.y + 1.0f, pos.z - fwd.z * 3.0f });
}

void DebugPanel::apply_particle_mode()
{
    auto ps = _particleTest ? _particleTest->get_component<ParticleSystemComponent>() : nullptr;
    if (!ps) return;

    ParticleSystemSettings s{};
    s.play_on_awake = false;
    s.max_particles = 1024;
    switch (_particleMode)
    {
    case 0:
        s.start_lifetime = { 1.0f, 1.6f }; s.start_speed = { 6.0f, 9.0f }; s.start_size = { 0.05f, 0.08f };
        s.start_color = { 1.0f, 0.9f, 0.5f, 1.0f }; s.start_color_2 = { 1.0f, 0.5f, 0.1f, 1.0f }; s.end_color = { 1.0f, 0.2f, 0.0f, 0.0f };
        s.gravity_modifier = 1.0f; s.rate_over_time = 200.0f; s.shape = ParticleShape::Cone; s.angle = 20.0f;
        s.blend = ParticleBlend::Additive; s.render_mode = ParticleRenderMode::StretchedBillboard;
        break;
    case 1:
        s.start_lifetime = { 2.0f, 3.0f }; s.start_speed = { 0.5f, 1.0f }; s.start_size = { 0.3f, 0.5f }; s.start_rotation = { 0.0f, 360.0f };
        s.start_color = { 0.5f, 0.5f, 0.5f, 0.6f }; s.start_color_2 = s.start_color; s.end_color = { 0.3f, 0.3f, 0.3f, 0.0f };
        s.end_size_multiplier = 3.0f; s.rate_over_time = 30.0f; s.shape = ParticleShape::Hemisphere; s.radius = 0.3f;
        s.blend = ParticleBlend::Alpha; s.render_mode = ParticleRenderMode::Billboard;
        break;
    case 2:
        s.looping = true; s.duration = 1.0f; s.rate_over_time = 0.0f; s.bursts = { { 0.0f, 300 } };
        s.start_lifetime = { 0.4f, 0.8f }; s.start_speed = { 8.0f, 14.0f }; s.start_size = { 0.06f, 0.1f };
        s.start_color = { 1.0f, 1.0f, 1.0f, 1.0f }; s.start_color_2 = { 1.0f, 0.7f, 0.2f, 1.0f }; s.end_color = { 1.0f, 0.3f, 0.0f, 0.0f };
        s.drag = 4.0f; s.shape = ParticleShape::Sphere; s.radius = 0.2f;
        s.blend = ParticleBlend::Additive; s.render_mode = ParticleRenderMode::StretchedBillboard;
        break;
    default:
        s.start_lifetime = { 1.5f, 1.5f }; s.start_speed = { 3.0f, 3.0f }; s.start_size = { 0.03f, 0.03f };
        s.start_color = { 0.6f, 0.8f, 1.0f, 0.8f }; s.start_color_2 = s.start_color; s.end_color = { 0.6f, 0.8f, 1.0f, 0.0f };
        s.rate_over_time = 150.0f; s.shape = ParticleShape::Edge; s.length = 4.0f;
        s.blend = ParticleBlend::Alpha; s.render_mode = ParticleRenderMode::StretchedBillboard;
        break;
    }
    ps->set_settings(s);

    // 방출 방향(오브젝트 정면 +Z): 비만 아래, 나머지는 위
    _particleTest->transform()->set_local_rotation(_particleMode == 3 ? 90.0f : -90.0f, 0.0f, 0.0f);
}
