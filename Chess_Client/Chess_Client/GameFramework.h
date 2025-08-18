#pragma once
#include "Timer.h"
#include "Scene.h"

class GameFramework : public Singleton<GameFramework>
{
	friend Singleton<GameFramework>; // 싱글톤 접근 허용
private:
	
	HINSTANCE m_hInstance = NULL;
	HWND m_hWnd = NULL;

	int m_nWndClientWidth;
	int m_nWndClientHeight;

	ComPtr<IDXGIFactory4> m_pdxgiFactory;
	ComPtr<IDXGISwapChain3> m_pdxgiSwapChain;
	ComPtr<ID3D12Device> m_pd3dDevice;

	bool m_bMsaa4xEnable = false;

	UINT m_nMsaa4xQualityLevels = 0;

	static const UINT m_nSwapChainBuffers = 2;
	UINT m_nSwapChainBufferIndex;

	std::array<ComPtr<ID3D12Resource>, m_nSwapChainBuffers> m_d3dRenderTargetBuffers;

	ComPtr<ID3D12DescriptorHeap> m_pd3dRtvDescriptorHeap;
	UINT m_nRtvDescriptorIncrementSize;

	ComPtr<ID3D12Resource> m_pd3dDepthStencilBuffer;
	ComPtr<ID3D12DescriptorHeap> m_pd3dDsvDescriptorHeap;
	UINT m_nDsvDescriptorIncrementSize;

	ComPtr<ID3D12CommandQueue> m_pd3dCommandQueue;
	ComPtr<ID3D12CommandAllocator> m_pd3dCommandAllocator;
	ComPtr<ID3D12GraphicsCommandList> m_pd3dCommandList;

	ComPtr<ID3D12PipelineState> m_pd3dPipelineState;

	ComPtr<ID3D12Fence> m_pd3dFence;
	std::array<UINT64, m_nSwapChainBuffers> m_nFenceValues;
	HANDLE m_hFenceEvent;

	GameTimer m_GameTimer;
	_TCHAR m_pszFrameRate[50];

	std::unique_ptr<Scene> m_pScene;

	
public:
	GameFramework();
	~GameFramework();


	bool OnCreate(HINSTANCE hInstance, HWND hMainWnd);
	// 프레임워크를 초기화하는 함수(주 윈도우가 생성되면 호출됨)

	void OnDestroy();

	void CreateSwapChain();
	void CreateRtvAndDsvDescriptorHeaps();
	void CreateDirect3DDevice();
	void CreateCommandQueueAndList();
	// 스왑 체인, 디바이스, 서술자 힙, 명령 큐/할당자/리스트를 생성하는 함수

	void ChangeSwapChainState(); // 따라하기 5

	void CreateRenderTargetViews();
	void CreateDepthStencilView();
	// 렌더 타겟 뷰와 깊이-스텐실 뷰를 생성하는 함수

	void BuildObjects();
	void ReleaseObjects();
	// 렌더링할 메쉬와 게임 객체를 생성하고 소멸하는 함수

	//프레임워크의 핵심(사용자 입력, 애니메이션, 렌더링)을 구성하는 함수
	void ProcessNetwork();
	void ProcessInput();
	void AnimateObjects();
	void FrameAdvance();

	void WaitForGpuComplete();
	//CPU와 GPU를 동기화하는 함수

	void OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	void OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	LRESULT CALLBACK OnProcessingWindowMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	//윈도우의 메시지(키보드, 마우스 입력)를 처리하는 함수이다.

	ComPtr<ID3D12GraphicsCommandList>& GetCommandList() { return m_pd3dCommandList; }
	ComPtr<ID3D12Device>& GetDevice() { return m_pd3dDevice; }
public:

	//마지막으로 마우스 버튼을 클릭할 때의 마우스 커서의 위치이다. 
	POINT m_ptOldCursorPos;

	UINT m_nMessageID; // 어떤 키를 입력받은것인지에 대한 확인용이다.

	bool m_bIsWindowActive = true; // 창 활성화 상태를 저장할 플래그

	enum class ClientState // 클라이언트의 상태를 나타내는 열거형
	{
		Lobby,
		InGame
	};
	ClientState m_eClientState = ClientState::Lobby; // 기본 상태는 로비
public:
	void MoveToNextFrame();

};

