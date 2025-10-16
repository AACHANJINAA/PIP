#pragma once
#include "stdafx.h"
#include "Timer.h"
#include "Shader.h"
#include "Camera.h"
#include "RenderComponent.h"

// LIGHTS 구조체: 씬의 모든 조명 정보
struct LIGHT
{
	XMFLOAT4 m_xmf4Ambient;
	XMFLOAT4 m_xmf4Diffuse;
	XMFLOAT4 m_xmf4Specular;
	XMFLOAT3 m_xmf3Position;
	float m_fFalloff;
	XMFLOAT3 m_xmf3Direction;
	float m_fTheta; //cos(m_fTheta)
	XMFLOAT3 m_xmf3Attenuation;
	float m_fPhi; //cos(m_fPhi)
	bool m_bEnable;
	int m_nType;
	float m_fRange;
	float padding;
};

struct LIGHTS
{
	LIGHT m_pLights[MAX_LIGHTS];
	XMFLOAT4 m_xmf4GlobalAmbient;
};

// MATERIALS 구조체: 씬의 모든 재질 정보.
struct MATERIALS
{
	Material m_pReflections[MAX_MATERIALS];
};

struct SceneObjectData {
	std::string name;
	std::string meshFile;
	struct {
		XMFLOAT3 location;
		XMFLOAT3 rotation;
		XMFLOAT3 scale;
	} transform;;
};

// 전방 선언
struct ID3D12Device;
struct ID3D12GraphicsCommandList;

class Scene
{
public:
    Scene() = default;
    virtual ~Scene() = default;

    // =================================================================
    // 1. 씬의 새로운 핵심 역할
    // =================================================================

    // [역할 유지] 파생 클래스는 이 함수를 구현하여 씬에 필요한 모든 GameObject를 생성하고 설정해야 합니다.
	virtual void build_objects(ID3D12Device* device, ID3D12GraphicsCommandList* commandList) = 0;

    // [역할 유지] 빌드 과정에서 사용된 업로드 버퍼를 해제합니다.
    virtual void release_upload_buffers() = 0;


    // =================================================================
    // 2. 유틸리티 함수 (선택적으로 유지)
    // =================================================================

    // [역할 유지] 씬 데이터를 파일에서 로드하는 기능은 유용하므로 남겨둡니다.
    // 단, 내부 구현은 새로운 아키텍처에 맞게 변경되어야 합니다. (Chess_Scene에서 재정의)
    virtual void load_scene_from_file(const std::string& filename, ID3D12Device* device, ID3D12GraphicsCommandList* commandList);


protected:
    // [제거] _AllRootSignature, _AllShaders, m_pCamera, m_pLights 등
    // Scene이 직접 관리하던 모든 멤버 변수들을 제거합니다.
    // 이들은 이제 Renderer, GameFramework, ObjectManager 등이 관리합니다.
};

// =================================================================
// [제거된 기능 목록]
// - OnProcessingMouseMessage, OnProcessingKeyboardMessage: 역할이 InputManager와 각 Script로 이전되어 제거
// - ProcessInput, AnimateObjects, Render, Collision: 역할이 GameFramework의 게임 루프와 각 Script로 이전되어 제거
// - CreateGraphicsRootSignature, MakeSrv, BuildLightsAndMaterials, CreateShaderVariables 등:
//   역할이 Renderer 또는 리소스 관리 시스템으로 이전되어 제거
// - PickObjectPointedByCursor: 역할이 InputManager 또는 별도의 시스템으로 이전되어야 하므로 제거
// =================================================================



