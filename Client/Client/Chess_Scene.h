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
};