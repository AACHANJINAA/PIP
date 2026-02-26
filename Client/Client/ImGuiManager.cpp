#include "stdafx.h"
#include "ImGuiManager.h"

bool ImGuiManager::initialize(HWND hWnd, ID3D12Device* device, int numFramesInFlight, DXGI_FORMAT rtvFormat)
{
    // 1. ImGui 전용 SRV 디스크립터 힙 생성 (매우 중요)
    // ImGui는 폰트 텍스처를 GPU에 올리기 위해 최소 1개의 SRV가 필요합니다.
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = 1; // 폰트 하나만 쓸 거면 1개면 충분합니다.
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; // 셰이더에서 볼 수 있어야 함

    if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&_srvDescHeap))))
    {
        CERROR("ImGui SRV Descriptor Heap 생성 실패!");
        return false;
    }

    // 2. ImGui 컨텍스트 생성 및 세팅
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // 키보드 컨트롤 허용

    // 테마 설정 (다크 모드)
    ImGui::StyleColorsDark();

    // 3. Win32 및 DX12 백엔드 초기화
    ImGui_ImplWin32_Init(hWnd);

    ImGui_ImplDX12_Init(device,
        numFramesInFlight, // SWAP_CHAIN_BUFFERS 개수
        rtvFormat,         // GameFramework의 스왑체인 포맷 (일반적으로 DXGI_FORMAT_R8G8B8A8_UNORM)
        _srvDescHeap.Get(),
        _srvDescHeap->GetCPUDescriptorHandleForHeapStart(),
        _srvDescHeap->GetGPUDescriptorHandleForHeapStart()
    );

    CLOG("ImGuiManager Initialized.");
    return true;
}

void ImGuiManager::release()
{
    // 할당했던 자원들을 역순으로 해제
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    _srvDescHeap.Reset();
    CLOG("ImGuiManager Released.");
}

void ImGuiManager::new_frame()
{
    // ImGui에게 "새 프레임이 시작되었어!" 라고 알려줌
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void ImGuiManager::render(ID3D12GraphicsCommandList* commandList)
{
    // Tool_Scene 등에서 ImGui::Begin() ~ ImGui::End()로 작성한 데이터를 최종적으로 렌더링 준비
    ImGui::Render();

    // 렌더링하기 전에 반드시 ImGui 전용 디스크립터 힙을 커맨드 리스트에 바인딩해야 합니다.
    ID3D12DescriptorHeap* ppHeaps[] = { _srvDescHeap.Get() };
    commandList->SetDescriptorHeaps(1, ppHeaps);

    // 실제 GPU에 그리기 명령 전달
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
}