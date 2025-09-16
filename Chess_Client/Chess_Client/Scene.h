#pragma once
#include "stdafx.h"
#include "Timer.h"
#include "Shader.h"
#include "Camera.h"

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

// (추가) 씬의 라이팅, 머터리얼 관련 변수 및 함수 선언
class Scene
{
public:
	Scene();
	virtual ~Scene();
	//씬에서 마우스와 키보드 메시지를 처리한다. 
	bool OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	bool OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	virtual void BuildObjects(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList) = 0;
	void LoadSceneFromFile(const std::string& filename, ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList); // 언리얼 엔진으로 뽑은 파일 불러오기
	virtual void ReleaseObjects() = 0;
	virtual void ProcessInput(float fElapsedTime) = 0;
	virtual void AnimateObjects(float fTimeElapsed, ID3D12GraphicsCommandList* pd3dCommandList) = 0;
	virtual void Render(ID3D12GraphicsCommandList* pd3dCommandList) = 0;
	virtual void Collision(float fElapsedTime) = 0;
	void MakeSrv(ID3D12Device* pd3dDevice);
	void ReleaseUploadBuffers();
	//그래픽 루트 시그너쳐를 생성한다. 
	ID3D12RootSignature* CreateGraphicsRootSignature(ID3D12Device* pd3dDevice);
	ID3D12RootSignature* CreateSkinnedGraphicsRootSignature(ID3D12Device* pd3dDevice); // GLB를 위한 루트시그너처 추가
	ID3D12RootSignature* GetGraphicsRootSignature();

	// 디스크립터 핸들을 할당하고 다음 위치로 이동시키는 함수
	void AllocateNextSrvDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE& outCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE& outGpuHandle);

	// 충돌함수
	GameObject* PickObjectPointedByCursor(int xClient, int yClient);

	// 오브젝트 생성 요청 함수
	// 오브젝트 생성 요청 실행함수

public: // GLB 뼈 없는 모델 더미전용
	void MakeDummyBonebuffer(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	D3D12_GPU_VIRTUAL_ADDRESS GetDummyBoneBufferAddress() const;

protected:
	// [추가] 모든 셰이더가 공유해서 사용할 더미 뼈 행렬 버퍼
	ComPtr<ID3D12Resource> m_pd3dcbDummyBoneTransforms;

protected:
	// 디스크립터 힙 관리를 위한 멤버 변수 추가
	ComPtr<ID3D12DescriptorHeap> _SrvDescriptorHeap;
	UINT _SrvDescriptorIncrementSize = 0;
	UINT _AllocatedSrvCount = 0;

protected:
	//씬은 게임 객체들의 집합이다. 게임 객체는 셰이더를 포함한다. 
	//ID3D12RootSignature* m_pd3dGraphicsRootSignature = NULL;
	//ID3D12RootSignature* m_pd3dSkinnedRootSignature = NULL; // GLB를 위한 것
	std::vector<ComPtr<ID3D12RootSignature>> _AllRootSignature;
	size_t _SignatureNum{};

protected:
	//배치(Batch) 처리를 하기 위하여 씬을 셰이더들의 리스트로 표현한다. 
	std::vector<std::shared_ptr<Shader>> _AllShaders;

	// Scene의 카메라
	Camera* m_pCamera = nullptr;

protected:
	// 키 입력을 위해 존재하는 것들
	BYTE NowKey[256]{};
	BYTE OldKey[256]{};

	POINT ptOldCursorPos{};

protected:
	// 씬의 조명과 재질 데이터
	LIGHTS* m_pLights = NULL;
	MATERIALS* m_pMaterials = NULL;

	// 조명과 재질 정보를 GPU로 보낼 상수 버퍼
	ComPtr<ID3D12Resource> m_pd3dcbLights;    
	ComPtr<ID3D12Resource> m_pd3dcbMaterials;  

	// 상수 버퍼와 매핑할 CPU 포인터
	LIGHTS* m_pcbMappedLights = NULL;
	MATERIALS* m_pcbMappedMaterials = NULL;

public: 
	// 씬의 모든 조명과 재질을 생성하는 함수
	void BuildLightsAndMaterials();

	// 조명/재질을 위한 상수 버퍼를 생성하고 갱신하는 함수들
	virtual void CreateShaderVariables(ID3D12Device* pd3dDevice, ID3D12GraphicsCommandList* pd3dCommandList);
	virtual void UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList);
	virtual void ReleaseShaderVariables();

};

