#include "stdafx.h"
#include "SoundManager.h"
#include "ObjectManager.h"
#include "GameObject.h"
#include "TransformComponent.h"

SoundManager::SoundManager()
{
}

SoundManager::~SoundManager()
{
}

bool SoundManager::initialize()
{
    FMOD_RESULT result;

    // 1. FMOD 시스템 객체 생성
    result = FMOD::System_Create(&_system);
    if (!check_fmod_error(result, "System_Create")) return false;

    // 2. 버전 체크 (선택 사항이지만 안전을 위해)
    unsigned int version;
    result = _system->getVersion(&version);
    if (version < FMOD_VERSION)
    {
        CERROR("FMOD lib version doesn't match header version!");
        return false;
    }

    // 3. FMOD 시스템 초기화 
    // (최대 512개의 채널, 기본 초기화 설정 사용, 추가 설정 없음)
    result = _system->init(512, FMOD_INIT_NORMAL, nullptr);
    if (!check_fmod_error(result, "System Init")) return false;

    // 4. 3D 사운드 환경 설정 (도플러 스케일, 거리 비율, 롤오프 스케일)
    // - 게임 스케일에 맞춰 나중에 조절 가능 (기본값은 1.0f)
    _system->set3DSettings(1.0f, 1.0f, 1.0f);


    // 채널 그룹 셋팅-- -
    // 마스터 그룹은 FMOD가 기본적으로 들고 있으니 가져오기
    _system->getMasterChannelGroup(&_masterGroup);

    // BGM과 SFX 그룹 생성
    _system->createChannelGroup("BGM", &_bgmGroup);
    _system->createChannelGroup("SFX", &_sfxGroup);

    // 마스터 그룹 아래에 BGM과 SFX를 자식으로 소속시킴 (마스터 볼륨 줄이면 다 같이 줄어들게)
    _masterGroup->addGroup(_bgmGroup);
    _masterGroup->addGroup(_sfxGroup);

    CLOG("[SUCCESS] FMOD SoundManager Initialized.");
    return true;
}

void SoundManager::update(float deltaTime)
{
    if (_system)
    {
        // 구간 재생용 타이머 업데이트 및 만료 시 종료 처리
        for (auto it = _stopTimers.begin(); it != _stopTimers.end(); )
        {
            it->second -= deltaTime;
            if (it->second <= 0.0f)
            {
                stop(it->first);
                it = _stopTimers.erase(it);
            }
            else
            {
                ++it;
            }
        }

        // FMOD 코어 엔진 업데이트 (매 프레임 호출 필수!)
        // 3D 사운드 계산, 채널 정리 등을 백그라운드에서 수행함
        
        // 오브젝트 매니저에서 카메라를 찾아옴
        auto cameraObj = ObjectManager::instance()->find_by_name("Camera");

        if (cameraObj && cameraObj->transform())
        {
            // 1. 카메라의 위치, 앞 방향(Forward), 위 방향(Up)을 가져옴
            XMFLOAT3 pos = cameraObj->transform()->get_world_position();
            XMFLOAT3 forward = cameraObj->transform()->forward();
            XMFLOAT3 up = cameraObj->transform()->up();          

            // 2. FMOD 벡터 포맷으로 변환
            FMOD_VECTOR fmod_pos = { pos.x, pos.y, pos.z };
            FMOD_VECTOR fmod_vel = { 0.0f, 0.0f, 0.0f }; // 카메라 이동 속도는 0으로 둬도 무방
            FMOD_VECTOR fmod_forward = { forward.x, forward.y, forward.z };
            FMOD_VECTOR fmod_up = { up.x, up.y, up.z };

            // 3. FMOD 엔진의 귀를 이 카메라 위치에 부착! (첫 번째 인자 0은 1P를 의미)
            _system->set3DListenerAttributes(0, &fmod_pos, &fmod_vel, &fmod_forward, &fmod_up);
        }

        _system->update();
    }
}

void SoundManager::release()
{
    if (_system)
    {
        // 로드된 사운드 데이터들 메모리 해제
        for (auto& pair : _sounds)
        {
            if (pair.second)
            {
                pair.second->release();
            }
        }
        _sounds.clear();
        _channels.clear();
        _stopTimers.clear();

        // FMOD 시스템 종료 및 해제
        _system->close();
        _system->release();
        _system = nullptr;

        CLOG("[INFO] FMOD SoundManager Released.");
    }
}

