#include "stdafx.h"
#include "GameFramework.h"

#include "Chess_Scene.h"
#include "DebugDrawManager.h"
#include "Renderer.h"

#include "DescriptorManager.h"
#include "InputManager.h"
#include "NetworkManager.h"
#include "ObjectManager.h"
#include "ResourceManager.h"
#include "SceneManager.h"
#include "LightManager.h"
#include "PhysicsManager.h"
#include "ReplicationSystem.h"


GameFramework::GameFramework()
	: _wndClientWidth(FRAME_BUFFER_WIDTH)
	, _wndClientHeight(FRAME_BUFFER_HEIGHT)
	, _isEnableMsaa(false)
	, _msaa4XQualityLevels(0)
	, _swapChainBufferIndex(0)
	, _rtvDescriptorIncrementSize(0)
	, _dsvDescriptorIncrementSize(0)
	, _fenceEvent(NULL)
{
	_tcscpy_s(_frameRate, _T("S.T.L ("));
	_fenceValues.fill(0);
}



GameFramework::~GameFramework()
{

}

//다음 함수는 응용 프로그램이 실행되어 주 윈도우가 생성되면 호출된다는 것에 유의하라.
bool GameFramework::OnCreate(HINSTANCE hInstance, HWND hMainWnd)
{
	_hInstance = hInstance;
	_hWnd = hMainWnd;

	//Direct3D 디바이스, 명령 큐와 명령 리스트, 스왑 체인 등을 생성하는 함수를 호출한다. 
	CreateDirect3DDevice();
	CreateCommandQueueAndList();
	CreateRtvAndDsvDescriptorHeaps();
	CreateSwapChain(); 
	CreateDepthStencilView();

	HRESULT hResult;
	hResult = _commandAllocators[0]->Reset();
	hResult = _commandList->Reset(_commandAllocators[0].Get(), NULL);

	InputManager::instance()->initialize(hMainWnd);
	if (!PhysicsManager::instance()->initialize()) {
		CLOG("[ERROR] PhysicsManager Init Failed!" << std::endl);
	}
	else {
		CLOG("[SUCCESS] PhysicsManager Initialized." << std::endl);
	}
	// [추가] 리플리케이션 시스템 초기화
	_replicationSystem = std::make_unique<ReplicationSystem>();
	DescriptorManager::instance()->initialize(_device.Get());
	ResourceManager::instance()->initialize(_device.Get(), _commandList.Get());
	Renderer::instance()->initialize(_device.Get());
	SceneManager::instance()->initialize(_device.Get(), _commandList.Get());
	LightManager::instance()->initialize(_device.Get());
#ifdef _DEBUG_PHYSICS_VISUALIZATION
	DebugDrawManager::instance()->Initialize(_device.Get());
#endif

	BuildObjects();
	//렌더링할 게임 객체를 생성한다.

	hResult = _commandList->Close();
	ID3D12CommandList * ppd3dCommandLists[] = { _commandList.Get() };
	_commandQueue->ExecuteCommandLists(1, ppd3dCommandLists);
	
	// GPU가 모든 초기화 작업을 마칠 때까지 기다립니다.
	WaitForGpuComplete();

	// GPU에 데이터 전송이 끝났으므로, 임시 업로드 버퍼들을 해제합니다.
	ResourceManager::instance()->release_upload_buffers(UINT_MAX);

	return(true);
}

void GameFramework::OnDestroy()
{
	WaitForGpuComplete();
	ReleaseObjects();
	if (_swapChain) _swapChain->SetFullscreenState(FALSE, NULL);
	::CloseHandle(_fenceEvent);

#if defined(_DEBUG)
	ComPtr<IDXGIDebug1> pdxgiDebug;
	DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pdxgiDebug));
	if (pdxgiDebug) pdxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_DETAIL);
#endif
	SceneManager::instance()->release();
	ResourceManager::instance()->release();
}

