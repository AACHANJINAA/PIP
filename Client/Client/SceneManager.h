#pragma once
#include "Scene.h"

enum class SCENE_NUM
{
	SCENE_NONE = 0, // 변경 안할 때
	SCENE_CHESS, // 체스 씬
	SCENE_OTHER, // 다른 씬 (예: Pong 씬)
	// 추가 씬 번호를 여기에 정의할 수 있습니다.
};


class SceneManager : public Singleton<SceneManager>
{
	friend Singleton<SceneManager>;
	SceneManager();
	~SceneManager() override;
public:
	void initialize();
	virtual void release() override;

	// [추가] 템플릿을 사용하여 새로운 씬 생성 방법을 등록합니다.
	template<typename T>
	void register_scene(const std::string& scene_name)
	{
		// T는 반드시 Scene을 상속받는 클래스여야 합니다.
		static_assert(std::is_base_of<Scene, T>::value, "T must be a descendant of Scene");

		// scene_name을 키로, 해당 씬을 생성하는 람다 함수를 값으로 map에 저장합니다.
		_scene_creators[scene_name] = []() { return std::make_unique<T>(); };
	}
	// [추가] 새로운 씬으로 전환하는 것을 총괄하는 함수
	void change_scene(const std::string& scene_name);

	// (현재 씬을 반환하는 getter 등 다른 유틸리티 함수...)
	Scene* current_scene() const { return _currentScene.get(); }

	// [역할] GameFramework가 매 프레임 시작 시 호출하여, 씬 전환 요청이 있다면 처리합니다.
	void process_scene_change_if_requested(ID3D12Device* device, 
		ID3D12CommandAllocator* command_allocator, ID3D12GraphicsCommandList* command_list);
private:
	std::unique_ptr<Scene> _currentScene = nullptr; // 현재 씬
	std::string _requestedSceneName;
	// [추가] 씬의 이름(string)과 씬을 생성하는 함수(function)를 매핑하는 팩토리 맵
	std::unordered_map<std::string, std::function<std::unique_ptr<Scene>()>> _scene_creators;
};

