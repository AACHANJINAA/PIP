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
    ~Renderer() override = default;

public:
    void initialize(ID3D12Device* device);
    void render(ID3D12GraphicsCommandList* commandList, UINT frame_index);

    ID3D12RootSignature* get_root_signature(const std::string& name) const;
    ID3D12PipelineState* get_pso(const std::string& name) const;

	std::shared_ptr<Shader> get_shader(const std::string& name) const;

    // [추가] 텍스처 디스크립터 테이블을 동적으로 할당하고 파이프라인에 바인딩합니다.
    void bind_texture_table(ID3D12GraphicsCommandList * command_list, UINT root_parameter_index, const std::vector<D3D12_CPU_DESCRIPTOR_HANDLE>&cpu_handles);

    ID3D12Device* get_device() const { return _device; }

    const std::unordered_map<std::string, std::vector<std::shared_ptr<GameObject>>>& get_render_map() const { return _renderMap; }


private:
    void create_root_signatures(ID3D12Device* device);
    void create_pipeline_state_objects(ID3D12Device* device);

    void build_render_list(CameraComponent* camera);
    void draw_render_list(ID3D12GraphicsCommandList* commandList, CameraComponent* camera, UINT frame_index);

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

    // [추가] 동적 디스크립터 힙 생성을 위한 헬퍼 함수
    void create_dynamic_descriptor_heap(UINT capacity = 8192);

    // [추가] 동적 바인딩을 위한 멤버 변수
    ComPtr<ID3D12DescriptorHeap> _dynamic_descriptor_heap;
    UINT _dynamic_descriptor_heap_capacity = 0;
    UINT _current_dynamic_descriptor_index = 0;
    UINT _descriptor_size = 0;

    // 프레임당 할당 가능한 최대 디스크립터 수 
    // DW설명 : 스왑체인 버퍼 수에 맞춰서 우리가 할당한 디스크립터 힙의 개수를 나누어야 함
    UINT _max_descriptors_per_frame = 0;
};