void GameFramework::CreateSwapChain()
{
	RECT rcClient;
	::GetClientRect(_hWnd, &rcClient);
	_wndClientWidth = rcClient.right - rcClient.left;
	_wndClientHeight = rcClient.bottom - rcClient.top;

	DXGI_SWAP_CHAIN_DESC dxgiSwapChainDesc;
	::ZeroMemory(&dxgiSwapChainDesc, sizeof(dxgiSwapChainDesc));
	dxgiSwapChainDesc.BufferCount = SWAP_CHAIN_BUFFERS;
	dxgiSwapChainDesc.BufferDesc.Width = _wndClientWidth;
	dxgiSwapChainDesc.BufferDesc.Height = _wndClientHeight;
	dxgiSwapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	dxgiSwapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
	dxgiSwapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	dxgiSwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	dxgiSwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	dxgiSwapChainDesc.OutputWindow = _hWnd;
	dxgiSwapChainDesc.SampleDesc.Count = (_isEnableMsaa) ? 4 : 1; dxgiSwapChainDesc.SampleDesc.Quality = (_isEnableMsaa) ? (_msaa4XQualityLevels - 1) : 0;
	dxgiSwapChainDesc.Windowed = TRUE;
	dxgiSwapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	ComPtr<IDXGISwapChain> pSwapChain;
	HRESULT hResult = _factory->CreateSwapChain(_commandQueue.Get(), &dxgiSwapChainDesc, &pSwapChain);
	_ASSERTE(SUCCEEDED(hResult));

	hResult = pSwapChain.As(&_swapChain);
	_ASSERTE(SUCCEEDED(hResult));

	_swapChainBufferIndex = _swapChain->GetCurrentBackBufferIndex();
	hResult = _factory->MakeWindowAssociation(_hWnd, DXGI_MWA_NO_ALT_ENTER);

#ifndef _WITH_SWAPCHAIN_FULLSCREEN_STATE 
	CreateRenderTargetViews();
#endif
}

void GameFramework::CreateDirect3DDevice() {
	HRESULT hResult;
	UINT nDXGIFactoryFlags = 0;
#if defined(_DEBUG)
	ID3D12Debug* pd3dDebugController = NULL;
	hResult = D3D12GetDebugInterface(__uuidof(ID3D12Debug), (void**)&pd3dDebugController);
	if (pd3dDebugController) {
		pd3dDebugController->EnableDebugLayer();
		pd3dDebugController->Release();
	}
	nDXGIFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

	hResult = ::CreateDXGIFactory2(nDXGIFactoryFlags, __uuidof(IDXGIFactory4), (void**)&_factory);
	_ASSERTE(SUCCEEDED(hResult));

	ComPtr<IDXGIAdapter1> pd3dAdapter;
	for (UINT i = 0; DXGI_ERROR_NOT_FOUND != _factory->EnumAdapters1(i, &pd3dAdapter); i++)
	{
		DXGI_ADAPTER_DESC1 dxgiAdapterDesc;
		pd3dAdapter->GetDesc1(&dxgiAdapterDesc);
		if (dxgiAdapterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) continue;
		if (SUCCEEDED(D3D12CreateDevice(pd3dAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&_device)))) break;
	}

	if (!pd3dAdapter) {
		_factory->EnumWarpAdapter(_uuidof(IDXGIAdapter1), (void**)&pd3dAdapter);
		D3D12CreateDevice(pd3dAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&_device));
	}

	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS d3dMsaaQualityLevels;
	d3dMsaaQualityLevels.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	d3dMsaaQualityLevels.SampleCount = 4;
	d3dMsaaQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	d3dMsaaQualityLevels.NumQualityLevels = 0;
	_device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &d3dMsaaQualityLevels, sizeof(D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS));
	_msaa4XQualityLevels = d3dMsaaQualityLevels.NumQualityLevels;

	_isEnableMsaa = (_msaa4XQualityLevels > 1) ? true : false;
	hResult = _device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence));
	_ASSERTE(SUCCEEDED(hResult));

	_fenceEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
}

