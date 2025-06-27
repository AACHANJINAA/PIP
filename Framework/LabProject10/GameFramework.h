#pragma once
#include "Timer.h"
#include "Scene.h"
#include "Camera.h"

// GameFramework.h
#include <memory>
#include <array>

class CGameFramework
{
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

	CGameTimer m_GameTimer;
	_TCHAR m_pszFrameRate[50];

	std::unique_ptr<CScene> m_pScene;
	std::unique_ptr<CCamera> m_pCamera;

public:
	CGameFramework();
	~CGameFramework();

	bool OnCreate(HINSTANCE hInstance, HWND hMainWnd);
	void OnDestroy();

	void CreateSwapChain();
	void CreateRtvAndDsvDescriptorHeaps();
	void CreateDirect3DDevice();
	void CreateCommandQueueAndList();

	void CreateRenderTargetViews();
	void CreateDepthStencilView();

	void BuildObjects();
	void ReleaseObjects();

	void ProcessInput();
	void AnimateObjects();
	void FrameAdvance();

	void WaitForGpuComplete();

	void OnProcessingMouseMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	void OnProcessingKeyboardMessage(HWND hWnd, UINT nMessageID, WPARAM wParam, LPARAM lParam);
	LRESULT CALLBACK OnProcessingWindowMessage(HWND hwnd, UINT nMessageID, WPARAM wParam, LPARAM IParam);

	void ChangeSwapChainState();

	void MoveToNextFrame();
};