bool SoundManager::check_fmod_error(FMOD_RESULT result, const std::string& errorContext)
{
    if (result != FMOD_OK)
    {
        // CERROR 매크로를 사용하여 FMOD 에러 코드와 컨텍스트 출력
        CERROR("FMOD Error [" << errorContext << "] : " << FMOD_ErrorString(result));
        return false;
    }
    return true;
}

void SoundManager::load_sound(const std::string& name, const std::string& filepath, bool is_3d)
{
    if (_sounds.contains(name)) return;

    FMOD_MODE mode = FMOD_DEFAULT;
    mode |= (is_3d) ? FMOD_3D : FMOD_2D; // 3D 여부 결정 
    // DW설명 : Fmod는 내부적으로 사운드 파이프라인을 설정할 때 2D와 3D 사운드를 구분지어 처리함
    // 이 구분을 런타임에 해주려고 하였으나, 3D 사운드를 재생하려면 무거운 계산 노드가 포함된 파이프라인으로 만들어줌
    // 이걸 만드는 과정에서 오버헤드가 발생하기 때문에 로드할 때 정해주도록 제작하였음


    FMOD::Sound* sound = nullptr;
    FMOD_RESULT result = _system->createSound(filepath.c_str(), mode, nullptr, &sound);

    // Fmod 실패 시 에러 체크 하는 함수이다.
    if (check_fmod_error(result, "Load Sound: " + name))
    {
        _sounds[name] = sound; // 우리는 사운드를 이름으로 관리
        CINFO("Sound Loaded: " << name);
    }
}

void SoundManager::set_group_volume(SoundType type, float volume)
{
    // BGM 또는 SFX 전체 볼륨을 한 번에 조절
    FMOD::ChannelGroup* targetGroup = (type == SoundType::BGM) ? _bgmGroup : _sfxGroup;
    if (targetGroup)
    {
        targetGroup->setVolume(volume);
    }
}

void SoundManager::set_master_volume(float volume)
{
    // 게임 전체 사운드 크기 조절
    if (_masterGroup)
    {
        _masterGroup->setVolume(volume);
    }
}

void SoundManager::play(const std::string& name, SoundType type, float volume, bool is_loop)
{
    if (!_sounds.contains(name)) return;

    FMOD::Channel* channel = nullptr;
    _system->playSound(_sounds[name], nullptr, false, &channel);

    if (channel)
    {
        // 1. 어느 그룹에 속할지 설정 (BGM인지 SFX인지)
        FMOD::ChannelGroup* targetGroup = (type == SoundType::BGM) ? _bgmGroup : _sfxGroup;
        channel->setChannelGroup(targetGroup);

        // 2. 볼륨 셋팅
        channel->setVolume(volume);

        // 재생할 때 채널의 모드를 변경해서 루프를 제어 -> Fmod는 채널 단위로 루프 여부를 설정함
        if (is_loop)
        {
            channel->setMode(FMOD_LOOP_NORMAL);
            channel->setLoopCount(-1); // -1은 무한 반복을 의미
        }
        else
        {
            channel->setMode(FMOD_LOOP_OFF);
        }

        _channels[name] = channel;
    }
}

void SoundManager::play_3d(const std::string& name, const XMFLOAT3& position, SoundType type, float volume, bool is_loop)
{
	if (!_sounds.contains(name)) return; // 사운드가 로드되어 있지 않으면 재생하지 않음

    FMOD::Channel* channel = nullptr;
    _system->playSound(_sounds[name], nullptr, true, &channel); // 시작할 땐 일시정지 해야한다. // 시작할 준비를 하고 시작할 채널 받기 -> 채널 설정을 위함

    if (channel)
    {
        // 1. 어느 그룹에 속할지 설정 (BGM인지 SFX인지)
        FMOD::ChannelGroup* targetGroup = (type == SoundType::BGM) ? _bgmGroup : _sfxGroup;
        channel->setChannelGroup(targetGroup);

        // 2. 위치 및 볼륨 셋팅
        FMOD_VECTOR fmod_pos = { position.x, position.y, position.z };
        FMOD_VECTOR fmod_vel = { 0.0f, 0.0f, 0.0f };
        channel->set3DAttributes(&fmod_pos, &fmod_vel);
        channel->setVolume(volume);

        // 루프 셋팅 추가
        if (is_loop)
        {
            channel->setMode(FMOD_LOOP_NORMAL);
            channel->setLoopCount(-1);
        }
        else
        {
            channel->setMode(FMOD_LOOP_OFF);
        }

        channel->setPaused(false); // 셋팅 끝났으니 발사!
        _channels[name] = channel;
    }
}

