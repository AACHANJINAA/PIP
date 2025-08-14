#pragma once

struct VS_CB_CAMERA_INFO
{
	XMFLOAT4X4 m_xmf4x4View;
	XMFLOAT4X4 m_xmf4x4Projection;
	XMFLOAT4 m_f4Position;
};


class CCamera
{
public:
	CCamera();
	virtual ~CCamera();


public:

	//카메라의 정보를 셰이더 프로그램에게 전달하기 위한 상수 버퍼를 생성하고 갱신한다. 
	virtual void CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	virtual void ReleaseShaderVariables();
	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList);


	void SetViewport(int xTopLeft, int yTopLeft, int nWidth, int nHeight, float fMinZ = 0.0f, float fMaxZ = 1.0f);
	void SetScissorRect(LONG xLeft, LONG yTop, LONG xRight, LONG yBottom);

	virtual void SetViewportsAndScissorRects(ID3D12GraphicsCommandList* pd3dCommandList);


	void SetFOVAngle(float fFOVAngle);


	void GenerateProjectionMatrix(float fNearPlaneDistance, float fFarPlaneDistance, float fFOVAngle);

	bool IsInFrustum(BoundingOrientedBox& xmBoundingBox);

	void SetLookAt(XMFLOAT3& xmf3LookAt, XMFLOAT3& xmf3Up);

	void Move(const XMFLOAT3& xmf3Shift);

	void Move(float x, float y, float z);

	void SetPosition(XMFLOAT3 Position);
	void SetPosition(float x, float y, float z);

	void Rotate(float fPitchX, float fYawY, float fRollZ);

	void LookTo(XMFLOAT3& xmf3LookTo, XMFLOAT3& xmf3Up);
	void LookTo(XMFLOAT3& xmf3LookTo);

	void Update();




	XMFLOAT3 GetLookVec() const { return m_xmf3Look; }
	XMFLOAT3 GetUpVec() const { return m_xmf3Up; }
	XMFLOAT3 GetPosVec() const { return m_xmf3Position; }
	XMFLOAT3 GetRightVec() const { return m_xmf3Right; }

	void Rotate();
	

protected:
	BoundingFrustum				m_xmFrustumView = BoundingFrustum();
	BoundingFrustum				m_xmFrustumWorld = BoundingFrustum();

	float						m_fFOVAngle = 90.0f;
	float						m_fProjectRectDistance = 1.0f;
	float						m_fAspectRatio = float(FRAME_BUFFER_WIDTH) / float(FRAME_BUFFER_HEIGHT);

	XMFLOAT3					m_xmf3Position = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3					m_xmf3Right = XMFLOAT3(1.0f, 0.0f, 0.0f);
	XMFLOAT3					m_xmf3Up = XMFLOAT3(0.0f, 1.0f, 0.0f);
	XMFLOAT3					m_xmf3Look = XMFLOAT3(0.0f, 0.0f, 1.0f);

	float						m_fRotate_X{};
	float						m_fRotate_Y{};
	float						m_fRotate_Z{};

	float						m_ElapseTime{};

public:
	XMFLOAT4X4					m_xmf4x4World = Matrix4x4::Identity(); // 카메라 월드행렬
	XMFLOAT4X4					m_xmf4x4View = Matrix4x4::Identity(); // 카메라 변환 행렬
	XMFLOAT4X4					m_xmf4x4Projection = Matrix4x4::Identity(); // 원근투영 행렬
	D3D12_VIEWPORT				m_d3dViewport;
	D3D12_RECT					m_d3dScissorRect;
};

