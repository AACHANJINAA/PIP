#pragma once
#include "Scene.h"
#include "FreeCamera.h"
#include "Shader.h"

class Chess_Scene : public Scene
{
public:
	Chess_Scene() {};
	Chess_Scene(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	virtual ~Chess_Scene();

public:
	// CScene을(를) 통해 상속됨
	void BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList) override;
	void ReleaseObjects() override;
	void ProcessInput(float fElapsedTime, HWND hWnd, UINT nMessageID, POINT ptOldCursorPos);
	void AnimateObjects(float fTimeElapsed, ID3D12GraphicsCommandList* pd3dCommandList) override;
	void Render(ID3D12GraphicsCommandList* pd3dCommandList) override;
	void Collision(float fElapsedTime) override;

	void ToggleBoundingBoxView() { isRenderFbxFileBoundingBoxes = !isRenderFbxFileBoundingBoxes; }

private:
	FreeCamera* m_ChessCamera{};

	bool isRenderFbxFileBoundingBoxes = false;
	std::vector<std::shared_ptr<GameObject>> debugObjects;

	std::shared_ptr<GameObject> _pFbxObject;
	ReadFbxMesh* _pCollisionMesh = nullptr;

	Shader* m_pDebugShader = nullptr;
};
