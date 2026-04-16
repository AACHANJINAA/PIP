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
	void AddDebugMeshShape(common::packet::DebugShapeType type, const std::vector<common::Vec3>& vertices, DirectX::XMFLOAT3 pos, DirectX::XMFLOAT4 rot, float lifeTime);

    // Update: Decrease lifetime of requests
    void Update(float deltaTime);

    // Render: Draw all active debug shapes (Called by Renderer)
    void Render(ID3D12GraphicsCommandList* cmdList, UINT frameIndex);

    void CreateDynamicLineBuffer(ID3D12Device* device);

private:
    // Mesh creation helpers
    void CreateUnitBox(ID3D12Device* device);
    void CreateUnitSphere(ID3D12Device* device);
    void CreateUnitCapsule(ID3D12Device* device);

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
    std::vector<DebugVertex> _remoteLineVertices; // 이번 프레임에 그릴 정점들

    std::vector<RemoteDebugShape> _remoteShapes; // 서버에서 받은 원격 디버그 쉐이프
};