//// (추가) 씬의 라이팅, 머터리얼 관련 변수 및 함수 선언
//class Scene
//{
//public:
//	Scene();
//	virtual ~Scene();
//	//씬에서 마우스와 키보드 메시지를 처리한다. 
//	bool OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
//	bool OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
//	virtual void BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList) = 0;
//	void LoadSceneFromFile(const std::string& filename, ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList); // 언리얼 엔진으로 뽑은 파일 불러오기
//	virtual void ReleaseObjects() = 0;
//	virtual void ProcessInput(float fElapsedTime) = 0;
//	virtual void AnimateObjects(float fTimeElapsed, ID3D12GraphicsCommandList* pd3dCommandList) = 0;
//	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList) = 0;
//	virtual void Collision(float fElapsedTime) = 0;
//	void MakeSrv(ID3D12Device* pd3dDevice);
//	void ReleaseUploadBuffers();
//	//그래픽 루트 시그너쳐를 생성한다. 
//	ID3D12RootSignature* CreateGraphicsRootSignature(ID3D12Device* pd3dDevice);
//	ID3D12RootSignature* CreateSkinnedGraphicsRootSignature(ID3D12Device* pd3dDevice); // GLB를 위한 루트시그너처 추가
//	ID3D12RootSignature* GetGraphicsRootSignature();
//
//	// 디스크립터 핸들을 할당하고 다음 위치로 이동시키는 함수
//	void AllocateNextSrvDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE& outCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE& outGpuHandle);
//
//	// 충돌함수
//	GameObject* PickObjectPointedByCursor(int xClient, int yClient);
//
//	// 오브젝트 생성 요청 함수
//	// 오브젝트 생성 요청 실행함수
//
//public: // GLB 뼈 없는 모델 더미전용
//	void MakeDummyBonebuffer(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
//	D3D12_GPU_VIRTUAL_ADDRESS GetDummyBoneBufferAddress() const;
//
//protected:
//	// [추가] 모든 셰이더가 공유해서 사용할 더미 뼈 행렬 버퍼
//	ComPtr<ID3D12Resource> m_pd3dcbDummyBoneTransforms;
//
//protected:
//	// 디스크립터 힙 관리를 위한 멤버 변수 추가
//	ComPtr<ID3D12DescriptorHeap> _SrvDescriptorHeap;
//	UINT _SrvDescriptorIncrementSize = 0;
//	UINT _AllocatedSrvCount = 0;
//
//protected:
//	//씬은 게임 객체들의 집합이다. 게임 객체는 셰이더를 포함한다. 
//	//ID3D12RootSignature* m_pd3dGraphicsRootSignature = NULL;
//	//ID3D12RootSignature* m_pd3dSkinnedRootSignature = NULL; // GLB를 위한 것
//	std::vector<ComPtr<ID3D12RootSignature>> _AllRootSignature;
//	size_t _SignatureNum{};
//
//protected:
//	//배치(Batch) 처리를 하기 위하여 씬을 셰이더들의 리스트로 표현한다. 
//	std::vector<std::shared_ptr<Shader>> _AllShaders;
//
//	// Scene의 카메라
//	Camera* m_pCamera = nullptr;
//
//protected:
//	// 키 입력을 위해 존재하는 것들
//	BYTE NowKey[256]{};
//	BYTE OldKey[256]{};
//
//	POINT ptOldCursorPos{};
//
//protected:
//	// 씬의 조명과 재질 데이터
//	LIGHTS* m_pLights = NULL;
//	MATERIALS* m_pMaterials = NULL;
//
//	// 조명과 재질 정보를 GPU로 보낼 상수 버퍼
//	ComPtr<ID3D12Resource> m_pd3dcbLights;    
//	ComPtr<ID3D12Resource> m_pd3dcbMaterials;  
//
//	// 상수 버퍼와 매핑할 CPU 포인터
//	LIGHTS* m_pcbMappedLights = NULL;
//	MATERIALS* m_pcbMappedMaterials = NULL;
//
//public: 
//	// 씬의 모든 조명과 재질을 생성하는 함수
//	void BuildLightsAndMaterials();
//
//	// 조명/재질을 위한 상수 버퍼를 생성하고 갱신하는 함수들
//	virtual void CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
//	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList);
//	virtual void ReleaseShaderVariables();
//
//};

