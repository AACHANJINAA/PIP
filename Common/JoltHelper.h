#pragma once
#include <Jolt/Jolt.h>
#include "Vector3.h"

namespace PIP::Utils
{
    // Common -> Jolt
    inline JPH::Vec3 ToJolt(const common::Vec3& v)
    {
        return {v.x, v.y, v.z};
    }

    inline JPH::Quat ToJolt(const common::Quat& q)
    {
        return { q.x, q.y, q.z, q.w };
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

    inline JPH::RMat44 ToJolt(const XMFLOAT4X4& m)
    {
		JPH::Vec4Arg row1 = { m._11, m._12, m._13, m._14 };
		JPH::Vec4Arg row2 = { m._21, m._22, m._23, m._24 };
		JPH::Vec4Arg row3 = { m._31, m._32, m._33, m._34 };
		JPH::Vec4Arg row4 = { m._41, m._42, m._43, m._44 };
        return JPH::RMat44{
            row1,
            row2,
            row3,
            row4
		};
	}

}
