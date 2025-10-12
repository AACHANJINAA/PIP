#include "stdafx.h"
#include "GameFramework.h"
#include "ObjectManager.h"
#include "Chess_Scene.h"
#include "InputManager.h"
#include "NetworkManager.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "SceneManager.h"
#include "TransformComponent.h"


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
	_tcscpy_s(_frameRate, _T("LapProject ("));
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

	InputManager::Instance()->initialize(hMainWnd);
	SceneManager::Instance()->initialize();
	Renderer::Instance()->initialize(_device.Get());
	ResourceManager::Instance()->initialize(_device.Get());

	BuildObjects();
	//렌더링할 게임 객체를 생성한다.

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
	D3D12_COMMAND_QUEUE_DESC d3dCommandQueueDesc;
	::ZeroMemory(&d3dCommandQueueDesc, sizeof(D3D12_COMMAND_QUEUE_DESC));
	d3dCommandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	d3dCommandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	HRESULT hResult = _device->CreateCommandQueue(&d3dCommandQueueDesc, IID_PPV_ARGS(&_commandQueue));
	_ASSERTE(SUCCEEDED(hResult));

	hResult = _device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_commandAllocator));
	_ASSERTE(SUCCEEDED(hResult));

	hResult = _device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _commandAllocator.Get(), NULL, IID_PPV_ARGS(&_commandList));
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
	_commandList->Reset(_commandAllocator.Get(), NULL);

	_scene = std::make_unique<Chess_Scene>(); //초기 씬 설정 TODO: 나중에 씬 매니저로 변경
	_scene->build_objects(_device.Get(), _commandList.Get());


	// 로드가 끝난 메시는 _pending_meshes 목록에 들어갔으므로, GPU에 업로드합니다.
	ResourceManager::Instance()->upload_pending_meshes(_device.Get(), _commandList.Get());

	_commandList->Close();
	ID3D12CommandList* ppd3dCommandLists[] = { _commandList.Get() };
	_commandQueue->ExecuteCommandLists(1, ppd3dCommandLists);

	WaitForGpuComplete();

	// GPU 작업이 완료되었으므로, 임시 업로드 버퍼를 해제합니다.
	ResourceManager::Instance()->release_upload_buffers();

	// [수정] 모든 리소스 초기화 및 GPU 업로드가 끝났으므로, 이제 스크립트의 awake/start를 호출합니다.
	ObjectManager::Instance()->process_new_game_objects();

	_gameTimer.Reset();
}

void GameFramework::ReleaseObjects()
{
	_scene.reset();
}

void GameFramework::ProcessNetwork()
{
	NetworkManager::Instance()->receive_packets();
}

void GameFramework::ProcessInput()
{
	InputManager::Instance()->Update();
}

//void GameFramework::AnimateObjects()
//{
//	_scene.get()->AnimateObjects(_gameTimer.GetTimeElapsed(), _commandList.Get());
//} 씬에 있던거 게임프레임워크로 옮김

void GameFramework::WaitForGpuComplete()
{
	UINT64 nFenceValue = ++_fenceValues[_swapChainBufferIndex];
	HRESULT hResult = _commandQueue->Signal(_fence.Get(), nFenceValue);
	if (_fence->GetCompletedValue() < nFenceValue)
	{
		hResult = _fence->SetEventOnCompletion(nFenceValue, _fenceEvent);
		::WaitForSingleObject(_fenceEvent, INFINITE);
	}
}

void GameFramework::MoveToNextFrame()
{
	_swapChainBufferIndex = _swapChain->GetCurrentBackBufferIndex();

	UINT64 nFenceValue = _fenceValues[_swapChainBufferIndex];
	HRESULT hResult = _commandQueue->Signal(_fence.Get(), nFenceValue);

	if (_fence->GetCompletedValue() < nFenceValue)
	{
		hResult = _fence->SetEventOnCompletion(nFenceValue, _fenceEvent);
		::WaitForSingleObject(_fenceEvent, INFINITE);
	}
}

