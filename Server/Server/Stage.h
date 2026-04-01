#pragma once
namespace PIP::SERVER
{
    class Room;
    class Stage
    {
    public:
        virtual ~Stage() = default;

        // 1. 물리 지형 및 정적 오브젝트 초기화 (씬 전환 시 즉시 실행)
        virtual void on_initialize(Room* room) = 0;

        // 2. 플레이어 진입 및 NPC 스폰 (로딩 완료 후 실행)
        virtual void on_enter(Room* room) = 0;

        // 3. 스테이지 전용 로직 업데이트
        virtual void update(Room* room, float dt) = 0;

        // 4. 자원 정리 (스테이지 전환 시 실행)
        virtual void on_exit(Room* room) = 0;

        virtual std::string get_stage_name() const = 0;
        virtual const common::Vec3 get_spawn_pos() const = 0;
    };
}