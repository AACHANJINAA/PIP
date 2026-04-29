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
    static void rayCheck(JPH::Vec3 rayOrigin, JPH::Vec3 dir, const JPH::PhysicsSystem* physicsSystem)
    {
#ifdef _DEBUG
        {

            // 1. 레이 설정: 위(100)에서 아래(-100)로 200만큼 쏨
            JPH::Vec3 rayDirection = dir * 200.0f; // 충분히 긴 거리로 레이 설정 (예: 100 유닛)
            MYLOG("[DEBUG] Starting Debug Raycast Test at  "
                "X: " << rayOrigin.GetX()
                << " Y: " << rayOrigin.GetY()
                << " Z: " << rayOrigin.GetZ());
            JPH::RRayCast ray(rayOrigin, rayDirection);

            JPH::RayCastResult result;

            // 2. 레이캐스트 실행 (NON_MOVING 레이어만 검사)
            // BroadPhaseLayers::NON_MOVING과 Layers::NON_MOVING 상수는 프로젝트 설정에 맞춰 확인 필요

            if (physicsSystem->GetNarrowPhaseQuery().CastRay(ray, result
                , JPH::SpecifiedBroadPhaseLayerFilter(BroadPhaseLayers::NON_MOVING)
                , JPH::SpecifiedObjectLayerFilter(Layers::NON_MOVING)))

            {
                // 3. 충돌 지점 계산
                JPH::Vec3 hitPos = ray.GetPointOnRay(result.mFraction);

                MYLOG("==========================================================");
                MYLOG("[DEBUG] !!! RAYCAST HIT SUCCESS !!!");
                MYLOG("[DEBUG] Hit Position - X: " << hitPos.GetX() << " Y: " << hitPos.GetY() << " Z: " <<
                    hitPos.GetZ());

                // 어떤 Body에 맞았는지 확인
                JPH::BodyLockRead lock(physicsSystem->GetBodyLockInterface(), result.mBodyID);
                if (lock.Succeeded())
                {
                    const JPH::Body& body = lock.GetBody();
                    MYLOG("[DEBUG] Hit Body ID: " << result.mBodyID.GetIndex());
                    MYLOG("[DEBUG] Hit Body Layer: " << (int)body.GetObjectLayer());
                }
                MYLOG("==========================================================");
            }
            else
            {
                MYLOG("==========================================================");
                MYLOG("[DEBUG] !!! RAYCAST FAILED !!! Nothing detected at this coordinate.");
                MYLOG("[DEBUG] Expected X:" << rayOrigin.GetX() << " Z: " << rayOrigin.GetZ() << " but ray passed through.");
                MYLOG("==========================================================");
            }
        }
#endif
    }
}