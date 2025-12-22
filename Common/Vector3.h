#pragma once
#include <DirectXMath.h>
#include <DirectXCollision.h>

using namespace DirectX;

namespace common
{
    using Vec3 = DirectX::XMFLOAT3;
	using Vec4 = DirectX::XMFLOAT4;
	using Quat = DirectX::XMFLOAT4; // Quaternion도 XMFLOAT4로 표현
	constexpr Vec3 Vec3One = { 1.0f, 1.0f, 1.0f };
	constexpr Vec3 Vec3Zero = { 0.0f, 0.0f, 0.0f };
	constexpr Vec3 Vec3Up = { 0.0f, 1.0f, 0.0f };
	constexpr Vec3 Vec3Down = { 0.0f, -1.0f, 0.0f };
	constexpr Vec3 Vec3Left = { -1.0f, 0.0f, 0.0f };
	constexpr Vec3 Vec3Right = { 1.0f, 0.0f, 0.0f };
	constexpr Vec3 Vec3Forward = { 0.0f, 0.0f, 1.0f };
	constexpr Vec3 Vec3Backward = { 0.0f, 0.0f, -1.0f };


	inline float Length(const Vec3& v)
	{
		return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
	}
	inline Vec3 Normalize(const Vec3& v)
	{
		float len = Length(v);
		if (len <= FLT_EPSILON) return Vec3Zero;
		return { v.x / len, v.y / len, v.z / len };
	}
}
