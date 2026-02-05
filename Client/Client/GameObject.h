#pragma once
#include "Object.h"

class Component;
class Behavior;
class TransformComponent; // TransformComponent에 대한 전방 선언 추가
class GameObject : public Object, public std::enable_shared_from_this<GameObject>
{
public:
	GameObject(const std::string& name = "GameObject");
	virtual ~GameObject() = default;

	void init();

	void awake() const;
	void start() const;
	void update(float deltaTime) const;
	void fixed_update(float deltaTime) const;
	void late_update(float deltaTime);
	void destroy();

	void prepare_render() const;

	void on_collision_enter(const std::shared_ptr<GameObject>& other);
	void on_collision_stay(const std::shared_ptr<GameObject>& other);
	void on_collision_exit(const std::shared_ptr<GameObject>& other);

	// [변경] 레이어 타입을 uint32_t로 변경
	uint32_t layer_mask() const { return _layerMask; }

	// [변경] 이름으로 레이어를 설정하는 함수
	void set_layer(const std::string& name);

	// [추가] 특정 레이어에 속하는지 확인하는 함수
	bool is_in_layer(const std::string& name) const;
	// --- 편의 Getter ---
	// [변경] 반환 타입을 shared_ptr로 변경
	std::shared_ptr<TransformComponent> transform() const { return _transform; }

	// --- Component Management ---

	template<typename T, typename... Args>
	std::shared_ptr<T> add_component(Args&&... args)
	{
		// 이미 해당 타입의 컴포넌트가 있으면 추가하지 않고 기존 것을 반환
		auto existing = get_component<T>();
		if (existing)
			return existing;

		// 먼저, 이 컴포넌트가 요구하는 다른 컴포넌트들을 재귀적으로 추가합니다.
		add_required_components(static_cast<typename T::required_components*>(nullptr));

		// 그 다음, 원래 요청된 컴포넌트를 추가하고 반환합니다.
		auto new_component = std::make_shared<T>(std::forward<Args>(args)...);
		new_component->set_game_object(shared_from_this());
		_components.push_back(new_component);
		return new_component;
	}

	template<typename T>
	std::shared_ptr<T> get_component()
	{
		for (const auto& component : _components)
		{
			if (auto castedComponent = std::dynamic_pointer_cast<T>(component))
			{
				return castedComponent;
			}
		}
		return nullptr;
	}

	void remove_component(std::shared_ptr<Component> component);

private:
	// 의존성을 재귀적으로 추가하기 위한 템플릿 도우미 함수
	template <typename... T>
	void add_required_components(std::tuple<T...>*)
	{
		// 튜플의 0번째 인덱스부터 의존성 추가를 시작합니다.
		add_required_component_at<0, T...>();
	}

	// 2. 재귀적으로 호출되며 실제 작업을 수행하는 함수
	template <size_t I, typename... T>
	// 템플릿 인자 I가 튜플의 크기보다 작을 때만 이 함수가 선택되도록 합니다. (SFINAE)
	typename std::enable_if<(I < sizeof...(T))>::type add_required_component_at()
	{
		// 현재 인덱스(I)에 해당하는 컴포넌트의 타입을 가져옵니다.
		using ComponentType = typename std::tuple_element<I, std::tuple<T...>>::type;

		// 해당 타입의 컴포넌트가 이 게임오브젝트에 아직 없으면, 추가합니다.
		if (!get_component<ComponentType>())
		{
			add_component<ComponentType>();
		}

		// 다음 인덱스(I + 1)의 컴포넌트를 처리하기 위해 재귀 호출합니다.
		add_required_component_at<I + 1, T...>();
	}

	// 3. 재귀 호출을 종료하는 함수
	template <size_t I, typename... T>
	// 템플릿 인자 I가 튜플의 크기와 같아지면 이 함수가 선택되어 재귀가 멈춥니다.
	typename std::enable_if<(I == sizeof...(T))>::type add_required_component_at()
	{
		// 모든 의존성을 확인했으므로 아무것도 하지 않고 재귀를 종료합니다.
	}

	// 4. 의존성이 아예 없는 경우를 위한 함수
	void add_required_components(std::tuple<>*)
	{
		// 할 일 없음
	}
private:
	std::vector<std::shared_ptr<Component>> _components;
	std::shared_ptr<TransformComponent>     _transform; // 필수 컴포넌트인 Transform에 대한 빠른 접근 포인터
	uint32_t                                _layerMask = 0;
};
//public:
//	MeshType _meshType{}; // 메쉬 어떤걸 원하는지?
//
//	BoundingOrientedBox _orientedBoundingBox = BoundingOrientedBox();
//	bool _shouldDelete{}; // 객체를 삭제해야 하는지?
//	bool _isCollided{}; // 충돌하였는지?
//
//	std::shared_ptr<GameObject> _collidedObject; // 누구랑 박은건지?
//	XMFLOAT4	_color = { 0.f,0.f,0.f,0.f };
//
//	XMFLOAT3 _gravityVector; // 중력
//	float _gravityValue = -2.0f; // 중력값
//	bool _hasGravity = true;
//
//	int _posX{};
//	int _posY{};
//
//protected:
//	BoundingFrustum				_viewFrustum = BoundingFrustum();
//	BoundingFrustum				_worldFrustum = BoundingFrustum();
//
//public:
//	virtual void animate(float elapsed_time, Camera* camera, ID3D12GraphicsCommandList* command_list) = 0;
//	virtual void collision(float elapsed_time) = 0;
//	virtual void process_input(float elapsed_time) = 0;
//
//	//게임 객체의 중력을 나타낸다.
//	void SetGravity(const XMFLOAT3& xmf3Gravity) { _gravityVector = xmf3Gravity; }
//
//	//TODO: 피킹도 컴포넌트로 뺄것 or 스크립트 컴포넌트로 뺄것
//	void generate_ray_for_picking(XMVECTOR& pick_position, XMMATRIX& view_matrix, XMVECTOR& pick_ray_origin, XMVECTOR& pick_ray_direction);
//	int pick_object_by_ray_intersection(XMVECTOR& pick_position, XMMATRIX& view_matrix, float* hit_distance);
//	bool pick_model_obb(XMVECTOR& pick_position, XMMATRIX& view_matrix, float* hit_distance);// 모델좌표계의 OBB와 충돌했는지 알려주는 함수 삼각형 검사는 안함
//
//	void update_bounding_box(); // DW설명 : OOBB바운딩 박스를 업데이트 한다. 즉 회전같은 것들을 업데이트함
//
//	void update(float DeltaTime);
//
//public:
