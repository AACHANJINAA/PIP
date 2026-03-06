#pragma once
#include "Scene.h"
#include "FreeCamera.h"
#include "Shader.h"
#include "ImGuiManager.h"

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
	void SpawnWantMesh();         // 1 메인 캐릭터 스폰
	void ViewBones();             // 2 뼈대 리스트 콤보박스 출력
	void SpawnWantSocketMesh();   // 3 소켓(무기) 메쉬 스폰 및 부착
	void EditSocketMesh();        // 4 소켓 트랜스폼 슬라이더 조절
	void DrawAndPickBones(); // 뼈대 리스트에서 선택한 뼈대를 씬에 시각적으로 표시하고 선택할 수 있게 하는 함수
	void DrawGizmo(); // 선택한 뼈대에 ImGuizmo를 이용해 위치/회전/스케일 조절 기능을 제공하는 함수

private:
	void SpawnCamera();

	// [추가] 파일 탐색기를 띄우는 헬퍼 함수
	std::string OpenFileDialog();

	// [추가] 툴 씬에서 현재 조작 중인 타겟 객체 (캐릭터)
	std::shared_ptr<GameObject> m_targetCharacter = nullptr;
	std::string m_loadedCharacterPath = "None";

private: // DW설명 : 소켓 에디터 관련 상태 변수들
	// 뼈대 리스트 및 선택 상태
	std::vector<std::string> m_boneNames;
	int m_selectedBoneIndex = 0;

	// 무기(소켓) 관련 상태
	std::string m_loadedWeaponPath = "None";
	std::shared_ptr<Mesh> m_weaponMesh = nullptr;

	// 조작할 로컬 Transform 수치들
	DirectX::XMFLOAT3 m_socketPos = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 m_socketRot = { 0.0f, 0.0f, 0.0f };
	DirectX::XMFLOAT3 m_socketScale = { 1.0f, 1.0f, 1.0f };

	// 뼈대를 화면에 그릴지 말지 결정하는 변수
	bool m_bShowBones = true;

	// 기즈모 조작 모드 (위치/회전/스케일)
	ImGuizmo::OPERATION m_currentGizmoOperation = ImGuizmo::TRANSLATE;
};

