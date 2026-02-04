#pragma once


struct DebugRequest {
    common::packet::DebugShapeType shapeType;
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT4 rotation;
    DirectX::XMFLOAT3 extents;
    float lifeTime;
};

struct DebugVertex {
    DirectX::XMFLOAT3 pos;
};

class DebugDrawManager : public Singleton<DebugDrawManager>
{
    friend class Singleton<DebugDrawManager>;

private:
    DebugDrawManager() = default;
    ~DebugDrawManager() override = default;

public:
    // 초기화: 메쉬 생성 및 상수 버퍼 준비
    void Initialize(ID3D12Device* device);

    // 서버 패킷을 받아 리스트에 추가
    void AddDebugRequest(const common::packet::SC_PACKET_DEBUG_DRAW& packet);

    // 수명 관리 (deltaTime만큼 감소)
    void Update(float deltaTime);

    // 실제 렌더링 (Renderer::render에서 호출됨)
    void Render(ID3D12GraphicsCommandList* cmdList, UINT frameIndex);

private:
    // 단위 메쉬 생성 함수들
    void CreateUnitBox(ID3D12Device* device);
    void CreateUnitSphere(ID3D12Device* device);

private:
    // 렌더링 요청 큐
    std::vector<DebugRequest> _requests;

    // --- 박스 메쉬 리소스 ---
    ComPtr<ID3D12Resource> _boxVB;
    D3D12_VERTEX_BUFFER_VIEW _boxVBView{};
    UINT _boxVertexCount = 0;

    // --- 구체 메쉬 리소스 ---
    ComPtr<ID3D12Resource> _sphereVB;
    D3D12_VERTEX_BUFFER_VIEW _sphereVBView{};
    UINT _sphereVertexCount = 0;

    // --- 상수 버퍼 (월드 행렬 전달용: b0) ---
    // 프레임별로 별도 관리가 필요함 (SWAP_CHAIN_BUFFERS = 2)
    std::array<ComPtr<ID3D12Resource>, 2> _cbWorld;
    std::array<DirectX::XMFLOAT4X4*, 2>   _mappedWorld{};
};

