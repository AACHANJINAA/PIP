#pragma once
#include "CameraComponent.h"
#include "RootSignature.h"
#include "Shader.h"

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
    void render(ID3D12GraphicsCommandList* commandList);

    ID3D12RootSignature* get_root_signature(const std::string& name) const;
    ID3D12PipelineState* get_pso(const std::string& name) const;

	std::shared_ptr<Shader> get_shader(const std::string& name) const;

private:
    void create_root_signatures(ID3D12Device* device);
    void create_pipeline_state_objects(ID3D12Device* device);

    void build_render_list(CameraComponent* camera);
    void draw_render_list(ID3D12GraphicsCommandList* commandList, CameraComponent* camera);

    // [변경] 개별 ComPtr 대신, 이름으로 루트 시그니처를 관리하는 map을 사용합니다.
    std::unordered_map<std::string, ComPtr<ID3D12RootSignature>> _rootSignatures;
    std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> _pipelineStates;

    // Key: PSO 이름 (string), Value: 해당 PSO를 사용하는 GameObject 목록
    std::unordered_map<std::string, std::vector<std::shared_ptr<GameObject>>> _renderMap;
    // [추가] PSO 생성을 위해 등록된 모든 셰이더의 프로토타입을 저장합니다.
    std::unordered_map<std::string, std::shared_ptr<Shader>> _shaderPrototypes;

	// [추가] 사용할 루트 시그니처 생성기들을 저장합니다.
    std::vector<std::unique_ptr<IRootSignatureGenerator>> _rootSignatureGenerators; 
    // [추가] GPU 업로드에 사용하기 위해 Device 포인터를 저장합니다.
    ID3D12Device* _device = nullptr;
};
