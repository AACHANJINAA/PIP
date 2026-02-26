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
	void SpawnCamera();

	// [추가] 파일 탐색기를 띄우는 헬퍼 함수
	std::string OpenFileDialog();

	// [추가] 툴 씬에서 현재 조작 중인 타겟 객체 (캐릭터)
	std::shared_ptr<GameObject> m_targetCharacter = nullptr;
	std::string m_loadedCharacterPath = "None";
};

