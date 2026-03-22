#pragma once
#include "TimerManager.h"
#include "Scene.h"
// static constexpr UINT SWAP_CHAIN_BUFFERS = 2;
class ReplicationSystem;
class GameFramework : public Singleton<GameFramework>
{
	friend Singleton<GameFramework>; // 싱글톤 접근 허용
private:

	bool _isFullscreenToggle = false; // 전체화면 전환

	bool _isRendering = false;
	
	HINSTANCE _hInstance = nullptr;
	HWND _hWnd = nullptr;

	int _wndClientWidth;
	int _wndClientHeight;

	ComPtr<IDXGIFactory4> _factory;
	ComPtr<IDXGISwapChain3> _swapChain;
	ComPtr<ID3D12Device> _device;

	bool _isEnableMsaa = false;

	UINT _msaa4XQualityLevels = 0;

	
	UINT _swapChainBufferIndex;

	std::array<ComPtr<ID3D12Resource>, SWAP_CHAIN_BUFFERS> _renderTargetBuffers;

	ComPtr<ID3D12DescriptorHeap> _rtvDescriptorHeap;
	UINT _rtvDescriptorIncrementSize;

	ComPtr<ID3D12Resource> _depthStencilBuffer;
	ComPtr<ID3D12DescriptorHeap> _dsvDescriptorHeap;
	UINT _dsvDescriptorIncrementSize;

	ComPtr<ID3D12CommandQueue> _commandQueue;
	std::array<ComPtr<ID3D12CommandAllocator>, SWAP_CHAIN_BUFFERS> _commandAllocators;
	ComPtr<ID3D12GraphicsCommandList> _commandList;
	// [추가] 리소스 업로드 전용 할당기
	// (만약 더블 버퍼링 중이라면 배열로 선언: _uploadAllocators[SWAP_CHAIN_BUFFERS])
	std::array<ComPtr<ID3D12CommandAllocator>, SWAP_CHAIN_BUFFERS> _uploadAllocators;


	ComPtr<ID3D12PipelineState> _pipelineState; // 기존 PSO

	ComPtr<ID3D12PipelineState> _glbPipelineState; // GLB 스키닝/텍스쳐용 PSO

	ComPtr<ID3D12Fence> _fence;
	std::array<UINT64, SWAP_CHAIN_BUFFERS> _fenceValues;
	HANDLE _fenceEvent;
	HANDLE _frameLatencyWaitableObject = nullptr; // Frame Latency Waitable Object

	TimerManager _gameTimer;
	_TCHAR _frameRate[50];

	std::unique_ptr<Scene> _scene;

	float _physicsTimeAccumulator = 0.0f; // 물리 업데이트 시간 누적 변수

	UINT64 _currentFenceValue = 0; // 펜스 값을 전체적으로 관리할 카운터 변수

	// [추가] 리플리케이션 시스템 소유 (unique_ptr로 생명주기 관리)
	std::unique_ptr<ReplicationSystem> _replicationSystem;


	void update_game_logic(float deltaTime);
	void update_physics(float elapsedTime);

	GameFramework();
	~GameFramework();
public:


	bool OnCreate(HINSTANCE hInstance, HWND hMainWnd);
	// 프레임워크를 초기화하는 함수(주 윈도우가 생성되면 호출됨)

	void OnDestroy();

	void CreateSwapChain();
	void CreateRtvAndDsvDescriptorHeaps();
	void CreateDirect3DDevice();
	void CreateCommandQueueAndList();
	// 스왑 체인, 디바이스, 서술자 힙, 명령 큐/할당자/리스트를 생성하는 함수

	void ChangeSwapChainState(); // 따라하기 5, 전체화면 <-> 창 모드 전환 시 호출

	void CreateRenderTargetViews();
	void CreateDepthStencilView();
	// 렌더 타겟 뷰와 깊이-스텐실 뷰를 생성하는 함수

	void BuildObjects();
	void ReleaseObjects();
	// 렌더링할 메쉬와 게임 객체를 생성하고 소멸하는 함수

	//프레임워크의 핵심(사용자 입력, 애니메이션, 렌더링)을 구성하는 함수
	void ProcessNetwork();
	void ProcessInput();
	/*void AnimateObjects();*/
	void FrameAdvance();

	void WaitForGpuComplete();
	//CPU와 GPU를 동기화하는 함수

	ComPtr<ID3D12GraphicsCommandList>& command_list() { return _commandList; }
	ComPtr<ID3D12Device>& device() { return _device; }
	ComPtr<ID3D12CommandAllocator>& command_allocator() {
		return _commandAllocators[_swapChainBufferIndex];
	}
	ComPtr<ID3D12CommandQueue>& command_queue() { return _commandQueue; }
	HWND hWnd() const { return _hWnd; }

	// [추가] 리플리케이션 시스템 접근자
	ReplicationSystem* get_replication_system() const { return _replicationSystem.get(); }

	UINT64 next_fence_value() const;
public:
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

