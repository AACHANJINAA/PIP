#include "stdafx.h"
#include "TransformComponent.h"

void TransformComponent::SetPosition(float x, float y, float z)
{
	_Position = { x,y,z };
}
void TransformComponent::SetPosition(XMFLOAT3 xmf3Position)
{
	SetPosition(xmf3Position.x, xmf3Position.y, xmf3Position.z);
}


void TransformComponent::SetScale(float x, float y, float z)
{
	_scale = { x,y,z };
}

void TransformComponent::Rotate(float fPitch, float fYaw, float fRoll)
{
	_Rotate = { fPitch, fYaw, fRoll };
}

void TransformComponent::Rotate(XMFLOAT3* pxmf3Axis, float fAngle)
{
	XMMATRIX mtxRotate = XMMatrixRotationAxis(XMLoadFloat3(pxmf3Axis),
		XMConvertToRadians(fAngle));
	_4x4World = Matrix4x4::Multiply(mtxRotate, _4x4World);
}

void TransformComponent::Move(XMFLOAT3& vDirection, float fSpeed)
{
	_Position.x = _Position.x + vDirection.x * fSpeed;
	_Position.y = _Position.y + vDirection.y * fSpeed;
	_Position.z = _Position.z + vDirection.z * fSpeed;
}

void TransformComponent::Move(float x, float y, float z)
{
	_Position.x += x;
	_Position.y += y;
	_Position.z += z;
}

void TransformComponent::LookTo(XMFLOAT3& xmf3LookTo, XMFLOAT3& xmf3Up)
{
	XMFLOAT4X4 xmf4x4View = Matrix4x4::LookToLH(GetPosition(), xmf3LookTo, xmf3Up);
	_4x4World._11 = xmf4x4View._11; _4x4World._12 = xmf4x4View._21; _4x4World._13 = xmf4x4View._31;
	_4x4World._21 = xmf4x4View._12; _4x4World._22 = xmf4x4View._22; _4x4World._23 = xmf4x4View._32;
	_4x4World._31 = xmf4x4View._13; _4x4World._32 = xmf4x4View._23; _4x4World._33 = xmf4x4View._33;
}

void TransformComponent::LookTo(XMFLOAT3& xmf3LookTo)
{
	XMFLOAT4X4 xmf4x4View = Matrix4x4::LookToLH(GetPosition(), xmf3LookTo, GetUp());
	_4x4World._11 = xmf4x4View._11; _4x4World._12 = xmf4x4View._21; _4x4World._13 = xmf4x4View._31;
	_4x4World._21 = xmf4x4View._12; _4x4World._22 = xmf4x4View._22; _4x4World._23 = xmf4x4View._32;
	_4x4World._31 = xmf4x4View._13; _4x4World._32 = xmf4x4View._23; _4x4World._33 = xmf4x4View._33;
}

XMFLOAT3 TransformComponent::GetPosition() const
{
	return _Position;
}

XMFLOAT3 TransformComponent::GetLook() const
{
	return(Vector3::Normalize(XMFLOAT3(_4x4World._31, _4x4World._32, _4x4World._33)));
}

XMFLOAT3 TransformComponent::GetSize() const
{
	return _scale;
}

XMFLOAT3 TransformComponent::GetUp() const
{
	return(Vector3::Normalize(XMFLOAT3(_4x4World._21, _4x4World._22, _4x4World._23)));
}

XMFLOAT3 TransformComponent::GetRight() const
{
	return(Vector3::Normalize(XMFLOAT3(_4x4World._11, _4x4World._12, _4x4World._13)));
}

XMFLOAT4X4 TransformComponent::GetWorldMatrix() const
{
	return _4x4World;
}

void TransformComponent::Start()
{
}

void TransformComponent::Update(float DeltaTime)
{
	_4x4World = Matrix4x4::Identity();

	XMMATRIX scaleMatrix = XMMatrixScaling(_scale.x, _scale.y, _scale.z);
	XMMATRIX rotateMatrix = XMMatrixRotationRollPitchYaw(XMConvertToRadians(_Rotate.x), XMConvertToRadians(_Rotate.y), XMConvertToRadians(_Rotate.z));
	XMMATRIX translateMatrix = XMMatrixTranslation(_Position.x, _Position.y, _Position.z);

	XMMATRIX worldMatrix = scaleMatrix * rotateMatrix * translateMatrix;
	//XMMATRIX zFlipworldMatrix = XMMatrixScaling(1.0f, 1.0f, -1.0f) * worldMatrix; // GLB 모델의 Z축 뒤집기
	XMStoreFloat4x4(&_4x4World, worldMatrix);
}
