#pragma once
#include "Scene.h"

class Main_Scene : public Scene
{
public:
	using Scene::Scene;
	Main_Scene() = default;
	virtual ~Main_Scene() = default;

	// --- Scene의 순수 가상 함수 오버라이드 ---
	virtual void build_objects(ID3D12Device* device, ID3D12GraphicsCommandList* commandList) override;
	virtual void release_upload_buffers() override;
	virtual void scene_process(float deltaTime) override;
	void Spawn_UI(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
	void Spawn_Monster_HP_UI(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
	void Spawn_Lever(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);

	void TestMesh(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);

	//void Spawn_Player(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
	//void Spawn_Test_NPCs(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
};