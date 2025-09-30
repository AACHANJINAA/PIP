#pragma once
#include "Scene.h"
#include "FreeCamera.h"
#include "Shader.h"

// 전방 선언
class GameObject;

class Chess_Scene : public Scene
{
public:
    Chess_Scene() = default;
    virtual ~Chess_Scene() = default;

    // --- Scene의 순수 가상 함수 오버라이드 ---
    virtual void build_objects(ID3D12Device* device, ID3D12GraphicsCommandList* commandList) override;
    virtual void release_upload_buffers() override;

private:
    // [제거] m_ChessCamera, _AllShaders, _fbxObject 등 모든 멤버 변수를 제거합니다.
    // 이 객체들은 이제 ObjectManager가 관리하거나, GameFramework가 직접 소유합니다.
};


//class Chess_Scene : public Scene
//{
//public:
//	Chess_Scene() {};
//	Chess_Scene(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
//	virtual ~Chess_Scene();
//
//public:
//	// CScene을(를) 통해 상속됨
//	void BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList) override;
//	void ReleaseObjects() override;
//	void ProcessInput(float fElapsedTime);
//	void AnimateObjects(float fTimeElapsed, ID3D12GraphicsCommandList* pd3dCommandList) override;
//	void Render(ID3D12GraphicsCommandList* pd3dCommandList) override;
//	void Collision(float fElapsedTime) override;
//
//	void ToggleBoundingBoxView() { isRenderFbxFileBoundingBoxes = !isRenderFbxFileBoundingBoxes; }
//
//private:
//	FreeCamera* m_ChessCamera{};
//
//	bool isRenderFbxFileBoundingBoxes = false;
//	std::vector<std::shared_ptr<GameObject>> debugObjects;
//
//	std::shared_ptr<GameObject> _fbxObject;
//	std::shared_ptr<ReadFbxMesh> _collisionMesh;
//
//	Shader* m_pDebugShader = nullptr;
//};
