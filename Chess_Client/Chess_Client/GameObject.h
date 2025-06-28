#pragma once
#include "Mesh.h"
#include "Camera.h"
class CShader;


enum WHAT_DO_YOU_WANT {
	I_WANT_CHESS_PLAYER,
	I_WANT_CHESS_ENEMY
};


class CGameObject
{
public:
	CGameObject();
	virtual ~CGameObject();
private:
	int m_nReferences = 0;
public:
	void AddRef() { m_nReferences++; }
	void Release() { if (--m_nReferences <= 0) delete this; }

	WHAT_DO_YOU_WANT m_Mesh_Type{}; // 메쉬 어떤걸 원하는지?

	BoundingOrientedBox m_xmOOBB = BoundingOrientedBox();
	bool m_Delete{}; // 객체를 삭제해야 하는지?
	bool m_bCollision{}; // 충돌하였는지?

	CGameObject* m_pObjectCollided = NULL; // 누구랑 박은건지?
	XMFLOAT4	m_dwColor = { 0.f,0.f,0.f,0.f };

	XMFLOAT3 m_xmf3Gravity; // 중력

	float Gravity = -2.0f; // 중력값

	bool m_bGravity = true;

public:
	XMFLOAT4X4 m_xmf4x4World;
	CMesh* m_pMesh = NULL;
	CShader* m_pShader = NULL;
public:
	void ReleaseUploadBuffers(); 
	virtual void SetMesh(CMesh* pMesh);
	virtual void SetShader(CShader* pShader);
	virtual void Animate(float fTimeElapsed, ID3D12GraphicsCommandList* pd3dCommandList) = 0;
	virtual void Collision(float fElapsedTime) = 0;
	virtual void ProcessInput(float fElapsedTime, HWND hWnd, UINT nMessageID, POINT ptOldCursorPos) = 0;
	virtual void OnPrepareRender();
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera);
public:
	//상수 버퍼를 생성한다. 
	virtual void CreateShaderVariables(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	//상수 버퍼의 내용을 갱신한다. 
	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList *pd3dCommandList);
	virtual void ReleaseShaderVariables();

	//게임 객체의 월드 변환 행렬에서 위치 벡터와 방향(x-축, y-축, z-축) 벡터를 반환한다. 
	XMFLOAT3 GetPosition();
	XMFLOAT3 GetLook();
	XMFLOAT3 GetSize();
	XMFLOAT3 GetUp();
	XMFLOAT3 GetRight();
	//게임 객체의 위치를 설정한다. 
	void SetPosition(float x, float y, float z);
	void SetPosition(XMFLOAT3 xmf3Position);

	//게임 객체의 크기를 설정한다.
	void SetScale(float x, float y, float z);

	//게임 객체의 중력을 나타낸다.
	void SetGravity(XMFLOAT3& xmf3Gravity) { m_xmf3Gravity = xmf3Gravity; }

	//게임 객체를 로컬 x-축, y-축, z-축 방향으로 이동한다.
	void MoveStrafe(float fDistance = 1.0f);
	void MoveUp(float fDistance = 1.0f);
	void MoveForward(float fDistance = 1.0f);
	//게임 객체를 회전(x-축, y-축, z-축)한다.
	void Rotate(float fPitch = 10.0f, float fYaw = 10.0f, float fRoll = 10.0f);

	void Move(XMFLOAT3& vDirection, float fSpeed);
	void Move(float x, float y, float z);
	void Move(XMFLOAT3& vMove);

	void LookTo(XMFLOAT3& xmf3LookTo, XMFLOAT3& xmf3Up);
	void LookTo(XMFLOAT3& xmf3LookTo);

	void GenerateRayForPicking(XMVECTOR& xmvPickPosition, XMMATRIX& xmmtxView, XMVECTOR& xmvPickRayOrigin, XMVECTOR& xmvPickRayDirection);

	int PickObjectByRayIntersection(XMVECTOR& xmvPickPosition, XMMATRIX& xmmtxView, float* pfHitDistance);

	bool PickModelOBB(XMVECTOR& xmPickPosition, XMMATRIX& xmmtxView, float* pfHitDistance);// 모델좌표계의 OBB와 충돌했는지 알려주는 함수 삼각형 검사는 안함
	void UpdateBoundingBox(); // DW설명 : OOBB바운딩 박스를 업데이트 한다. 즉 회전같은 것들을 업데이트함

public:
	void Rotate(XMFLOAT3* pxmf3Axis, float fAngle);

public:
	int m_PosX{};
	int m_PosY{};
};