#include "stdafx.h"
#include "VfxPresetLibrary.h"
#include <algorithm>
#include "LuaUtil.h"

namespace
{
    // 문자열 값을 열거형으로. 모르는 값이면 로그를 남기고 기본값
    template <typename E, size_t N>
    E parse_enum(LuaTable& t, const char* key, E def, const std::pair<const char*, E> (&table)[N])
    {
        if (!t.has(key)) return def;
        const std::string value = t.string(key, "");
        for (const auto& [name, e] : table)
            if (value == name) return e;
        std::string allowed;
        for (const auto& [name, e] : table) allowed += std::string(allowed.empty() ? "" : ", ") + name;
        CERROR("[Vfx] " << t.context() << "." << key << ": 알 수 없는 값 \"" << value << "\" (" << allowed << " 중 하나, 기본값 사용)");
        return def;
    }

    const std::pair<const char*, ParticleShape> kShapes[] = {
        { "Point", ParticleShape::Point }, { "Sphere", ParticleShape::Sphere }, { "Hemisphere", ParticleShape::Hemisphere },
        { "Cone", ParticleShape::Cone }, { "Edge", ParticleShape::Edge } };
    const std::pair<const char*, ParticleBlend> kBlends[] = {
        { "Additive", ParticleBlend::Additive }, { "Alpha", ParticleBlend::Alpha } };
    const std::pair<const char*, ParticleRenderMode> kRenderModes[] = {
        { "Billboard", ParticleRenderMode::Billboard }, { "StretchedBillboard", ParticleRenderMode::StretchedBillboard } };

    ParticleSystemSettings parse_settings(LuaTable& t)
    {
        ParticleSystemSettings s;
        // Main
        s.duration = t.number("duration", s.duration);
        s.looping = t.boolean("looping", s.looping);
        s.play_on_awake = t.boolean("play_on_awake", s.play_on_awake);
        s.start_lifetime = t.range("start_lifetime", s.start_lifetime);
        s.start_speed = t.range("start_speed", s.start_speed);
        s.start_size = t.range("start_size", s.start_size);
        s.start_rotation = t.range("start_rotation", s.start_rotation);
        s.start_color = t.color("start_color", s.start_color);
        s.start_color_2 = t.color("start_color_2", s.start_color); // 없으면 단색
        s.gravity_modifier = t.number("gravity_modifier", s.gravity_modifier);
        s.drag = t.number("drag", s.drag);
        s.max_particles = static_cast<uint32_t>(std::max(1, t.integer("max_particles", static_cast<int>(s.max_particles))));
        // Emission
        s.rate_over_time = t.number("rate_over_time", s.rate_over_time);
        t.for_each_array("bursts", [&](LuaTable& b) {
            s.bursts.push_back({ b.number("time", 0.0f), b.integer("count", 0) });
            b.warn_unknown_keys();
        });
        // Shape
        s.shape = parse_enum(t, "shape", s.shape, kShapes);
        s.radius = t.number("radius", s.radius);
        s.angle = t.number("angle", s.angle);
        s.length = t.number("length", s.length);
        // Over lifetime
        s.end_color = t.color("end_color", s.end_color);
        s.end_size_multiplier = t.number("end_size_multiplier", s.end_size_multiplier);
        // Renderer
        s.blend = parse_enum(t, "blend", s.blend, kBlends);
        s.render_mode = parse_enum(t, "render_mode", s.render_mode, kRenderModes);
        s.length_scale = t.number("length_scale", s.length_scale);
        return s;
    }
}

bool VfxPresetLibrary::load(const std::string& path)
{
    LuaState lua;
    if (!lua.do_file(path)) return false;

    std::unordered_map<std::string, ParticleSystemSettings> presets;
    const bool found = lua_for_each_global_table(lua.get(), "vfx", [&](const std::string& name, LuaTable& t) {
        presets[name] = parse_settings(t);
        t.warn_unknown_keys();
    });
    if (!found)
    {
        CERROR("[Vfx] " << path << ": 전역 테이블 vfx가 없음");
        return false;
    }

    _presets = std::move(presets);
    CLOG("[Vfx] 프리셋 " << _presets.size() << "개 읽음: " << path);
    return true;
}

const ParticleSystemSettings* VfxPresetLibrary::find(const std::string& name) const
{
    auto it = _presets.find(name);
    return it != _presets.end() ? &it->second : nullptr;
}

std::vector<std::string> VfxPresetLibrary::names() const
{
    std::vector<std::string> result;
    result.reserve(_presets.size());
    for (const auto& [name, settings] : _presets) result.push_back(name);
    std::sort(result.begin(), result.end());
    return result;
}