void GameFramework::CreateCommandQueueAndList()
{
	HRESULT hResult;
	// 큐 생성 (기존 유지)
	D3D12_COMMAND_QUEUE_DESC d3dCommandQueueDesc;
	::ZeroMemory(&d3dCommandQueueDesc, sizeof(D3D12_COMMAND_QUEUE_DESC));
	d3dCommandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	d3dCommandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	hResult = _device->CreateCommandQueue(&d3dCommandQueueDesc, IID_PPV_ARGS(&_commandQueue));
	_ASSERTE(SUCCEEDED(hResult));

	// [수정] 할당기 배열 생성
	for (int i = 0; i < SWAP_CHAIN_BUFFERS; i++)
	{
		hResult = _device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_commandAllocators[i]));
		_ASSERTE(SUCCEEDED(hResult));
		hResult = _device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_uploadAllocators[i]));
		_ASSERTE(SUCCEEDED(hResult));
	}

	hResult = _device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _commandAllocators[0].Get(), nullptr,
		IID_PPV_ARGS(&_commandList));
	_ASSERTE(SUCCEEDED(hResult));

	hResult = _commandList->Close();
	_ASSERTE(SUCCEEDED(hResult));
}

void GameFramework::CreateRtvAndDsvDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC d3dDescriptorHeapDesc;
	::ZeroMemory(&d3dDescriptorHeapDesc, sizeof(D3D12_DESCRIPTOR_HEAP_DESC));
	d3dDescriptorHeapDesc.NumDescriptors = SWAP_CHAIN_BUFFERS;
	d3dDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	d3dDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	d3dDescriptorHeapDesc.NodeMask = 0;
	HRESULT hResult = _device->CreateDescriptorHeap(&d3dDescriptorHeapDesc, IID_PPV_ARGS(&_rtvDescriptorHeap));
	_ASSERTE(SUCCEEDED(hResult));

	_rtvDescriptorIncrementSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	d3dDescriptorHeapDesc.NumDescriptors = 1;
	d3dDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	d3dDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	hResult = _device->CreateDescriptorHeap(&d3dDescriptorHeapDesc, IID_PPV_ARGS(&_dsvDescriptorHeap));
	_ASSERTE(SUCCEEDED(hResult));

	_dsvDescriptorIncrementSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
}

//스왑체인의 각 후면 버퍼에 대한 렌더 타겟 뷰를 생성한다. 
void GameFramework::CreateRenderTargetViews()
{
	D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvCPUDescriptorHandle = _rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	for (UINT i = 0; i < SWAP_CHAIN_BUFFERS; i++)
	{
		_swapChain->GetBuffer(i, IID_PPV_ARGS(&_renderTargetBuffers[i]));
		_device->CreateRenderTargetView(_renderTargetBuffers[i].Get(), NULL, d3dRtvCPUDescriptorHandle);
		d3dRtvCPUDescriptorHandle.ptr += _rtvDescriptorIncrementSize;
	}
}

