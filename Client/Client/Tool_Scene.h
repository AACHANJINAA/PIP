#pragma once
#include "Scene.h"
#include "FreeCamera.h"
#include "Shader.h"

// 전방 선언
class GameObject;

class Tool_Scene : public Scene
{
public:
	Tool_Scene() = default;
	virtual ~Tool_Scene() = default;
	// --- Scene의 순수 가상 함수 오버라이드 ---
	virtual void build_objects(ID3D12Device* device, ID3D12GraphicsCommandList* commandList) override;
	virtual void release_upload_buffers() override;
	virtual void scene_process(float deltaTime) override;

private:
	// 테스트하고 싶은 객체들 넣기
	void Spawn_SK_MagicConstruct(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);

};

