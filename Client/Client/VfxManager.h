#pragma once
#include <string>
#include <unordered_map>
#include "VfxPresetLibrary.h"

class GameObject;

// 이펙트 재생 창구 (ParticleSystem_Plan_KR.md 3.4). 유니티에서 프리팹을 Instantiate하는 것처럼 한 줄로 재생한다:
//   VfxManager::instance()->play("hit_spark", hit.point, hit.direction);
// - 프리셋은 Resource/Vfx/VfxPresets.lua (처음 재생할 때 읽고, F5 또는 reload()로 다시 읽음)
// - 이름별 방출기 오브젝트를 하나씩만 만들어 두고(풀링) 방출만 요청한다. 타격마다 게임 오브젝트를 만들지 않는다.
// - 프리셋의 time = 0 버스트 개수만큼 지정한 위치·방향에서 한 번 방출 (연속 방출·반복은 쓰지 않음)
class VfxManager : public Singleton<VfxManager>
{
    friend class Singleton<VfxManager>;
public:
    // 이펙트 재생. 프리셋이 없으면 로그만 (같은 이름은 한 번)
    void play(const std::string& name, const XMFLOAT3& position, const XMFLOAT3& direction);

    // 프리셋 파일을 다시 읽고 만들어 둔 방출기 설정을 갱신 (최대 개수는 처음 값 유지)
    void reload();

    const VfxPresetLibrary& presets();

    static constexpr const char* kPresetPath = "Resource/Vfx/VfxPresets.lua";

private:
    VfxManager() = default;

    void ensure_loaded();
    std::shared_ptr<GameObject> emitter(const std::string& name, const ParticleSystemSettings& preset);
    static ParticleSystemSettings to_one_shot(const ParticleSystemSettings& preset);

    VfxPresetLibrary _library;
    bool _loaded = false;
    std::unordered_map<std::string, std::shared_ptr<GameObject>> _emitters;
    std::unordered_map<std::string, bool> _missingLogged;
};
