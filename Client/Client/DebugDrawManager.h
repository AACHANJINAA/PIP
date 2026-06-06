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

struct RemoteDebugShape {
    std::vector<common::Vec3> triangles;
    common::Vec3 pos;
    common::Quat rot;
};

class DebugDrawManager : public Singleton<DebugDrawManager>
{
    friend class Singleton<DebugDrawManager>;

private:
    DebugDrawManager() = default;
    ~DebugDrawManager() override = default;

public:
    // Initialize: Create meshes and constant buffers
    void Initialize(ID3D12Device* device);

    // Add debug draw request from server packet
    void AddDebugRequest(const common::packet::SC_PACKET_DEBUG_DRAW& packet);

    // [Add] Add debug draw request from client local call
    void AddDebugShape(common::packet::DebugShapeType type, DirectX::XMFLOAT3 pos, DirectX::XMFLOAT4 rot, DirectX::XMFLOAT3 extents, float lifeTime);
    void AddRemoteDebugShape(RemoteDebugShape&& shape);
    void ClearRemoteShapes() { _remoteShapes.clear(); }
    // Update: Decrease lifetime of requests
    void Update(float deltaTime);

    // Render: Draw all active debug shapes (Called by Renderer)
    void Render(ID3D12GraphicsCommandList* cmdList, UINT frameIndex);

    void LoadLocalDebugShape(const std::string& jsonPath, const std::string& targetActor, const std::string&
        targetMesh);
private:
    // Mesh creation helpers
    void CreateDynamicLineBuffer(ID3D12Device* device);
    void CreateUnitBox(ID3D12Device* device);
    void CreateUnitSphere(ID3D12Device* device);
    void CreateUnitCapsule(ID3D12Device* device);
    void RenderRemoteShape(ID3D12GraphicsCommandList* cmdList, UINT frameIndex, UINT cbSize, int& shapeIdx);

    static DirectX::XMVECTOR ToCVec(const nlohmann::json& j) {
        return DirectX::XMVectorSet(j.value("X", 0.0f), j.value("Y", 0.0f), j.value("Z", 0.0f), 0.0f);
    }

    static DirectX::XMVECTOR ToCQuat(const nlohmann::json& j) {
        return DirectX::XMVectorSet(j.value("X", 0.0f), j.value("Y", 0.0f), j.value("Z", 0.0f), j.value("W",
            1.0f));
    }
private:
    // Queue of active debug draw requests
    std::vector<DebugRequest> _requests;

    // --- Box Mesh Resources ---
    ComPtr<ID3D12Resource> _boxVB;
    D3D12_VERTEX_BUFFER_VIEW _boxVBView{};
    UINT _boxVertexCount = 0;

    // --- Sphere Mesh Resources ---
    ComPtr<ID3D12Resource> _sphereVB;
    D3D12_VERTEX_BUFFER_VIEW _sphereVBView{};
    UINT _sphereVertexCount = 0;

    // --- Capsule Mesh Resources ---
    ComPtr<ID3D12Resource> _capsuleVB;
    D3D12_VERTEX_BUFFER_VIEW _capsuleVBView{};
    UINT _capsuleVertexCount = 0;

    // --- Constant Buffer Resources (b0) ---
    // Double buffered for swap chain
    std::array<ComPtr<ID3D12Resource>, 2> _cbWorld;
    std::array<DirectX::XMFLOAT4X4*, 2>   _mappedWorld{};

	// [Add] Remote debug shapes received from server (for rendering)
    // 서버 Shape 시각화를 위한 동적 버퍼
    ComPtr<ID3D12Resource> _remoteLineVB;
    D3D12_VERTEX_BUFFER_VIEW _remoteLineVBView{};
    std::vector<DebugVertex> _remoteLineVertices;

    std::vector<RemoteDebugShape> _remoteShapes; // 서버에서 받은 원격 디버그 쉐이프
};
