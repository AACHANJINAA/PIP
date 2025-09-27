#pragma once

class GameObject;
class Camera;

class Renderer : public Singleton<Renderer>
{
    friend Singleton<Renderer>;
private:
    Renderer() = default;
    ~Renderer() = default;

public:
    void initialize(ID3D12Device* device);
    void render(ID3D12GraphicsCommandList* commandList, Camera* camera);

    ID3D12RootSignature* get_root_signature(const std::string& name) const;
    ID3D12PipelineState* get_pso(const std::string& name) const;

private:
    void create_root_signatures(ID3D12Device* device);
    void create_pipeline_state_objects(ID3D12Device* device);

    void build_render_list(Camera* camera);
    void draw_render_list(ID3D12GraphicsCommandList* commandList, Camera* camera);

    ComPtr<ID3D12RootSignature> _defaultRootSignature;
    ComPtr<ID3D12RootSignature> _skinnedRootSignature;
    std::map<std::string, ComPtr<ID3D12PipelineState>> _pipelineStates;

    // Key: PSO 이름 (string), Value: 해당 PSO를 사용하는 GameObject 목록
    std::map<std::string, std::vector<std::shared_ptr<GameObject>>> _renderMap;

    // [추가] GPU 업로드에 사용하기 위해 Device 포인터를 저장합니다.
    ID3D12Device* _device = nullptr;
};
