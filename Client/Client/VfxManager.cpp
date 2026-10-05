#include "stdafx.h"
#include "VfxManager.h"
#include "GameObject.h"
#include "ObjectManager.h"
#include "ParticleSystemComponent.h"

void VfxManager::ensure_loaded()
{
    if (_loaded) return;
    _loaded = true; // 실패해도 매 재생마다 다시 읽지 않음 (F5로 다시 시도)
    _library.load(kPresetPath);
}

const VfxPresetLibrary& VfxManager::presets()
{
    ensure_loaded();
    return _library;
}

ParticleSystemSettings VfxManager::to_one_shot(const ParticleSystemSettings& preset)
{
    // 풀링된 방출기는 스스로 방출하지 않고 play 요청만 받음
    ParticleSystemSettings s = preset;
    s.play_on_awake = false;
    s.looping = false;
    s.rate_over_time = 0.0f;
    return s;
}

std::shared_ptr<GameObject> VfxManager::emitter(const std::string& name, const ParticleSystemSettings& preset)
{
    auto it = _emitters.find(name);
    if (it != _emitters.end() && it->second && !it->second->is_destroyed()) return it->second;

    // 처음 재생이거나 씬 전환으로 지워졌으면 새로 만듦
    auto obj = ObjectManager::instance()->create_game_object("Vfx_" + name);
    auto ps = obj->add_component<ParticleSystemComponent>();
    ps->set_settings(to_one_shot(preset));
    _emitters[name] = obj;
    return obj;
}

void VfxManager::play(const std::string& name, const XMFLOAT3& position, const XMFLOAT3& direction)
{
    ensure_loaded();
    const ParticleSystemSettings* preset = _library.find(name);
    if (!preset)
    {
        if (!_missingLogged[name]) CERROR("[Vfx] 프리셋 없음: " << name << " (" << kPresetPath << ")");
        _missingLogged[name] = true;
        return;
    }

    auto ps = emitter(name, *preset)->get_component<ParticleSystemComponent>();
    if (!ps) return;
    for (const auto& burst : preset->bursts)
    {
        if (burst.time <= 0.0f) ps->emit(burst.count, position, direction);
    }
}

void VfxManager::reload()
{
    _loaded = true;
    if (!_library.load(kPresetPath)) return;
    _missingLogged.clear();

    for (auto& [name, obj] : _emitters)
    {
        if (!obj || obj->is_destroyed()) continue;
        const ParticleSystemSettings* preset = _library.find(name);
        auto ps = obj->get_component<ParticleSystemComponent>();
        if (preset && ps) ps->set_settings(to_one_shot(*preset));
    }
}
