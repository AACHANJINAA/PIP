#pragma once
#include "Scene.h"

enum class TITLE_SCENE_STATE
{
	RESOURCE_LOADING,
    OPENING_UI_SEQUENCE,
    OPENING_SEQUENCE,
    CONNECTING_SERVER,
    CONNECTED,
    END
};


class Title_Scene : public Scene
{
private:
    // DW설명 : 타이틀 씬 볼건지? -> 이거 true 하면 실제 타이틀 씬 연출을 볼 수 있다. -> 디버깅 빨리 하려면 false 두기
	bool _isYouWantSeeTitleScene = true;

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
    void spawn_resource(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
	void spawn_opening_sequence_object();
    static INT_PTR CALLBACK DialogProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

public:
	bool _isOpeningEnd = false; // 오프닝 연출이 끝났는지 여부를 추적하는 멤버 변수
	bool _isConnectedToServer = false; // 서버 연결 여부를 추적하는 멤버 변수

private:

	TITLE_SCENE_STATE _currentOpeningState = TITLE_SCENE_STATE::RESOURCE_LOADING; // 현재 타이틀 씬의 상태를 나타내는 멤버 변수

    bool _isOpeningUIEnd = false; // 맨 처음 오프닝 UI 연출 끝났는지?

	void Resource_Loading_Sequence(float deltaTime); // 리소스 로딩 연출

    void Opening_UI_Sequence(float deltaTime); // 오프닝 UI 연출 -> 메인화면 선택 UI 아님
	void Opening_Sequence(float deltaTime); // 오프닝 연출
	


private:
    // 관리할 ui 객체들
	std::shared_ptr<GameObject> _title_ui_obj = {}; // 타이틀 화면 UI
	std::shared_ptr<GameObject> _blackBackground_ui_obj = {}; // 타이틀 화면 UI
	std::shared_ptr<GameObject> _logo_ui_background_obj = {}; // slay the lord UI
	std::shared_ptr<GameObject> _controls_ui_obj = {}; // 조작법 UI

    //void Spawn_Player(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
    //void Spawn_Test_NPCs(ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
};