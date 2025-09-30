#pragma once
#include "Object.h"

class Component;
class Behaviour;
class TransformComponent; // TransformComponent에 대한 전방 선언 추가
class GameObject : public Object, public std::enable_shared_from_this<GameObject>
{
public:
    GameObject(const std::string& name = "GameObject");
    virtual ~GameObject() = default;

    void awake();
    void start();
    void update(float deltaTime);
    void fixed_update(float deltaTime);
    void late_update(float deltaTime);
    void destroy();
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
        static_assert(std::is_base_of<Component, T>::value, "T must be a descendant of Component");
        auto newComponent = std::make_shared<T>(std::forward<Args>(args)...);
        newComponent->set_game_object(this);
        _components.push_back(newComponent);
        return newComponent;
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
    std::vector<std::shared_ptr<Component>> _components;
    std::shared_ptr<TransformComponent> _transform; // 필수 컴포넌트인 Transform에 대한 빠른 접근 포인터
    uint32_t _layerMask = 0;
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
