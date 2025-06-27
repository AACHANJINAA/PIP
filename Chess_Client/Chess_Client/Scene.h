#pragma once
#include "stdafx.h"
#include "Timer.h"
#include "Shader.h"
#include "Camera.h"

class CScene
{
public:
	CScene();
	virtual ~CScene();
	//씬에서 마우스와 키보드 메시지를 처리한다. 
	bool OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	bool OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	virtual void BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList) = 0;
	virtual void ReleaseObjects() = 0;
	virtual void ProcessInput(float fElapsedTime, HWND hWnd, UINT nMessageID, POINT ptOldCursorPos) = 0;
	virtual void AnimateObjects(float fTimeElapsed, ID3D12GraphicsCommandList* pd3dCommandList) = 0;
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList) = 0;
	virtual void Collision(float fElapsedTime) = 0;
	void ReleaseUploadBuffers();
	//그래픽 루트 시그너쳐를 생성한다. 
	ID3D12RootSignature* CreateGraphicsRootSignature(ID3D12Device* pd3dDevice);
	ID3D12RootSignature* GetGraphicsRootSignature();

	// 충돌함수
	CGameObject* PickObjectPointedByCursor(int xClient, int yClient);


protected:
	//씬은 게임 객체들의 집합이다. 게임 객체는 셰이더를 포함한다. 

	ID3D12RootSignature* m_pd3dGraphicsRootSignature = NULL;

protected:
	//배치(Batch) 처리를 하기 위하여 씬을 셰이더들의 리스트로 표현한다. 
	CObjectsShader* m_pShaders = NULL;
	int m_nShaders = 0;

	// Cscene의 카메라
	CCamera* m_pCamera;

protected:
	// 키 입력을 위해 존재하는 것들
	BYTE NowKey[256]{};
	BYTE OldKey[256]{};

	POINT ptOldCursorPos{};

};

