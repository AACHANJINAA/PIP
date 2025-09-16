#pragma once
#include "Mesh.h"
#include "Camera.h"
#include <vector>
#include <memory>
#include <algorithm>
#include "TransformComponent.h"

class Shader;
enum MeshType {
	PLAYER,
	ENEMY
};

// (추가) MATERIAL 구조체 [PONG]
struct Material
{
	XMFLOAT4 _ambient;     // 환경광(Ambient) 
	XMFLOAT4 _diffuse;     // 난반사(Diffuse) 
	XMFLOAT4 _specular;    // 정반사(Specular)
	XMFLOAT4 _emissive;    // 방출광(Emissive)
};

// (추가) Material_Shader 클래스 [PONG]
class Material_Shader
{
public:
	Material_Shader();
	virtual ~Material_Shader();

private:
	int m_nReferences = 0;
	ID3D12RootSignature* _rootSignature = nullptr;
public://TODO: 참조 카운트 갖다 버리고 -> 스마트포인터로 바꿀것
	void AddRef() { m_nReferences++; }
	void Release() { 
		if (--m_nReferences <= 0) 
			delete this;
	}

	// Material_Shader은 재질 정보 + 셰이더
	Material* _material = nullptr;
	std::shared_ptr<Shader> _shader = nullptr;

	void set_shader(const std::shared_ptr<Shader>& shader);
	void set_shader_root_signature(ID3D12RootSignature* root_signature);
	void set_root_signature(ID3D12GraphicsCommandList* command_list);
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

// 셰이더의 cbuffer 구조체와 1:1로 대응하는 C++ 구조체
struct CbGameObjectInfo
{
	XMFLOAT4X4 _world;
};


class GameObject
{
public:
	GameObject();
	virtual ~GameObject();

private: //TODO: 참조 카운트 갖다 버리고 -> 스마트포인터로 바꿀것
	int m_nReferences = 0;
public:
	void AddRef() { m_nReferences++; }
	void Release() { if (--m_nReferences <= 0) delete this; }

	MeshType _meshType{}; // 메쉬 어떤걸 원하는지?

	BoundingOrientedBox _orientedBoundingBox = BoundingOrientedBox();
	bool _shouldDelete{}; // 객체를 삭제해야 하는지?
	bool _isCollided{}; // 충돌하였는지?

	GameObject* _collidedObject = nullptr; // 누구랑 박은건지?
	XMFLOAT4	_color = { 0.f,0.f,0.f,0.f };

	XMFLOAT3 _gravityVector; // 중력
	float _gravityValue = -2.0f; // 중력값
	bool _hasGravity = true;

private:
	ComPtr<ID3D12Resource> _cbGameObject;
	CbGameObjectInfo* _cbMappedGameObject = nullptr;

public:
	Mesh* _mesh = nullptr;
	//Shader* m_pShader = NULL; 
	Material_Shader* _materialShader = nullptr; // 쉐이더 대신 머터리얼 [PONG]

protected:
	BoundingFrustum				_viewFrustum = BoundingFrustum();
	BoundingFrustum				_worldFrustum = BoundingFrustum();

public:
	void release_upload_buffers(); 
	virtual void set_mesh(Mesh* mesh);
	virtual void set_shader(std::shared_ptr<Shader> shader);
	void set_material(Material_Shader* material); // 쉐이더 대신 머터리얼 [PONG]
	virtual void animate(float elapsed_time, Camera* camera, ID3D12GraphicsCommandList* command_list) = 0;
	virtual void collision(float elapsed_time) = 0;
	virtual void process_input(float elapsed_time) = 0;
	virtual void on_prepare_render(ID3D12GraphicsCommandList* command_List);
	virtual void render(ID3D12GraphicsCommandList* command_list, Camera* camera);

	//상수 버퍼를 생성한다. 
	virtual void CreateShaderVariables(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	//상수 버퍼의 내용을 갱신한다. 
	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList *pd3dCommandList);
	virtual void ReleaseShaderVariables();

	//게임 객체의 중력을 나타낸다.
	void SetGravity(const XMFLOAT3& xmf3Gravity) { _gravityVector = xmf3Gravity; }

	//TODO: 피킹도 컴포넌트로 뺄것 or 스크립트 컴포넌트로 뺄것
	void generate_ray_for_picking(XMVECTOR& pick_position, XMMATRIX& view_matrix, XMVECTOR& pick_ray_origin, XMVECTOR& pick_ray_direction);
	int pick_object_by_ray_intersection(XMVECTOR& pick_position, XMMATRIX& view_matrix, float* hit_distance);
	bool pick_model_obb(XMVECTOR& pick_position, XMMATRIX& view_matrix, float* hit_distance);// 모델좌표계의 OBB와 충돌했는지 알려주는 함수 삼각형 검사는 안함

	void update_bounding_box(); // DW설명 : OOBB바운딩 박스를 업데이트 한다. 즉 회전같은 것들을 업데이트함

	bool is_visible(Camera* camera = nullptr);

	void update(float DeltaTime);

public:
	int _posX{};
	int _posY{};

public:
	std::vector<std::shared_ptr<Component>> _components;

	template<typename T, typename... Args>
	std::shared_ptr<T> AddComponent(Args&&... args) {
		std::shared_ptr<T> newComponent = std::make_shared<T>(std::forward<Args>(args)...);
		_components.emplace_back(newComponent);
		newComponent->start();
		return newComponent;
	}

	template<typename T>
	std::shared_ptr<T> GetComponent() {
		for (const auto& component : _components) {
			if (auto casting = std::dynamic_pointer_cast<T>(component)) {
				return casting;
			}
		}
		return nullptr;
	}
};