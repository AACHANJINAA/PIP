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



	// 시네마틱 카메라 모드 관련 멤버 변수 및 함수
public:
	// 시네마틱에 관련된 UI와 오브젝트를 생성하는 함수
	void spawn_ui_and_object(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);

	// 시네마틱 연출 함수
	void cinematic_sequence(float deltaTime);

	void set_cinematic_mode(bool isCinematic);
	bool get_cinematic_mode() const { return _isCinematicMode; }

private:
	bool _isCinematicMode = false;
	bool _isCutsceneDoneSent = false;
	float _cinematicTimer = 0.0f;

	std::shared_ptr<GameObject> _blackBackground_ui_obj = {}; // 검정 페이드 아웃 UI
	std::vector<std::shared_ptr<GameObject>> _dummyPlayers; // 컷씬용 더미 플레이어들

	// 컷씬 플레이어 더미
	std::shared_ptr<GameObject> _dummy_player_1 = {}; // 플레이어 1 성 입구 쪽 바라보는 애
	std::shared_ptr<GameObject> _dummy_player_2 = {}; // 플레이어 2 1번 기준 오른쪽
	std::shared_ptr<GameObject> _dummy_player_3 = {}; // 플레이어 3 1번 기준 앞
	std::shared_ptr<GameObject> _dummy_player_4 = {}; // 플레이어 4 1번 기준 왼쪽

	//void Spawn_Player(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
	//void Spawn_Test_NPCs(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
};