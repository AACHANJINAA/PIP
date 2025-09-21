#pragma once
#include <memory>


class GameObject;
class Camera;

class Component
{
public:
	Component(GameObject* Owner) : _gameObject(Owner) {};
	virtual ~Component() = default;

	virtual void start();
	virtual void update(float DeltaTime);
	virtual void render(ComPtr<ID3D12GraphicsCommandList> command_list, Camera* camera);

	//std::shared_ptr<GameObject> get_GameObject() const { return _gameObject; }
	GameObject* get_GameObject() const { return _gameObject; }

protected:
	//std::shared_ptr<GameObject> _gameObject;
	GameObject* _gameObject; // 순환참조 해결을 위한 원시포인터,  이렇게 하면 Component는 GameObject의 주소만 가지고 있을 수 있음
};