void SoundManager::stop(const std::string& name)
{
	if (!_channels.contains(name)) return;
    FMOD::Channel* channel = _channels[name];
    if (channel)
    {
        bool isPlaying = false;
        // FMOD_OK 확인 후 재생 중일 때만 stop 호출하여 댕글링 포인터로 인한 크래시 방지
        if (channel->isPlaying(&isPlaying) == FMOD_OK && isPlaying)
        {
            channel->stop();
        }
    }
    _channels.erase(name); // 채널 목록에서 제거
}

void SoundManager::stop_all()
{
    for (auto& pair : _channels)
    {
        if (pair.second)
            pair.second->stop();
    }
    _channels.clear();
    _stopTimers.clear();
}

bool SoundManager::is_playing(const std::string& name)
{
    // 채널 목록에 해당 이름이 없으면 재생 중이 아님
    if (!_channels.contains(name)) return false;

    FMOD::Channel* channel = _channels[name];
    if (channel)
    {
        bool playing = false;
        channel->isPlaying(&playing); // FMOD 채널 상태 확인
        return playing;
    }

    return false;
}

float SoundManager::get_playback_position(const std::string& name)
{
    if (!_channels.contains(name)) return 0.0f;

    FMOD::Channel* channel = _channels[name];
    if (channel)
    {
        unsigned int pos = 0;
        // FMOD에게 현재 채널의 재생 위치를 밀리초(MS) 단위로 받아옵니다.
        channel->getPosition(&pos, FMOD_TIMEUNIT_MS);

        // 밀리초를 초(Seconds) 단위의 float으로 변환하여 반환
        return pos / 1000.0f;
    }
    return 0.0f;
}

unsigned int SoundManager::parse_time_to_ms(const std::string& timeStr)
{
    // hh:mm:ss:msms 포맷 파싱
    std::stringstream ss(timeStr);
    std::string token;
    std::vector<int> parts;

    while (std::getline(ss, token, ':'))
    {
        try {
            parts.push_back(std::stoi(token));
        } catch (...) {
            parts.push_back(0);
        }
    }

    unsigned int ms = 0;
    if (parts.size() == 4) // hh:mm:ss:msms
    {
        ms += parts[0] * 3600000;
        ms += parts[1] * 60000;
        ms += parts[2] * 1000;
        ms += parts[3];
    }
    else if (parts.size() == 3) // mm:ss:msms
    {
        ms += parts[0] * 60000;
        ms += parts[1] * 1000;
        ms += parts[2];
    }
    else if (parts.size() == 2) // ss:msms
    {
        ms += parts[0] * 1000;
        ms += parts[1];
    }
    else if (parts.size() == 1) // msms
    {
        ms += parts[0];
    }

    return ms;
}

void SoundManager::play_section(const std::string& name, const std::string& startTimeStr, const std::string& endTimeStr, SoundType type, float volume)
{
    unsigned int startMs = parse_time_to_ms(startTimeStr);
    unsigned int endMs = parse_time_to_ms(endTimeStr);

    _channels[name] = nullptr; // 기존 댕글링 포인터 방지
    play(name, type, volume, false); // 우선 재생시킴

    if (_channels.contains(name) && _channels[name])
    {
        FMOD::Channel* channel = _channels[name];
        bool isPlaying = false;
        if (channel->isPlaying(&isPlaying) == FMOD_OK)
        {
            channel->setPosition(startMs, FMOD_TIMEUNIT_MS);
        }

        if (endMs > startMs)
        {
            float durationSec = (endMs - startMs) / 1000.0f;
            _stopTimers[name] = durationSec;
        }
    }
}

void SoundManager::play_3d_section(const std::string& name, const XMFLOAT3& position, const std::string& startTimeStr, const std::string& endTimeStr, SoundType type, float volume)
{
    unsigned int startMs = parse_time_to_ms(startTimeStr);
    unsigned int endMs = parse_time_to_ms(endTimeStr);

    _channels[name] = nullptr; // 기존 댕글링 포인터 방지
    play_3d(name, position, type, volume, false);

    if (_channels.contains(name) && _channels[name])
    {
        FMOD::Channel* channel = _channels[name];
        bool isPlaying = false;
        if (channel->isPlaying(&isPlaying) == FMOD_OK)
        {
            channel->setPosition(startMs, FMOD_TIMEUNIT_MS);
        }

        if (endMs > startMs)
        {
            float durationSec = (endMs - startMs) / 1000.0f;
            _stopTimers[name] = durationSec;
        }
    }
}