void GameFramework::CreateDepthStencilView()
{
	D3D12_RESOURCE_DESC d3dResourceDesc;
	d3dResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	d3dResourceDesc.Alignment = 0;
	d3dResourceDesc.Width = _wndClientWidth;
	d3dResourceDesc.Height = _wndClientHeight;
	d3dResourceDesc.DepthOrArraySize = 1;
	d3dResourceDesc.MipLevels = 1;
	d3dResourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	d3dResourceDesc.SampleDesc.Count = (_isEnableMsaa) ? 4 : 1;
	d3dResourceDesc.SampleDesc.Quality = (_isEnableMsaa) ? (_msaa4XQualityLevels - 1) : 0;
	d3dResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	d3dResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_HEAP_PROPERTIES d3dHeapProperties;
	::ZeroMemory(&d3dHeapProperties, sizeof(D3D12_HEAP_PROPERTIES));
	d3dHeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
	d3dHeapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	d3dHeapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	d3dHeapProperties.CreationNodeMask = 1;
	d3dHeapProperties.VisibleNodeMask = 1;

	D3D12_CLEAR_VALUE d3dClearValue;
	d3dClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; d3dClearValue.DepthStencil.Depth = 1.0f;
	d3dClearValue.DepthStencil.Stencil = 0;

	_device->CreateCommittedResource(&d3dHeapProperties, D3D12_HEAP_FLAG_NONE, &d3dResourceDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &d3dClearValue, IID_PPV_ARGS(&_depthStencilBuffer));

	D3D12_CPU_DESCRIPTOR_HANDLE d3dDsvCPUDescriptorHandle = _dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	_device->CreateDepthStencilView(_depthStencilBuffer.Get(), NULL, d3dDsvCPUDescriptorHandle);
}




void GameFramework::BuildObjects()
{
	// 비워두자?
}

void GameFramework::ReleaseObjects()
{
	_scene.reset();
}

void GameFramework::ProcessNetwork()
{
	NetworkManager::instance()->process_queued_packets();
}

void GameFramework::ProcessInput()
{
	InputManager::instance()->Update();
}

//void GameFramework::AnimateObjects()
//{
//	_scene.get()->AnimateObjects(_gameTimer.GetTimeElapsed(), _commandList.Get());
//} 씬에 있던거 게임프레임워크로 옮김

void GameFramework::WaitForGpuComplete()
{
	// 기존 방식이 전역 펜스 값을 사용하지 않아 전역 펜스값을 사용하도록 수정
	_currentFenceValue++;
	UINT64 fenceToWaitFor = _currentFenceValue;

	// 명령 큐에 시그널을 보내기
	HRESULT hResult = _commandQueue->Signal(_fence.Get(), fenceToWaitFor);

	// 해당 펜스 값에 도달할 때까지 CPU를 대기
	if (_fence->GetCompletedValue() < fenceToWaitFor)
	{
		hResult = _fence->SetEventOnCompletion(fenceToWaitFor, _fenceEvent);
		::WaitForSingleObject(_fenceEvent, INFINITE);
	}

	// 각 버퍼의 마지막 펜스 값도 최신화
	for (int i = 0; i < SWAP_CHAIN_BUFFERS; ++i)
	{
		_fenceValues[i] = fenceToWaitFor;
	}
}

void GameFramework::MoveToNextFrame()
{
	const UINT64 fenceValueToSignal = _currentFenceValue + 1;
	_currentFenceValue++;

	HRESULT hResult = _commandQueue->Signal(_fence.Get(), fenceValueToSignal);

	_fenceValues[_swapChainBufferIndex] = fenceValueToSignal;

	_swapChainBufferIndex = _swapChain->GetCurrentBackBufferIndex();

	const UINT64 fenceValueToWaitFor = _fenceValues[_swapChainBufferIndex];

	// DW주석 : 이거 풀면 gpu를 기다리는 방식이지만 깜빡이는 현상은 해결할 수 있음
	// WaitForGpuComplete();

	if (_fence->GetCompletedValue() < fenceValueToWaitFor)
	{
		hResult = _fence->SetEventOnCompletion(fenceValueToWaitFor, _fenceEvent);
		::WaitForSingleObject(_fenceEvent, INFINITE);
	}
}

