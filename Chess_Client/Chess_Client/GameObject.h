#pragma once
#include "Mesh.h"
#include "Camera.h"
#include <vector>
#include <memory>
#include <algorithm>
#include "TransformComponent.h"
class Shader;


enum MESH_TYPE {
	PLAYER,
	ENEMY
};

// (추가) MATERIAL 구조체 [PONG]
struct MATERIAL
{
	XMFLOAT4 m_xmf4Ambient;     // 환경광(Ambient) 
	XMFLOAT4 m_xmf4Diffuse;     // 난반사(Diffuse) 
	XMFLOAT4 m_xmf4Specular;    // 정반사(Specular)
	XMFLOAT4 m_xmf4Emissive;    // 방출광(Emissive)
};

// (추가) Material_Shader 클래스 [PONG]
class Material_Shader
{
public:
	Material_Shader();
	virtual ~Material_Shader();

private:
	int m_nReferences = 0;
	ID3D12RootSignature* _RootSignature = nullptr;
public:
	void AddRef() { m_nReferences++; }
	void Release() { 
		if (--m_nReferences <= 0) 
			delete this;
	}

	// Material_Shader은 재질 정보 + 셰이더
	MATERIAL* m_pMaterial = NULL;
	std::shared_ptr<Shader> _Shader = NULL;

	void SetShader(std::shared_ptr<Shader> pShader);
	void SetShaderRootSignature(ID3D12RootSignature* RootSignature);
	void SetRootSignature(ID3D12GraphicsCommandList* pd3dCommandList);
};

class HPObject // HPObject 클래스는 HP와 MaxHP를 관리하는 기본 클래스
{
	short _hp;
	short _max_hp;
public:
	HPObject(short hp = 100, short max_hp = 100) : _hp(hp), _max_hp(max_hp) {}
	virtual ~HPObject() = default; // 가상 소멸자 => 파생 클래스에서 소멸자 호출 가능 = 무조건 가상테이블 생성됨(8바이트)

	void SetHP(short hp) { _hp = hp; }
	short GetHP() const { return _hp; }

	void SetMaxHP(short max_hp) { _max_hp = max_hp; }
	short GetMaxHP() const { return _max_hp; }

	bool IsDead() const { return _hp <= 0; }
};

// 셰이더의 cbuffer 구조체와 1:1로 대응하는 C++ 구조체
struct CB_GAMEOBJECT_INFO
{
	XMFLOAT4X4 _4x4World;
};


class GameObject
{
public:
	GameObject();
	virtual ~GameObject();
private:
	int m_nReferences = 0;
public:
	void AddRef() { m_nReferences++; }
	void Release() { if (--m_nReferences <= 0) delete this; }

	MESH_TYPE m_Mesh_Type{}; // 메쉬 어떤걸 원하는지?

	BoundingOrientedBox m_xmOOBB = BoundingOrientedBox();
	bool m_Delete{}; // 객체를 삭제해야 하는지?
	bool m_bCollision{}; // 충돌하였는지?

	GameObject* m_pObjectCollided = NULL; // 누구랑 박은건지?
	XMFLOAT4	m_dwColor = { 0.f,0.f,0.f,0.f };

	XMFLOAT3 m_xmf3Gravity; // 중력
	float Gravity = -2.0f; // 중력값
	bool m_bGravity = true;

private:
	ComPtr<ID3D12Resource> m_pd3dcbGameObject;
	// 맵핑된 상수 버퍼의 CPU 주소 (매 프레임 여기다 데이터 복사)
	CB_GAMEOBJECT_INFO* _pcbMappedGameObject = nullptr;

public:
	Mesh* m_pMesh = NULL;
	//Shader* m_pShader = NULL; 
	Material_Shader* m_pMaterial = NULL; // 쉐이더 대신 머터리얼 [PONG]

protected:
	BoundingFrustum				m_xmFrustumView = BoundingFrustum();
	BoundingFrustum				m_xmFrustumWorld = BoundingFrustum();

public:
	void ReleaseUploadBuffers(); 
	virtual void SetMesh(Mesh* pMesh);
	virtual void SetShader(std::shared_ptr<Shader> pShader);
	void SetMaterial(Material_Shader* pMaterial); // 쉐이더 대신 머터리얼 [PONG]
	virtual void Animate(float fTimeElapsed, Camera* pCamera, ID3D12GraphicsCommandList* pd3dCommandList) = 0;
	virtual void Collision(float fElapsedTime) = 0;
	virtual void ProcessInput(float fElapsedTime) = 0;
	virtual void OnPrepareRender(ID3D12GraphicsCommandList* pd3dCommandList);
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList, Camera* pCamera);

	//상수 버퍼를 생성한다. 
	virtual void CreateShaderVariables(ID3D12Device *pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	//상수 버퍼의 내용을 갱신한다. 
	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList *pd3dCommandList);
	virtual void ReleaseShaderVariables();

	//게임 객체의 중력을 나타낸다.
	void SetGravity(XMFLOAT3& xmf3Gravity) { m_xmf3Gravity = xmf3Gravity; }

	void GenerateRayForPicking(XMVECTOR& xmvPickPosition, XMMATRIX& xmmtxView, XMVECTOR& xmvPickRayOrigin, XMVECTOR& xmvPickRayDirection);

	int PickObjectByRayIntersection(XMVECTOR& xmvPickPosition, XMMATRIX& xmmtxView, float* pfHitDistance);

	bool PickModelOBB(XMVECTOR& xmPickPosition, XMMATRIX& xmmtxView, float* pfHitDistance);// 모델좌표계의 OBB와 충돌했는지 알려주는 함수 삼각형 검사는 안함
	void UpdateBoundingBox(); // DW설명 : OOBB바운딩 박스를 업데이트 한다. 즉 회전같은 것들을 업데이트함

	bool IsVisible(Camera* pCamera = NULL);

	void Update(float DeltaTime);

public:
	int m_PosX{};
	int m_PosY{};

public:
	std::vector<std::shared_ptr<Component>> _components;

	template<typename T, typename... Args>
	std::shared_ptr<T> AddComponent(Args&&... args) {
		std::shared_ptr<T> newComponent = std::make_shared<T>(std::forward<Args>(args)...);
		_components.emplace_back(newComponent);
		newComponent->Start();
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