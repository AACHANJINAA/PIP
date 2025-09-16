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
	void start() override;
	void update(float DeltaTime) override;
public:
	void set_position(float x, float y, float z);
	void set_position(XMFLOAT3 xmf3Position);
	void set_scale(float x, float y, float z);
	
	void rotate(float fPitch = 10.0f, float fYaw = 10.0f, float fRoll = 10.0f);
	void rotate(XMFLOAT3* pxmf3Axis, float fAngle);

	void move(XMFLOAT3& vDirection, float fSpeed);
	void move(float x, float y, float z);

	void look_to(XMFLOAT3& xmf3LookTo, XMFLOAT3& xmf3Up);
	void look_to(XMFLOAT3& xmf3LookTo);

	XMFLOAT3 get_position() const;
	XMFLOAT3 get_look() const;
	XMFLOAT3 get_size() const;
	XMFLOAT3 get_up() const;
	XMFLOAT3 get_right() const;
	XMFLOAT4X4 get_world_matrix() const;

protected:
	XMFLOAT4X4 _world = Matrix4x4::Identity();

	XMFLOAT3					_position = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3					_right = XMFLOAT3(1.0f, 0.0f, 0.0f);
	XMFLOAT3					_up = XMFLOAT3(0.0f, 1.0f, 0.0f);
	XMFLOAT3					_look = XMFLOAT3(0.0f, 0.0f, 1.0f);

	XMFLOAT3					_scale = XMFLOAT3(1.0f, 1.0f, 1.0f);
	XMFLOAT3					_rotate = XMFLOAT3(0.0f, 0.0f, 1.0f);
};

