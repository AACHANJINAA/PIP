#include "stdafx.h"
//#include "Camera.h"
//#include "Shader.h"
//#include <iostream>
//
//Camera::Camera()
//{
//	m_xmf4x4View = Matrix4x4::Identity();
//	m_xmf4x4Projection = Matrix4x4::Identity();
//	m_d3dViewport = { 0, 0, FRAME_BUFFER_WIDTH , FRAME_BUFFER_
// , 0.0f, 1.0f };
//	m_d3dScissorRect = { 0, 0, FRAME_BUFFER_WIDTH , FRAME_BUFFER_HEIGHT };
//	m_xmf3Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
//	m_xmf3Right = XMFLOAT3(1.0f, 0.0f, 0.0f);
//	m_xmf3Look = XMFLOAT3(0.0f, 0.0f, 1.0f);
//	m_xmf3Up = XMFLOAT3(0.0f, 1.0f, 0.0f);
//
//	GenerateProjectionMatrix(0.1f, 5000.0f, m_fFOVAngle);
//}
//
//Camera::~Camera()
//{
//}
//
//
//void Camera::CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList)
//{
//	m_pd3dcbCamera = ::CreateBufferResource(pd3dDevice, pd3dCommandList, nullptr, sizeof(CB_CAMERA_INFO), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);
//
//	D3D12_RANGE d3dReadRange{ 0, 0 };
//	m_pd3dcbCamera->Map(0, &d3dReadRange, reinterpret_cast<void**>(&m_pcbMappedCamera));
//
//}
//
//void Camera::UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList)
//{
//	// 뷰 행렬과 프로젝션 행렬을 Transpose하여 맵핑된 버퍼에 복사
//	XMStoreFloat4x4(&m_pcbMappedCamera->m_xmf4x4View, XMMatrixTranspose(XMLoadFloat4x4(&m_xmf4x4View)));
//	XMStoreFloat4x4(&m_pcbMappedCamera->m_xmf4x4Projection, XMMatrixTranspose(XMLoadFloat4x4(&m_xmf4x4Projection)));
//	m_pcbMappedCamera->m_f4Position = XMFLOAT4(m_xmf3Position.x, m_xmf3Position.y, m_xmf3Position.z, 1.0f);
//
//	// [중요] 실제 바인딩은 이 함수 또는 별도의 Set... 함수에서!
//	D3D12_GPU_VIRTUAL_ADDRESS cbGpuAddress = m_pd3dcbCamera->GetGPUVirtualAddress();
//	pd3dCommandList->SetGraphicsRootConstantBufferView(1, cbGpuAddress); // b1에 바인딩
//}
//
//void Camera::ReleaseShaderVariables()
//{
//
//}
//
//void Camera::SetViewport(int xTopLeft, int yTopLeft, int nWidth, int nHeight, float
//	fMinZ, float fMaxZ)
//{
//	m_d3dViewport.TopLeftX = float(xTopLeft);
//	m_d3dViewport.TopLeftY = float(yTopLeft);
//	m_d3dViewport.Width = float(nWidth); m_d3dViewport.Height = float(nHeight);
//	m_d3dViewport.MinDepth = fMinZ;
//	m_d3dViewport.MaxDepth = fMaxZ;
//}
//
//void Camera::SetScissorRect(LONG xLeft, LONG yTop, LONG xRight, LONG yBottom)
//{
//	m_d3dScissorRect.left = xLeft;
//	m_d3dScissorRect.top = yTop;
//	m_d3dScissorRect.right = xRight;
//	m_d3dScissorRect.bottom = yBottom;
//}
//
//void Camera::SetViewportsAndScissorRects(ID3D12GraphicsCommandList* pd3dCommandList)
//{
//	pd3dCommandList->RSSetViewports(1, &m_d3dViewport);
//	pd3dCommandList->RSSetScissorRects(1, &m_d3dScissorRect);
//}
//
//
//void Camera::SetFOVAngle(float fFOVAngle)
//{
//	m_fFOVAngle = fFOVAngle;
//	m_fProjectRectDistance = float(1.0f / tan(DegreeToRadian(fFOVAngle * 0.5f)));
//}
//
//void Camera::GenerateProjectionMatrix(float fNearPlaneDistance, float fFarPlaneDistance, float fFOVAngle)
//{
//	m_xmf4x4Projection = Matrix4x4::PerspectiveFovLH(XMConvertToRadians(fFOVAngle), m_fAspectRatio, fNearPlaneDistance, fFarPlaneDistance);
//
//	GenerateFrustum();
//}
//
//void Camera::GenerateFrustum() 
//{
//	m_xmFrustumWorld.CreateFromMatrix(m_xmFrustumWorld, XMLoadFloat4x4(&m_xmf4x4Projection));
//	XMMATRIX InversView = XMMatrixInverse(nullptr, XMLoadFloat4x4(&m_xmf4x4View));
//	m_xmFrustumWorld.Transform(m_xmFrustumWorld, InversView);
//}
//
//bool Camera::IsInFrustum(BoundingOrientedBox& xmBoundingBox)
//{
//	return(m_xmFrustumWorld.Intersects(xmBoundingBox));
//}
//
//void Camera::SetLookAt(XMFLOAT3& xmf3LookAt, XMFLOAT3& xmf3Up)
//{
//	XMFLOAT4X4 xmf4x4View = Matrix4x4::LookAtLH(m_xmf3Position, xmf3LookAt, xmf3Up);
//}
//
//void Camera::Move(const XMFLOAT3& xmf3Shift)
//{
// 	m_xmf3Position = Vector3::Add(m_xmf3Position, xmf3Shift);
//}
//
//void Camera::Move(float x, float y, float z)
//{
//	Move(XMFLOAT3(x, y, z));
//}
//
//void Camera::SetPosition(XMFLOAT3 Position)
//{
//	m_xmf3Position = Position;
//}
//
//void Camera::SetPosition(float x, float y, float z)
//{
//	m_xmf3Position = XMFLOAT3(x,y,z);;
//}
//
//void Camera::Rotate(float fPitchX, float fYawY, float fRollZ)
//{
//	m_fRotate_X += fPitchX;
//	m_fRotate_Y += fYawY;
//	m_fRotate_Z += fRollZ;
//}
//
//void Camera::LookTo(XMFLOAT3& xmf3LookTo, XMFLOAT3& xmf3Up)
//{
//	XMFLOAT4X4 xmf4x4View = Matrix4x4::LookToLH(GetPosVec(), xmf3LookTo, xmf3Up);
//	_4x4World._11 = xmf4x4View._11; _4x4World._12 = xmf4x4View._21; _4x4World._13 = xmf4x4View._31;
//	_4x4World._21 = xmf4x4View._12; _4x4World._22 = xmf4x4View._22; _4x4World._23 = xmf4x4View._32;
//	_4x4World._31 = xmf4x4View._13; _4x4World._32 = xmf4x4View._23; _4x4World._33 = xmf4x4View._33;
//}
//
//void Camera::LookTo(XMFLOAT3& xmf3LookTo)
//{
//	XMFLOAT4X4 xmf4x4View = Matrix4x4::LookToLH(GetPosVec(), xmf3LookTo, GetUpVec());
//	_4x4World._11 = xmf4x4View._11; _4x4World._12 = xmf4x4View._21; _4x4World._13 = xmf4x4View._31;
//	_4x4World._21 = xmf4x4View._12; _4x4World._22 = xmf4x4View._22; _4x4World._23 = xmf4x4View._32;
//	_4x4World._31 = xmf4x4View._13; _4x4World._32 = xmf4x4View._23; _4x4World._33 = xmf4x4View._33;
//}
//
//
//void Camera::Update()
//{
//	Rotate();
//
//	// 룩업라이트 벡터를 정규직교하게 만들기
//	m_xmf3Look = Vector3::Normalize(m_xmf3Look);
//	m_xmf3Right = Vector3::Normalize(Vector3::CrossProduct(m_xmf3Up, m_xmf3Look));
//	m_xmf3Up = Vector3::Normalize(Vector3::CrossProduct(m_xmf3Look, m_xmf3Right));
//
//	// 카메라 룩업라이트의 전치 포지션은 마이너스
//	// 카메라 변환 행렬
//
//	m_xmf4x4View._11 = m_xmf3Right.x; m_xmf4x4View._21 = m_xmf3Right.y; m_xmf4x4View._31 = m_xmf3Right.z;
//	m_xmf4x4View._12 = m_xmf3Up.x; m_xmf4x4View._22 = m_xmf3Up.y; m_xmf4x4View._32 = m_xmf3Up.z;
//	m_xmf4x4View._13 = m_xmf3Look.x; m_xmf4x4View._23 = m_xmf3Look.y; m_xmf4x4View._33 = m_xmf3Look.z;
//	m_xmf4x4View._41 = -Vector3::DotProduct(m_xmf3Position, m_xmf3Right); m_xmf4x4View._42 = -Vector3::DotProduct(m_xmf3Position, m_xmf3Up); m_xmf4x4View._43 = -Vector3::DotProduct(m_xmf3Position, m_xmf3Look);
//	// 투영행렬들
//	m_xmf4x4Projection = m_xmf4x4Projection;
//
//	_4x4World._11 = m_xmf3Right.x; _4x4World._12 = m_xmf3Right.y; _4x4World._13 = m_xmf3Right.z;
//	_4x4World._21 = m_xmf3Up.x; _4x4World._22 = m_xmf3Up.y; _4x4World._23 = m_xmf3Up.z;
//	_4x4World._31 = m_xmf3Look.x; _4x4World._32 = m_xmf3Look.y; _4x4World._33 = m_xmf3Look.z;
//	_4x4World._41 = m_xmf3Position.x; _4x4World._42 = m_xmf3Position.y; _4x4World._43 = m_xmf3Position.z;
//
//	m_xmFrustumView.Transform(m_xmFrustumWorld, XMLoadFloat4x4(&_4x4World));
//
//	GenerateFrustum();
//}
//
//
//void Camera::Rotate()
//{
//	m_xmf3Right = XMFLOAT3(1.0f, 0.0f, 0.0f);
//	m_xmf3Up = XMFLOAT3(0.0f, 1.0f, 0.0f);
//	m_xmf3Look = XMFLOAT3(0.0f, 0.0f, 1.0f);
//	
//
//	if (m_fRotate_Y != 0.0f)
//	{
//		XMFLOAT3 Yaxis = { 0.f, 1.f, 0.f };
//		XMMATRIX mtxRotate = XMMatrixRotationAxis(XMLoadFloat3(&Yaxis), XMConvertToRadians(m_fRotate_Y));
//		m_xmf3Look = Vector3::TransformNormal(m_xmf3Look, mtxRotate);
//		m_xmf3Right = Vector3::TransformNormal(m_xmf3Right, mtxRotate);
//	}
//
//	if (m_fRotate_X != 0.0f)
//	{
//		XMMATRIX mtxRotate = XMMatrixRotationAxis(XMLoadFloat3(&m_xmf3Right), XMConvertToRadians(m_fRotate_X));
//		m_xmf3Look = Vector3::TransformNormal(m_xmf3Look, mtxRotate);
//		m_xmf3Up = Vector3::TransformNormal(m_xmf3Up, mtxRotate);
//	}
//	
//	if (m_fRotate_Z != 0.0f)
//	{
//		XMMATRIX mtxRotate = XMMatrixRotationAxis(XMLoadFloat3(&m_xmf3Look), XMConvertToRadians(m_fRotate_Z));
//		m_xmf3Up = Vector3::TransformNormal(m_xmf3Up, mtxRotate);
//		m_xmf3Right = Vector3::TransformNormal(m_xmf3Right, mtxRotate);
//	}
//
//}
//
//
