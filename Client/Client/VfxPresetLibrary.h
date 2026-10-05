#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include "ParticleSystemSettings.h"

// 이펙트 프리셋 (Lua 파일 → ParticleSystemSettings), ParticleSystem_Plan_KR.md 3.3
// 파일의 전역 테이블 vfx의 각 항목이 프리셋 하나. 키 이름은 ParticleSystemSettings와 같다.
class VfxPresetLibrary
{
public:
    // 파일을 읽어 프리셋을 교체. 파일을 못 읽으면 기존 프리셋을 그대로 두고 false
    bool load(const std::string& path);

    const ParticleSystemSettings* find(const std::string& name) const;
    std::vector<std::string> names() const;    // 이름순
    size_t size() const { return _presets.size(); }

private:
    std::unordered_map<std::string, ParticleSystemSettings> _presets;
};
