#pragma once
#include "Scene.h"

enum class SCENE_NUM
{
	SCENE_NONE = 0, // 변경 안할 때
	SCENE_CHESS, // 체스 씬
	SCENE_OTHER, // 다른 씬 (예: Pong 씬)
	// 추가 씬 번호를 여기에 정의할 수 있습니다.
};


class CSceneManager
{
public:
	CSceneManager();
	~CSceneManager();

	static CSceneManager* GetManager() {
		if (m_SceneManager == nullptr)
		{
			m_SceneManager = new CSceneManager{};
		}
		return m_SceneManager;
	}

	static void DeleteManager() {
		if (m_SceneManager != nullptr)
		{
			delete m_SceneManager;
		}
	}

	void ChangeScene();

	void ProcessInput(float fElapsedTime, HWND hWnd, UINT nMessageID, POINT ptOldCursorPos) {
		if (m_pCurrentScene.get()) {
			m_pCurrentScene.get()->ProcessInput(fElapsedTime, hWnd, nMessageID, ptOldCursorPos);
		}
	}

	void AnimateObjects(float fTimeElapsed, ID3D12GraphicsCommandList* pd3dCommandList) {
		if (m_pCurrentScene.get()) {
			m_pCurrentScene.get()->AnimateObjects(fTimeElapsed, pd3dCommandList);
		}
	}

	void Render(ID3D12GraphicsCommandList* pd3dCommandList) {
		if (m_pCurrentScene.get()) {
			m_pCurrentScene.get()->Render(pd3dCommandList);
		}
	}

	void Collision(float fElapsedTime) {
		if (m_pCurrentScene.get()) {
			m_pCurrentScene.get()->Collision(fElapsedTime);
		}
	}

private:
	static CSceneManager* m_SceneManager;
	std::unique_ptr<CScene> m_pCurrentScene = nullptr; // 현재 씬
	std::unique_ptr<CScene> m_pNextScene = nullptr; // 다음 씬

	SCENE_NUM m_WantScene = SCENE_NUM::SCENE_NONE; // 원하는 다음 씬 번호
};