void GameFramework::FrameAdvance()
{
	// [핵심] 실제 렌더링이나 업데이트 시작 전에 씬 전환을 먼저 처리합니다. 한프레임 지연
	SceneManager::Instance()->process_scene_change_if_requested(_device.Get(),
		_commandAllocator.Get(), _commandList.Get());

	// 1. 타이머 틱 및 기본 처리
	_gameTimer.Tick(0.0f);
	float deltaTime = _gameTimer.GetTimeElapsed();
	ProcessNetwork();
	ProcessInput();
	
	// 2. 게임 로직 업데이트 (Update, LateUpdate)
	update_game_logic(deltaTime);

	// 3. 물리 업데이트 (FixedUpdate)
	update_physics(deltaTime);

	HRESULT hResult = _commandAllocator->Reset();
	hResult = _commandList->Reset(_commandAllocator.Get(), NULL);
	D3D12_RESOURCE_BARRIER d3dResourceBarrier;
	::ZeroMemory(&d3dResourceBarrier, sizeof(D3D12_RESOURCE_BARRIER));
	d3dResourceBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	d3dResourceBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	d3dResourceBarrier.Transition.pResource = _renderTargetBuffers[_swapChainBufferIndex].Get();
	d3dResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	d3dResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	_commandList->ResourceBarrier(1, &d3dResourceBarrier);
	D3D12_CPU_DESCRIPTOR_HANDLE d3dRtvCPUDescriptorHandle =	_rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	d3dRtvCPUDescriptorHandle.ptr += (_swapChainBufferIndex * _rtvDescriptorIncrementSize);
	//float pfClearColor[4] = { 0.0f, 0.125f, 0.3f, 1.0f };
	
	// 내가 한 색상
	float pfClearColor[4] = { 0.894f, 0.651f, 0.475f, 1.0f };
	_commandList->ClearRenderTargetView(d3dRtvCPUDescriptorHandle,	pfClearColor/*Colors::Azure*/, 0, NULL);
	D3D12_CPU_DESCRIPTOR_HANDLE d3dDsvCPUDescriptorHandle =	_dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	_commandList->ClearDepthStencilView(d3dDsvCPUDescriptorHandle,	D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
		1.0f, 0, 0, NULL); _commandList->OMSetRenderTargets(1, &d3dRtvCPUDescriptorHandle, TRUE, &d3dDsvCPUDescriptorHandle);


	// [수정] 렌더러 호출 시 더 이상 카메라를 넘기지 않습니다.
	Renderer::Instance()->render(_commandList.Get());
	

	//3인칭 카메라일 때 플레이어가 항상 보이도록 렌더링한다.
#ifdef _WITH_PLAYER_TOP

	//렌더 타겟은 그대로 두고 깊이 버퍼를 1.0으로 지우고 플레이어를 렌더링하면 플레이어는 무조건 그려질 것이다.
	_commandList->ClearDepthStencilView(d3dDsvCPUDescriptorHandle, 
	D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, NULL);

#endif
	//3인칭 카메라일 때 플레이어를 렌더링한다.
	/*if (m_pPlayer) 
	{
		m_pPlayer->Render(m_pd3dCommandList, m_pCamera);
	}*/
	d3dResourceBarrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	d3dResourceBarrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	d3dResourceBarrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	_commandList->ResourceBarrier(1, &d3dResourceBarrier);

	hResult = _commandList->Close();
	ID3D12CommandList* ppd3dCommandLists[] = { _commandList.Get()};
	_commandQueue->ExecuteCommandLists(1, ppd3dCommandLists);
	
	_swapChain->Present(0, 0);
	WaitForGpuComplete();
	MoveToNextFrame();
	// 5. 파괴 예정 객체 정리
	ObjectManager::Instance()->process_destructions();
	_gameTimer.GetFrameRate(_frameRate + 12, 37);
	::SetWindowText(_hWnd, _frameRate);
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
	ObjectManager::Instance()->process_new_game_objects();
	 
	const auto& allGameObjects = ObjectManager::Instance()->get_all_game_objects();

	// .FreeCameraScript가 입력을 받아 자신의 Transform을 업데이트
	for (const auto& gameObject : allGameObjects)
	{
		if (gameObject && !gameObject->is_destroyed())
		{
			gameObject->update(deltaTime);
		}
	}

	// 메인 카메라의 뷰 행렬 계산
	if (auto main_cam = CameraComponent::get_main())
	{
		main_cam->recalculate_view_matrix();
	}

	// LateUpdate는 뷰 행렬 계산 후에도 ㄱㅊ
	for (const auto& gameObject : allGameObjects)
	{
		if (gameObject && !gameObject->is_destroyed())
		{
			gameObject->late_update(deltaTime);
		}
	}
}

void GameFramework::update_physics(float elapsedTime)
{
	_physicsTimeAccumulator += elapsedTime;
	const float fixedTimeStep = 1.0f / 60.0f;

	while (_physicsTimeAccumulator >= fixedTimeStep)
	{
		const auto& allGameObjects = ObjectManager::Instance()->get_all_game_objects();
		for (const auto& gameObject : allGameObjects)
		{
			if (gameObject && !gameObject->is_destroyed())
			{
				gameObject->fixed_update(fixedTimeStep);
			}
		}
		_physicsTimeAccumulator -= fixedTimeStep;
	}
}






