#pragma once
#include "Timer.h"
#include "Gameobject.h"
#include "Camera.h"
#include "Shader.h"

// Scene
#include <memory>
#include <vector>

class CGameObject;
class CObjectsShader;

class CScene
{
public:
	CScene();
	~CScene();

	bool OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	bool OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	void BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	void ReleaseObjects();
	bool ProcessInput(UCHAR* pKeysBuffer);
	void AnimateObjects(float fTimeElapsed);
	void Render(ID3D12GraphicsCommandList* pd3dCommandList, CCamera* pCamera);
	void ReleaseUploadBuffers();
	ID3D12RootSignature *CreateGraphicsRootSignature(ID3D12Device *pd3dDevice);ID3D12RootSignature *GetGraphicsRootSignature();
protected:
	std::vector<std::unique_ptr<CGameObject>> m_vObjects;

	std::vector<std::unique_ptr<CShader>>     m_vShaders;

	ID3D12RootSignature* m_pd3dGraphicsRootSignature = NULL;
};