#pragma once
#include "stdafx.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include "ImGuizmo.h"

class ImGuiManager : public Singleton<ImGuiManager>
{
    friend Singleton<ImGuiManager>;
private:
    ImGuiManager() = default;
    ~ImGuiManager() override = default;

public:
    // GameFramework::OnCreate에서 호출될 초기화 함수
    bool initialize(HWND hWnd, ID3D12Device* device, ID3D12CommandQueue* commandQueue, int numFramesInFlight, DXGI_FORMAT rtvFormat);

    // GameFramework::OnDestroy에서 호출될 해제 함수
    void release();

    // 매 프레임 UI 그리기 준비 (FrameAdvance 시작 부분)
    void new_frame();

    // 렌더링 파이프라인에 ImGui 그리기 명령 추가 (FrameAdvance 끝 부분)
    void render(ID3D12GraphicsCommandList* commandList);

private:
    // ImGui 전용 폰트 및 텍스처를 담아둘 SRV 디스크립터 힙
    ComPtr<ID3D12DescriptorHeap> _srvDescHeap;
};