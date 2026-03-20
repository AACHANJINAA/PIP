#pragma once
#include "Scene.h"

class Boss_Scene : public Scene
{
public:
    Boss_Scene() = default;
    virtual ~Boss_Scene() = default;

    // --- Scene의 순수 가상 함수 오버라이드 ---
    virtual void build_objects(ID3D12Device* device, ID3D12GraphicsCommandList* commandList) override;
    virtual void release_upload_buffers() override;
    virtual void scene_process(float deltaTime) override;

    //void Spawn_Player(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
    //void Spawn_Test_NPCs(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
};