#pragma once
#include "pch.h"
#include <Jolt/Jolt.h>
#include "../../Common/Vector3.h" // 경로에 맞게 수정

namespace PIP::Utils
{
    // Common -> Jolt
    inline JPH::Vec3 ToJolt(const common::Vec3& v)
    {
        return {v.x, v.y, v.z};
    }

    inline JPH::Quat ToJolt(const common::Quat& q)
    {
        return {q.x, q.y, q.z, q.w};
    }

    // Jolt -> Common
    inline common::Vec3 FromJolt(const JPH::Vec3& v)
    {
        return {v.GetX(), v.GetY(), v.GetZ()};
    }

    inline common::Quat FromJolt(const JPH::Quat& q)
    {
        return {q.GetX(), q.GetY(), q.GetZ(), q.GetW()};
    }
}