void GameFramework::FrameAdvance()
{
	if (_isRendering) return;
	_isRendering = true;

	// [수정] 현재 프레임 인덱스에 맞는 할당기 선택
	auto& currentRenderAllocator = _commandAllocators[_swapChainBufferIndex];
	auto& currentUploadAllocator = _uploadAllocators[_swapChainBufferIndex];

	// 1. 씬 전환 처리 (필요시)
	SceneManager::instance()->process_scene_change_if_requested(_device.Get(), currentRenderAllocator.Get(), _commandList.Get());

	// 2. 타이머 & 로직 & 물리 업데이트
	_gameTimer.Tick(0.0f);
	float deltaTime = _gameTimer.GetTimeElapsed();

	ProcessNetwork(); // (스레드 분리했다면 큐 비우기)

	// 2. 리플리케이션 시스템 업데이트 (ReplicationSystem)
	// 채워진 스냅샷 데이터를 각 오브젝트(INetSync)에 일괄 적용합니다.
	if (_replicationSystem) {
		_replicationSystem->update(deltaTime);
	}
	ProcessInput();
	update_game_logic(deltaTime);
	update_physics(deltaTime);

	// ---------------------------------------------------------
	// 3. [비동기 리소스 업로드] (대기 없음!)
	// ---------------------------------------------------------
	// 업로드 전용 할당기 사용 -> 렌더링 할당기와 충돌 안 함
	UINT64 nextFenceValue = _currentFenceValue + 1;

	// 업로드 처리시 이 값을 알려줌
	currentUploadAllocator->Reset();
	_commandList->Reset(currentUploadAllocator.Get(), nullptr);

	// 큐에 쌓인 메쉬 중 일부만(Time Slicing) 업로드 명령 기록
	ResourceManager::instance()->process_pending_uploads(_device.Get(), _commandList.Get(), nextFenceValue);

	_commandList->Close();
	ID3D12CommandList* ppUploadLists[] = { _commandList.Get() };
	_commandQueue->ExecuteCommandLists(1, ppUploadLists);

	// [중요] 여기서 WaitForGpuComplete() 절대 호출 금지!
	// GPU가 알아서 업로드하고 나서 렌더링함 (같은 큐라서 순서 보장됨)

	// ---------------------------------------------------------
	// 4. [렌더링]
	// ---------------------------------------------------------
	// 렌더링 전용 할당기 사용
	currentRenderAllocator->Reset();
	_commandList->Reset(currentRenderAllocator.Get(), nullptr);

	ResourceManager::instance()->set_current_command_list(_commandList.Get());

	// (리소스 배리어 설정: Present -> RenderTarget)
	auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		_renderTargetBuffers[_swapChainBufferIndex].Get(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
	_commandList->ResourceBarrier(1, &barrier);

	// 뷰포트, RTV/DSV 설정 및 클리어
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = _rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	rtvHandle.ptr += (_swapChainBufferIndex * _rtvDescriptorIncrementSize);
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = _dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	float clearColor[4] = { 0.894f, 0.651f, 0.475f, 1.0f };
	_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
	_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0,
		nullptr);
	_commandList->OMSetRenderTargets(1, &rtvHandle, TRUE, &dsvHandle);

	// 실제 그리기 (업로드 안 된 메쉬는 Mesh::render 내부에서 skip됨)
	Renderer::instance()->render(_commandList.Get(), _swapChainBufferIndex);

	// 씬의 후처리 렌더링
	Scene* currentScene = SceneManager::instance()->current_scene();
	if (currentScene)
	{
		currentScene->render_post_process(_commandList.Get(), _swapChainBufferIndex);
	}

#ifdef _WITH_PLAYER_TOP
	_commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0,
		nullptr);
#endif

	// (리소스 배리어 복구: RenderTarget -> Present)
	barrier = CD3DX12_RESOURCE_BARRIER::Transition(
		_renderTargetBuffers[_swapChainBufferIndex].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
	_commandList->ResourceBarrier(1, &barrier);

	_commandList->Close();
	ID3D12CommandList* ppRenderLists[] = { _commandList.Get() };
	_commandQueue->ExecuteCommandLists(1, ppRenderLists);

	// ---------------------------------------------------------
	// 5. [프레임 종료]
	// ---------------------------------------------------------
	_swapChain->Present(1, 0); // VSync 끄기 (0)

	// [중요] 임시 업로드 버퍼 해제
	// (스마트 포인터라 큐에서 빠지면 알아서 해제되지만, 명시적 호출도 가능)

	// 다음 프레임 준비 (여기서만 펜스 대기)
	MoveToNextFrame();


	ResourceManager::instance()->release_upload_buffers(_fence->GetCompletedValue());
	// 후처리
	ObjectManager::instance()->process_destructions();
	_gameTimer.GetFrameRate(_frameRate + 7, 42);
	::SetWindowText(_hWnd, _frameRate);

	_isRendering = false;
}

