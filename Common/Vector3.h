#pragma once
#include <DirectXMath.h>
#include <DirectXCollision.h>

using namespace DirectX;

namespace common
{
    using Vec3 = DirectX::XMFLOAT3;
	using Vec4 = DirectX::XMFLOAT4;
	using Quat = DirectX::XMFLOAT4; // Quaternion�� XMFLOAT4�� ǥ��
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
	inline float LengthSq(const Vec3& v)
	{
		return v.x * v.x + v.y * v.y + v.z * v.z;
	}
	inline Vec3 Normalize(const Vec3& v)
	{
		float lenSq = v.x * v.x + v.y * v.y + v.z * v.z;
		if (lenSq < 1e-6f || std::isinf(lenSq) || std::isnan(lenSq)) return Vec3Zero;
		float len = sqrtf(lenSq);
		return { v.x / len, v.y / len, v.z / len };
	}
	inline float Distance(const Vec3& v1, const Vec3& v2)
	{
		XMVECTOR vec = { v2.x - v1.x, v2.y - v1.y, v2.z - v1.z };
		XMVECTOR result = DirectX::XMVector3Length(vec);
		float dist;
		XMStoreFloat(&dist, result);
		return dist;
	}
	inline float DistanceSq(const Vec3& v1, const Vec3& v2)
	{
		XMVECTOR vec = { v2.x - v1.x, v2.y - v1.y, v2.z - v1.z };
		XMVECTOR result = DirectX::XMVector3LengthSq(vec);
		float distSq;
		XMStoreFloat(&distSq, result);
		return distSq;
	}
	inline bool XMVector3AnyNaN(XMVECTOR v)
	{
		return std::isnan(v.m128_f32[0]) || std::isnan(v.m128_f32[1]) || std::isnan(v.m128_f32[2]);
	}
	inline bool XMVector4AnyNaN(XMVECTOR v)
	{
		return std::isnan(v.m128_f32[0]) || std::isnan(v.m128_f32[1]) || std::isnan(v.m128_f32[2]) || std::isnan(v.m128_f32[3]);
	}
	inline bool IsEqual(const Vec3& v1, const Vec3& v2, float epsilon = 1e-6f)
	{
		return DistanceSq(v1, v2) < epsilon * epsilon;
	}
	inline bool IsEqual(const Vec4& v1, const Vec4& v2, float epsilon = 1e-6f)
	{
		float dx = v1.x - v2.x;
		float dy = v1.y - v2.y;
		float dz = v1.z - v2.z;
		float dw = v1.w - v2.w;
		return (dx * dx + dy * dy + dz * dz + dw * dw) < epsilon * epsilon;
	}
	namespace VectorHelper
	{
		inline Vec3 operator+(const Vec3& a, const Vec3& b)
		{
			return { a.x + b.x, a.y + b.y, a.z + b.z };
		}
		inline Vec3 operator+=(Vec3& a, const Vec3& b)
		{
			a.x += b.x; a.y += b.y; a.z += b.z;
			return a;
		}
		inline Vec3 operator*(const Vec3& v, float scalar)
		{
			return { v.x * scalar, v.y * scalar, v.z * scalar };
		}
		inline Vec3 operator-(const Vec3& a, const Vec3& b)
		{
			return { a.x - b.x, a.y - b.y, a.z - b.z };
		}
		inline Vec3 operator/(const common::Vec3& lhs, float rhs)
		{
			return { lhs.x / rhs, lhs.y / rhs, lhs.z / rhs };
		}
	}
	
}
