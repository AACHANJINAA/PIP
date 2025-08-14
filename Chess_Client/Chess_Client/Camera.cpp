#include "stdafx.h"
#include "Camera.h"
#include "Shader.h"
#include <iostream>

CCamera::CCamera()
{
	m_xmf4x4View = Matrix4x4::Identity();
	m_xmf4x4Projection = Matrix4x4::Identity();
	m_d3dViewport = { 0, 0, FRAME_BUFFER_WIDTH , FRAME_BUFFER_HEIGHT, 0.0f, 1.0f };
	m_d3dScissorRect = { 0, 0, FRAME_BUFFER_WIDTH , FRAME_BUFFER_HEIGHT };
	m_xmf3Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
	m_xmf3Right = XMFLOAT3(1.0f, 0.0f, 0.0f);
	m_xmf3Look = XMFLOAT3(0.0f, 0.0f, 1.0f);
	m_xmf3Up = XMFLOAT3(0.0f, 1.0f, 0.0f);

	GenerateProjectionMatrix(0.1f, 5000.0f, m_fFOVAngle);
}

CCamera::~CCamera()
{
}


void CCamera::CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
{

}

void CCamera::UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList)
{
	XMFLOAT4X4 xmf4x4View;
	XMStoreFloat4x4(&xmf4x4View, XMMatrixTranspose(XMLoadFloat4x4(&m_xmf4x4View)));
	//루트 파라메터 인덱스 1의
	pd3dCommandList->SetGraphicsRoot32BitConstants(1, 16, &xmf4x4View, 0);
	XMFLOAT4X4 xmf4x4Projection;
	XMStoreFloat4x4(&xmf4x4Projection,
		XMMatrixTranspose(XMLoadFloat4x4(&m_xmf4x4Projection)));
	pd3dCommandList->SetGraphicsRoot32BitConstants(1, 16, &xmf4x4Projection, 16);

	//XMFLOAT4 xmf4Position(m_xmf3Position.x, m_xmf3Position.y, m_xmf3Position.z, 1.f);
	pd3dCommandList->SetGraphicsRoot32BitConstants(1, 4, &xmf4x4Projection, 32);
}

void CCamera::ReleaseShaderVariables()
{

}

void CCamera::SetViewport(int xTopLeft, int yTopLeft, int nWidth, int nHeight, float
	fMinZ, float fMaxZ)
{
	m_d3dViewport.TopLeftX = float(xTopLeft);
	m_d3dViewport.TopLeftY = float(yTopLeft);
	m_d3dViewport.Width = float(nWidth); m_d3dViewport.Height = float(nHeight);
	m_d3dViewport.MinDepth = fMinZ;
	m_d3dViewport.MaxDepth = fMaxZ;
}

void CCamera::SetScissorRect(LONG xLeft, LONG yTop, LONG xRight, LONG yBottom)
{
	m_d3dScissorRect.left = xLeft;
	m_d3dScissorRect.top = yTop;
	m_d3dScissorRect.right = xRight;
	m_d3dScissorRect.bottom = yBottom;
}

void CCamera::SetViewportsAndScissorRects(ID3D12GraphicsCommandList* pd3dCommandList)
{
	pd3dCommandList->RSSetViewports(1, &m_d3dViewport);
	pd3dCommandList->RSSetScissorRects(1, &m_d3dScissorRect);
}


void CCamera::SetFOVAngle(float fFOVAngle)
{
	m_fFOVAngle = fFOVAngle;
	m_fProjectRectDistance = float(1.0f / tan(DegreeToRadian(fFOVAngle * 0.5f)));
}

void CCamera::GenerateProjectionMatrix(float fNearPlaneDistance, float fFarPlaneDistance, float fFOVAngle)
{
	m_xmf4x4Projection = Matrix4x4::PerspectiveFovLH(XMConvertToRadians(fFOVAngle),
		m_fAspectRatio, fNearPlaneDistance, fFarPlaneDistance);
}


bool CCamera::IsInFrustum(BoundingOrientedBox& xmBoundingBox)
{
	return(m_xmFrustumWorld.Intersects(xmBoundingBox));
}

void CCamera::SetLookAt(XMFLOAT3& xmf3LookAt, XMFLOAT3& xmf3Up)
{
	XMFLOAT4X4 xmf4x4View = Matrix4x4::LookAtLH(m_xmf3Position, xmf3LookAt, xmf3Up);
}


void CCamera::Move(const XMFLOAT3& xmf3Shift)
{
 	m_xmf3Position = Vector3::Add(m_xmf3Position, xmf3Shift);
}

void CCamera::Move(float x, float y, float z)
{
	Move(XMFLOAT3(x, y, z));
}

void CCamera::SetPosition(XMFLOAT3 Position)
{
	m_xmf3Position = Position;
}

void CCamera::SetPosition(float x, float y, float z)
{
	m_xmf3Position = XMFLOAT3(x,y,z);;
}

void CCamera::Rotate(float fPitchX, float fYawY, float fRollZ)
{
	m_fRotate_X += fPitchX;
	m_fRotate_Y += fYawY;
	m_fRotate_Z += fRollZ;

//	std::cout << m_fRotate_Y << std::endl;

}