void GameFramework::ChangeSwapChainState()
{
	WaitForGpuComplete();

	BOOL bFullScreenState = FALSE;
	_swapChain->GetFullscreenState(&bFullScreenState, NULL);
	_swapChain->SetFullscreenState(!bFullScreenState, NULL);

	DXGI_MODE_DESC dxgiTargetParameters;
	dxgiTargetParameters.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	dxgiTargetParameters.Width = _wndClientWidth;
	dxgiTargetParameters.Height = _wndClientHeight;
	dxgiTargetParameters.RefreshRate.Numerator = 60;
	dxgiTargetParameters.RefreshRate.Denominator = 1;
	dxgiTargetParameters.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	dxgiTargetParameters.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	_swapChain->ResizeTarget(&dxgiTargetParameters);

	for (int i = 0; i < SWAP_CHAIN_BUFFERS; i++) _renderTargetBuffers[i].Reset();

	DXGI_SWAP_CHAIN_DESC dxgiSwapChainDesc;
	_swapChain->GetDesc(&dxgiSwapChainDesc);
	_swapChain->ResizeBuffers(SWAP_CHAIN_BUFFERS, _wndClientWidth,
	_wndClientHeight, dxgiSwapChainDesc.BufferDesc.Format, dxgiSwapChainDesc.Flags);

	_swapChainBufferIndex = _swapChain->GetCurrentBackBufferIndex();
	CreateRenderTargetViews();
}
void GameFramework::update_game_logic(float deltaTime)
{
	// Awake와 Start가 먼저 호출되도록 순서 변경
	ObjectManager::instance()->process_new_game_objects();
	 
	const auto& allGameObjects = ObjectManager::instance()->get_all_game_objects();

	// .FreeCameraScript가 입력을 받아 자신의 Transform을 업데이트
	for (const auto& gameObject : allGameObjects)
	{
		if (gameObject && !gameObject->is_destroyed())
		{
			gameObject->update(deltaTime);
		}
	}

	// LateUpdate는 뷰 행렬 계산 후에도 ㄱㅊ
	for (const auto& gameObject : allGameObjects)
	{
		if (gameObject && !gameObject->is_destroyed())
		{
			gameObject->late_update(deltaTime);
		}
	}
	// 조명 매니저 업데이트
	LightManager::instance()->update();

	// 메인 카메라의 뷰 행렬 계산
	if (auto main_cam = CameraComponent::get_main())
	{
		main_cam->recalculate_view_matrix();
	}

#ifdef _DEBUG_PHYSICS_VISUALIZATION
	DebugDrawManager::instance()->Update(deltaTime);
#endif
}

void GameFramework::update_physics(float elapsedTime)
{
	_physicsTimeAccumulator += elapsedTime;
	const float fixedTimeStep = 0.02f;

	while (_physicsTimeAccumulator >= fixedTimeStep)
	{
		const auto& allGameObjects = ObjectManager::instance()->get_all_game_objects();

		// 1. Transform -> Physics Body 동기화
		for (const auto& gameObject : allGameObjects)
		{
			if (gameObject && !gameObject->is_destroyed())
			{
				gameObject->fixed_update(fixedTimeStep);
			}
		}

		// 2. Physics Simulation & Event Dispatch (여기에 추가!)
		PhysicsManager::instance()->update(fixedTimeStep);

		_physicsTimeAccumulator -= fixedTimeStep;
	}
}