#pragma once
#include "Mesh.h"
#include "Camera.h"
class Shader;


enum MeshType {
	PLAYER,
	ENEMY
};

// (추가) MATERIAL 구조체 [PONG]
struct Material
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
	ID3D12RootSignature* _rootSignature = nullptr;
public:
	void AddRef() { m_nReferences++; }
	void Release() { 
		if (--m_nReferences <= 0) 
			delete this;
	}

	// Material_Shader은 재질 정보 + 셰이더
	Material* _material = nullptr;
	std::shared_ptr<Shader> _shader = nullptr;

	void SetShader(std::shared_ptr<Shader> pShader);
	void SetShaderRootSignature(ID3D12RootSignature* RootSignature);
	void SetRootSignature(ID3D12GraphicsCommandList* pd3dCommandList);
};

class HPObject // TODO: 이것을 컴포넌트로 뺄것 <- 찬진스
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
struct CbGameObjectInfo
{
	XMFLOAT4X4 m_xmf4x4World;
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
	// 월드 행렬을 담을 상수 버퍼 리소스
	ComPtr<ID3D12Resource> _cbGameObject;
	// 맵핑된 상수 버퍼의 CPU 주소 (매 프레임 여기다 데이터 복사)
	CbGameObjectInfo* _cbMappedGameObject = nullptr;

public:
	XMFLOAT4X4 _worldMatrix = Matrix4x4::Identity();
	Mesh* _mesh = nullptr;
	//Shader* m_pShader = NULL; 
	Material_Shader* _materialShader = nullptr; // 쉐이더 대신 머터리얼 [PONG]

protected:
	BoundingFrustum				_viewFrustum = BoundingFrustum();
	BoundingFrustum				_worldFrustum = BoundingFrustum();

	XMFLOAT3					_position = XMFLOAT3(0.0f, 0.0f, 0.0f);
	XMFLOAT3					_right = XMFLOAT3(1.0f, 0.0f, 0.0f);
	XMFLOAT3					_up = XMFLOAT3(0.0f, 1.0f, 0.0f);
	XMFLOAT3					_look = XMFLOAT3(0.0f, 0.0f, 1.0f);
	XMFLOAT3					_scale = XMFLOAT3(0.0f, 0.0f, 1.0f);
	XMFLOAT3					_rotate = XMFLOAT3(0.0f, 0.0f, 1.0f);


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

	//게임 객체의 월드 변환 행렬에서 위치 벡터와 방향(x-축, y-축, z-축) 벡터를 반환한다. 
	XMFLOAT3 look();
	XMFLOAT3 size();
	XMFLOAT3 up();
	XMFLOAT3 right();
	//게임 객체의 위치를 설정한다. 
	void set_position(float x, float y, float z);
	void set_position(XMFLOAT3 position);
	XMFLOAT3 position();

	//게임 객체의 크기를 설정한다.
	void SetScale(float x, float y, float z);

	//게임 객체의 중력을 나타낸다.
	void SetGravity(XMFLOAT3& xmf3Gravity) { _gravityVector = xmf3Gravity; }

	//게임 객체를 회전(x-축, y-축, z-축)한다.
	void rotate(float pitch = 10.0f, float yaw = 10.0f, float roll = 10.0f);

	void move(XMFLOAT3& direction, float speed);
	void Move(float x, float y, float z);

	void LookTo(XMFLOAT3& xmf3LookTo, XMFLOAT3& xmf3Up);
	void LookTo(XMFLOAT3& xmf3LookTo);

	void GenerateRayForPicking(XMVECTOR& xmvPickPosition, XMMATRIX& xmmtxView, XMVECTOR& xmvPickRayOrigin, XMVECTOR& xmvPickRayDirection);

	int PickObjectByRayIntersection(XMVECTOR& xmvPickPosition, XMMATRIX& xmmtxView, float* pfHitDistance);

	bool PickModelOBB(XMVECTOR& xmPickPosition, XMMATRIX& xmmtxView, float* pfHitDistance);// 모델좌표계의 OBB와 충돌했는지 알려주는 함수 삼각형 검사는 안함
	void UpdateBoundingBox(); // DW설명 : OOBB바운딩 박스를 업데이트 한다. 즉 회전같은 것들을 업데이트함

	bool IsVisible(Camera* pCamera = NULL);

	void Rotate(XMFLOAT3* pxmf3Axis, float fAngle);

	void Update();

public:
	int m_PosX{};
	int m_PosY{};
};