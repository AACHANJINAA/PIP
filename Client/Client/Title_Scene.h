#pragma once
#include "Scene.h"

class Title_Scene : public Scene
{
public:
    using Scene::Scene;
    Title_Scene() = default;
    virtual ~Title_Scene() = default;

    // --- Scene의 순수 가상 함수 오버라이드 ---
    virtual void build_objects(ID3D12Device* device, ID3D12GraphicsCommandList* commandList) override;
    virtual void release_upload_buffers() override;
    virtual void scene_process(float deltaTime) override;
    void Spawn_UI(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);

    bool InterRoom();

private:
    void Spawn_Resource(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
    static INT_PTR CALLBACK DialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

public:
	bool _isOpeningEnd = false; // 오프닝 연출이 끝났는지 여부를 추적하는 멤버 변수
	bool _isConnectedToServer = false; // 서버 연결 여부를 추적하는 멤버 변수

private:
	void Opening_Sequence(float deltaTime);

    //void Spawn_Player(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
    //void Spawn_Test_NPCs(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
};