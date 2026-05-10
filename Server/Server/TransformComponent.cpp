#include "pch.h"
#include "TransformComponent.h"
namespace PIP::GAME
{
	void TransformComponent::SmoothRotateTo(const common::Vec3& dir, float dt)
	{
		if (common::Length(dir) < 0.001f) return;

		// 1. 목표 회전값 (Y축 기준 회전)
		float targetAngle = std::atan2(dir.x, dir.z);
		DirectX::XMVECTOR targetQuat = DirectX::XMQuaternionRotationRollPitchYaw(0, targetAngle, 0);

		// 2. 현재 회전값
		DirectX::XMVECTOR currentQuat = DirectX::XMLoadFloat4((DirectX::XMFLOAT4*)&_rotation);

		// 3. 부드러운 보간 (Slerp)
		// t가 1.0f를 넘지 않도록 제한 (dt * speed)
		DirectX::XMVECTOR resultQuat = DirectX::XMQuaternionSlerp(currentQuat, targetQuat, std::min(1.0f, dt));

		// 4. 결과 저장
		DirectX::XMStoreFloat4((DirectX::XMFLOAT4*)&_rotation, resultQuat);
	}

	common::Vec3 TransformComponent::GetForward() const
	{
		// 1. 현재 회전값(Quaternion) 로드
		DirectX::XMVECTOR q = DirectX::XMLoadFloat4((const DirectX::XMFLOAT4*)&_rotation);

		// 2. 기본 정면 벡터(0, 0, 1) 로드
		DirectX::XMVECTOR forward = DirectX::XMLoadFloat3(&common::Vec3Forward);

		// 3. 쿼터니언을 이용해 벡터 회전
		DirectX::XMVECTOR rotatedForward = DirectX::XMVector3Rotate(forward, q);

		// 4. 결과 반환
		common::Vec3 result;
		DirectX::XMStoreFloat3(&result, rotatedForward);
		return result;
	}

	common::Vec3 TransformComponent::GetRight() const
	{
		// 1. 현재 회전값(Quaternion) 로드
		DirectX::XMVECTOR q = DirectX::XMLoadFloat4((const DirectX::XMFLOAT4*)&_rotation);
		// 2. 기본 오른쪽 벡터(1, 0, 0) 로드
		DirectX::XMVECTOR right = DirectX::XMLoadFloat3(&common::Vec3Right);
		// 3. 쿼터니언을 이용해 벡터 회전
		DirectX::XMVECTOR rotatedRight = DirectX::XMVector3Rotate(right, q);
		// 4. 결과 반환
		common::Vec3 result;
		DirectX::XMStoreFloat3(&result, rotatedRight);
		return result;
		
	}
}

