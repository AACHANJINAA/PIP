#include "stdafx.h"
#include "TransformComponent.h"

void TransformComponent::set_position(float x, float y, float z)
{
	_position = { x,y,z };
}
void TransformComponent::set_position(XMFLOAT3 xmf3Position)
{
	set_position(xmf3Position.x, xmf3Position.y, xmf3Position.z);
}


void TransformComponent::set_scale(float x, float y, float z)
{
	_scale = { x,y,z };
}

void TransformComponent::rotate(float fPitch, float fYaw, float fRoll)
{
	_rotate = { fPitch, fYaw, fRoll };
}

void TransformComponent::rotate(XMFLOAT3* pxmf3Axis, float fAngle)
{
	XMMATRIX mtxRotate = XMMatrixRotationAxis(XMLoadFloat3(pxmf3Axis),
		XMConvertToRadians(fAngle));
	_world = Matrix4x4::Multiply(mtxRotate, _world);
}

void TransformComponent::move(XMFLOAT3& vDirection, float fSpeed)
{
	_position.x = _position.x + vDirection.x * fSpeed;
	_position.y = _position.y + vDirection.y * fSpeed;
	_position.z = _position.z + vDirection.z * fSpeed;
}

void TransformComponent::move(float x, float y, float z)
{
	_position.x += x;
	_position.y += y;
	_position.z += z;
}

void TransformComponent::look_to(XMFLOAT3& xmf3LookTo, XMFLOAT3& xmf3Up)
{
	XMFLOAT4X4 xmf4x4View = Matrix4x4::LookToLH(get_position(), xmf3LookTo, xmf3Up);
	_world._11 = xmf4x4View._11; _world._12 = xmf4x4View._21; _world._13 = xmf4x4View._31;
	_world._21 = xmf4x4View._12; _world._22 = xmf4x4View._22; _world._23 = xmf4x4View._32;
	_world._31 = xmf4x4View._13; _world._32 = xmf4x4View._23; _world._33 = xmf4x4View._33;
}

void TransformComponent::look_to(XMFLOAT3& xmf3LookTo)
{
	XMFLOAT4X4 xmf4x4View = Matrix4x4::LookToLH(get_position(), xmf3LookTo, get_up());
	_world._11 = xmf4x4View._11; _world._12 = xmf4x4View._21; _world._13 = xmf4x4View._31;
	_world._21 = xmf4x4View._12; _world._22 = xmf4x4View._22; _world._23 = xmf4x4View._32;
	_world._31 = xmf4x4View._13; _world._32 = xmf4x4View._23; _world._33 = xmf4x4View._33;
}

XMFLOAT3 TransformComponent::get_position() const
{
	return _position;
}

XMFLOAT3 TransformComponent::get_look() const
{
	return(Vector3::Normalize(XMFLOAT3(_world._31, _world._32, _world._33)));
}

XMFLOAT3 TransformComponent::get_size() const
{
	return _scale;
}

XMFLOAT3 TransformComponent::get_up() const
{
	return(Vector3::Normalize(XMFLOAT3(_world._21, _world._22, _world._23)));
}

XMFLOAT3 TransformComponent::get_right() const
{
	return(Vector3::Normalize(XMFLOAT3(_world._11, _world._12, _world._13)));
}

XMFLOAT4X4 TransformComponent::get_world_matrix() const
{
	return _world;
}


void TransformComponent::start()
{
}

void TransformComponent::update(float DeltaTime)
{
	_world = Matrix4x4::Identity();

	XMMATRIX scaleMatrix = XMMatrixScaling(_scale.x, _scale.y, _scale.z);
	XMMATRIX rotateMatrix = XMMatrixRotationRollPitchYaw(XMConvertToRadians(_rotate.x), XMConvertToRadians(_rotate.y), XMConvertToRadians(_rotate.z));
	XMMATRIX translateMatrix = XMMatrixTranslation(_position.x, _position.y, _position.z);

	XMMATRIX worldMatrix = scaleMatrix * rotateMatrix * translateMatrix;
	XMMATRIX zFlipworldMatrix = XMMatrixScaling(1.0f, 1.0f, -1.0f) * worldMatrix; // GLB 모델의 Z축 뒤집기
	XMStoreFloat4x4(&_world, worldMatrix);
}