void CCamera::LookTo(XMFLOAT3& xmf3LookTo, XMFLOAT3& xmf3Up)
{
	XMFLOAT4X4 xmf4x4View = Matrix4x4::LookToLH(GetPosVec(), xmf3LookTo, xmf3Up);
	m_xmf4x4World._11 = xmf4x4View._11; m_xmf4x4World._12 = xmf4x4View._21; m_xmf4x4World._13 = xmf4x4View._31;
	m_xmf4x4World._21 = xmf4x4View._12; m_xmf4x4World._22 = xmf4x4View._22; m_xmf4x4World._23 = xmf4x4View._32;
	m_xmf4x4World._31 = xmf4x4View._13; m_xmf4x4World._32 = xmf4x4View._23; m_xmf4x4World._33 = xmf4x4View._33;
}

void CCamera::LookTo(XMFLOAT3& xmf3LookTo)
{
	XMFLOAT4X4 xmf4x4View = Matrix4x4::LookToLH(GetPosVec(), xmf3LookTo, GetUpVec());
	m_xmf4x4World._11 = xmf4x4View._11; m_xmf4x4World._12 = xmf4x4View._21; m_xmf4x4World._13 = xmf4x4View._31;
	m_xmf4x4World._21 = xmf4x4View._12; m_xmf4x4World._22 = xmf4x4View._22; m_xmf4x4World._23 = xmf4x4View._32;
	m_xmf4x4World._31 = xmf4x4View._13; m_xmf4x4World._32 = xmf4x4View._23; m_xmf4x4World._33 = xmf4x4View._33;
}


void CCamera::Update()
{
	Rotate();

	// 룩업라이트 벡터를 정규직교하게 만들기
	m_xmf3Look = Vector3::Normalize(m_xmf3Look);
	m_xmf3Right = Vector3::Normalize(Vector3::CrossProduct(m_xmf3Up, m_xmf3Look));
	m_xmf3Up = Vector3::Normalize(Vector3::CrossProduct(m_xmf3Look, m_xmf3Right));

	// 카메라 룩업라이트의 전치 포지션은 마이너스
	// 카메라 변환 행렬

	m_xmf4x4View._11 = m_xmf3Right.x; m_xmf4x4View._21 = m_xmf3Right.y; m_xmf4x4View._31 = m_xmf3Right.z;
	m_xmf4x4View._12 = m_xmf3Up.x; m_xmf4x4View._22 = m_xmf3Up.y; m_xmf4x4View._32 = m_xmf3Up.z;
	m_xmf4x4View._13 = m_xmf3Look.x; m_xmf4x4View._23 = m_xmf3Look.y; m_xmf4x4View._33 = m_xmf3Look.z;
	m_xmf4x4View._41 = -Vector3::DotProduct(m_xmf3Position, m_xmf3Right); m_xmf4x4View._42 = -Vector3::DotProduct(m_xmf3Position, m_xmf3Up); m_xmf4x4View._43 = -Vector3::DotProduct(m_xmf3Position, m_xmf3Look);
	// 투영행렬들
	m_xmf4x4Projection = m_xmf4x4Projection;

	m_xmf4x4World._11 = m_xmf3Right.x; m_xmf4x4World._12 = m_xmf3Right.y; m_xmf4x4World._13 = m_xmf3Right.z;
	m_xmf4x4World._21 = m_xmf3Up.x; m_xmf4x4World._22 = m_xmf3Up.y; m_xmf4x4World._23 = m_xmf3Up.z;
	m_xmf4x4World._31 = m_xmf3Look.x; m_xmf4x4World._32 = m_xmf3Look.y; m_xmf4x4World._33 = m_xmf3Look.z;
	m_xmf4x4World._41 = m_xmf3Position.x; m_xmf4x4World._42 = m_xmf3Position.y; m_xmf4x4World._43 = m_xmf3Position.z;

	m_xmFrustumView.Transform(m_xmFrustumWorld, XMLoadFloat4x4(&m_xmf4x4World));
}


void CCamera::Rotate()
{
	m_xmf3Right = XMFLOAT3(1.0f, 0.0f, 0.0f);
	m_xmf3Up = XMFLOAT3(0.0f, 1.0f, 0.0f);
	m_xmf3Look = XMFLOAT3(0.0f, 0.0f, 1.0f);
	

	if (m_fRotate_Y != 0.0f)
	{
		XMFLOAT3 Yaxis = { 0.f, 1.f, 0.f };
		XMMATRIX mtxRotate = XMMatrixRotationAxis(XMLoadFloat3(&Yaxis), XMConvertToRadians(m_fRotate_Y));
		m_xmf3Look = Vector3::TransformNormal(m_xmf3Look, mtxRotate);
		m_xmf3Right = Vector3::TransformNormal(m_xmf3Right, mtxRotate);
	}

	if (m_fRotate_X != 0.0f)
	{
		XMMATRIX mtxRotate = XMMatrixRotationAxis(XMLoadFloat3(&m_xmf3Right), XMConvertToRadians(m_fRotate_X));
		m_xmf3Look = Vector3::TransformNormal(m_xmf3Look, mtxRotate);
		m_xmf3Up = Vector3::TransformNormal(m_xmf3Up, mtxRotate);
	}
	
	if (m_fRotate_Z != 0.0f)
	{
		XMMATRIX mtxRotate = XMMatrixRotationAxis(XMLoadFloat3(&m_xmf3Look), XMConvertToRadians(m_fRotate_Z));
		m_xmf3Up = Vector3::TransformNormal(m_xmf3Up, mtxRotate);
		m_xmf3Right = Vector3::TransformNormal(m_xmf3Right, mtxRotate);
	}


}


