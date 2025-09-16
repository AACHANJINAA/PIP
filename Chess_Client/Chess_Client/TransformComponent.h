#pragma once
#include "Component.h"

class TransformComponent : public Component
{
public:
	TransformComponent(GameObject* Owner)
		: Component(Owner)
	{
	}
	~TransformComponent() = default;
public:
	void Start() override;
	void Update(float DeltaTime) override;
public:
	void SetPosition(float x, float y, float z);
	void SetPosition(XMFLOAT3 xmf3Position);
	void SetScale(float x, float y, float z);
	
	void Rotate(float fPitch = 10.0f, float fYaw = 10.0f, float fRoll = 10.0f);
	void Rotate(XMFLOAT3* pxmf3Axis, float fAngle);

	void Move(XMFLOAT3& vDirection, float fSpeed);
	void Move(float x, float y, float z);

	void LookTo(XMFLOAT3& xmf3LookTo, XMFLOAT3& xmf3Up);
	void LookTo(XMFLOAT3& xmf3LookTo);

	XMFLOAT3 GetPosition() const;
	XMFLOAT3 GetLook() const;
	XMFLOAT3 GetSize() const;
	XMFLOAT3 GetUp() const;
	XMFLOAT3 GetRight() const;
	XMFLOAT4X4 GetWorldMatrix() const;

protected:
	XMFLOAT4X4 _4x4World = Matrix4x4::Identity();

	XMFLOAT3					_Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3					_Right = XMFLOAT3(1.0f, 0.0f, 0.0f);
	XMFLOAT3					_Up = XMFLOAT3(0.0f, 1.0f, 0.0f);
	XMFLOAT3					_Look = XMFLOAT3(0.0f, 0.0f, 1.0f);

	XMFLOAT3					_scale = XMFLOAT3(1.0f, 1.0f, 1.0f);
	XMFLOAT3					_Rotate = XMFLOAT3(0.0f, 0.0f, 1.0f);
};

