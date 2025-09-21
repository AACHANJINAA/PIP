#pragma once
#include <vector>
#include <memory>
#include <algorithm>
#include "TransformComponent.h"
#include "RenderComponent.h"

enum MeshType {
	PLAYER,
	ENEMY
};

class HPObject // TODO: 이것을 컴포넌트로 뺄것 <- 찬진스
{
	short _hp;
	short _maxHp;
public:
	HPObject(short hp = 100, short max_hp = 100) : _hp(hp), _maxHp(max_hp) {}
	virtual ~HPObject() = default; // 가상 소멸자 => 파생 클래스에서 소멸자 호출 가능 = 무조건 가상테이블 생성됨(8바이트)

	void SetHP(short hp) { _hp = hp; }
	short GetHP() const { return _hp; }

	void SetMaxHP(short max_hp) { _maxHp = max_hp; }
	short GetMaxHP() const { return _maxHp; }

	bool IsDead() const { return _hp <= 0; }
};

class GameObject
{
public:
	GameObject();
	virtual ~GameObject();

public:
	MeshType _meshType{}; // 메쉬 어떤걸 원하는지?

	BoundingOrientedBox _orientedBoundingBox = BoundingOrientedBox();
	bool _shouldDelete{}; // 객체를 삭제해야 하는지?
	bool _isCollided{}; // 충돌하였는지?

	std::shared_ptr<GameObject> _collidedObject; // 누구랑 박은건지?
	XMFLOAT4	_color = { 0.f,0.f,0.f,0.f };

	XMFLOAT3 _gravityVector; // 중력
	float _gravityValue = -2.0f; // 중력값
	bool _hasGravity = true;

	int _posX{};
	int _posY{};

protected:
	BoundingFrustum				_viewFrustum = BoundingFrustum();
	BoundingFrustum				_worldFrustum = BoundingFrustum();

public:
	virtual void animate(float elapsed_time, Camera* camera, ID3D12GraphicsCommandList* command_list) = 0;
	virtual void collision(float elapsed_time) = 0;
	virtual void process_input(float elapsed_time) = 0;

	//게임 객체의 중력을 나타낸다.
	void SetGravity(const XMFLOAT3& xmf3Gravity) { _gravityVector = xmf3Gravity; }

	//TODO: 피킹도 컴포넌트로 뺄것 or 스크립트 컴포넌트로 뺄것
	void generate_ray_for_picking(XMVECTOR& pick_position, XMMATRIX& view_matrix, XMVECTOR& pick_ray_origin, XMVECTOR& pick_ray_direction);
	int pick_object_by_ray_intersection(XMVECTOR& pick_position, XMMATRIX& view_matrix, float* hit_distance);
	bool pick_model_obb(XMVECTOR& pick_position, XMMATRIX& view_matrix, float* hit_distance);// 모델좌표계의 OBB와 충돌했는지 알려주는 함수 삼각형 검사는 안함

	void update_bounding_box(); // DW설명 : OOBB바운딩 박스를 업데이트 한다. 즉 회전같은 것들을 업데이트함

	void update(float DeltaTime);

public:

	std::vector<std::shared_ptr<Component>> _components;

	template<typename T, typename... Args>
	std::shared_ptr<T> add_component(Args&&... args) {
		std::shared_ptr<T> newComponent = std::make_shared<T>(std::forward<Args>(args)...);
		_components.emplace_back(newComponent);
		newComponent->start();
		return newComponent;
	}

	template<typename T>
	std::shared_ptr<T> get_component() {
		for (const auto& component : _components) {
			if (auto casting = std::dynamic_pointer_cast<T>(component)) {
				return casting;
			}
		}
		return nullptr;
	}
};