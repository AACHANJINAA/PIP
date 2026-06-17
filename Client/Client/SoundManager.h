#pragma once
#include "stdafx.h"

enum class SoundType
{
    BGM,
    SFX
};

// DW설명 : 사운드를 관리한다 -> 3D 사운드도 재생 가능하게 제작하였음 로드할 때 설정해 주어야 함
class SoundManager : public Singleton<SoundManager>
{
    friend Singleton<SoundManager>;
private:
    SoundManager();
    ~SoundManager() override;

public:
    bool initialize();
    void update(float deltaTime);
    virtual void release() override;

    // 사운드 관련 에러 체크용 유틸리티 함수
    static bool check_fmod_error(FMOD_RESULT result, const std::string& errorContext);

public:
    // 음원 로드
    void load_sound(const std::string& name, const std::string& filepath, bool is_3d = false);

	// 볼륨 조절 함수 -> 그룹별 볼륨 조절과 마스터 볼륨 조절로 나누어서 제작하였음
    void set_group_volume(SoundType type, float volume);
    void set_master_volume(float volume);

	// 재생 (2D 또는 3D 사운드 구분하여 재생)
    void play(const std::string& name, SoundType type = SoundType::SFX, float volume = 1.0f, bool is_loop = false);
    void play_3d(const std::string& name, const XMFLOAT3& position, SoundType type = SoundType::SFX, float volume = 1.0f, bool is_loop = false);

	// 사운드 정지
	void stop(const std::string& name);
    void stop_all();

    // 문자열(hh:mm:ss:msms 또는 mm:ss:msms) 파싱 헬퍼
    static unsigned int parse_time_to_ms(const std::string& timeStr);

    // 구간 재생 (문자열 타이밍 기반)
    void play_section(const std::string& name, const std::string& startTimeStr, const std::string& endTimeStr, SoundType type = SoundType::SFX, float volume = 1.0f);
    void play_3d_section(const std::string& name, const XMFLOAT3& position, const std::string& startTimeStr, const std::string& endTimeStr, SoundType type = SoundType::SFX, float volume = 1.0f);

	// 재생중인지 확인하는 함수 -> 사운드 이름으로 현재 재생 중인 채널이 있는지 체크
    bool is_playing(const std::string& name);

    // 사운드의 현재 재생 위치(초 단위) 반환
    float get_playback_position(const std::string& name);

private:
    FMOD::System* _system = nullptr;

    // 사운드(메모리에 로드된 음원 데이터) 관리
    std::unordered_map<std::string, FMOD::Sound*> _sounds;

    // 채널(현재 재생 중인 소리) 관리
    std::unordered_map<std::string, FMOD::Channel*> _channels;

    // 구간 재생을 위한 채널 타이머 관리 (남은 재생 시간, 초 단위)
    std::unordered_map<std::string, float> _stopTimers;

    // 볼륨 그룹 관리를 위한 채널 그룹들
    FMOD::ChannelGroup* _masterGroup = nullptr;
    FMOD::ChannelGroup* _bgmGroup = nullptr;
    FMOD::ChannelGroup* _sfxGroup = nullptr;
};