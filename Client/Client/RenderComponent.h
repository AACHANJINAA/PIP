#pragma once
#include "stdafx.h"

#include "Behavior.h"
#include "Camera.h"
#include "CameraComponent.h"
#include "Mesh.h"

class Shader;

struct Material
{
	XMFLOAT4 _ambient;     // 환경광(Ambient) 
	XMFLOAT4 _diffuse;     // 난반사(Diffuse) 
	XMFLOAT4 _specular;    // 정반사(Specular)
	XMFLOAT4 _emissive;    // 방출광(Emissive)
};

struct CbGameObjectInfo
{
	XMFLOAT4X4 _world;
    XMFLOAT4X4 _worldInverseTranspose;
    int bReceiveShadow;
    int otherplayer_id;
    XMFLOAT2 padding;
};

class Material_Shader
{
public:
	Material_Shader();
	virtual ~Material_Shader();

	void set_shader(const std::shared_ptr<Shader>& shader);
	void set_shader_root_signature(ComPtr<ID3D12RootSignature> root_signature);
	void set_root_signature(ComPtr<ID3D12GraphicsCommandList> command_list);

	std::shared_ptr<Shader> get_shader() const { return _shader; }

private:
	ComPtr<ID3D12RootSignature> _rootSignature;
	std::shared_ptr<Material> _material;
	std::shared_ptr<Shader> _shader;
};

class RenderComponent : public Behavior
{
public:
    RenderComponent();
    virtual ~RenderComponent();

    // render 함수는 이제 Renderer에 의해 호출됩니다.
    virtual void render(ID3D12GraphicsCommandList* commandList, UINT frame_index);
	// render_CascadeShadowMap 함수
	virtual void render_CascadeShadowMap(ID3D12GraphicsCommandList* commandList, UINT frame_index);

    // --- Getters & Setters ---
    virtual void set_mesh(const std::shared_ptr<Mesh>& mesh) { _mesh = mesh; }

    //void set_material(std::shared_ptr<GltfMaterial> material) { _material = material; };
    //void set_materials(const std::vector<std::shared_ptr<GltfMaterial>>& materials) { _materials = materials; }
   
    virtual void set_pso_name(const std::string& name) { _psoName = name; }

    virtual BoundingOrientedBox get_world_bounding_box() const;
    virtual std::shared_ptr<Mesh> mesh() const { return _mesh; }
    virtual const std::string& pso_name() const;

    virtual bool is_visible(const BoundingFrustum& frustum) const;

    virtual void pre_render(ID3D12GraphicsCommandList* commandList, class Renderer* renderer);

    void set_frustum_culling_enabled(bool enabled) { _frustumCullingEnabled = enabled; }

    UINT get_occlusion_query_index();
    XMMATRIX get_occlusion_box_world_matrix();
    void update_world_matrix_cb(UINT frame_index);
    void set_skip_occlusion(bool skip) { _skipOcclusion = skip; }
    bool skip_occlusion() const { return _skipOcclusion; }
	UINT get_occlusion_query_index() const { return _occlusionQueryIndex; }
	UINT set_occlusion_query_index(UINT index) { return _occlusionQueryIndex = index; }
	bool has_allocated_index() const { return _occlusionQueryIndex != 0xFFFFFFFF; }
	D3D12_GPU_VIRTUAL_ADDRESS get_cb_gpu_address(UINT frame_index) const { return _cbGameObjectInfo[frame_index]->GetGPUVirtualAddress(); }

protected:
    std::shared_ptr<Mesh> _mesh;
    //std::shared_ptr<GltfMaterial> _material;                // 셰이더 또는 머티리얼
    //std::vector<std::shared_ptr<GltfMaterial>> _materials;  // 서브 리소스를 이용한 다중 텍스쳐링을 위한 변수
    std::string _psoName = "default";
    // 프레임 개수만큼 늘리기
    std::array<ComPtr<ID3D12Resource>, SWAP_CHAIN_BUFFERS> _cbGameObjectInfo;
    std::array<CbGameObjectInfo*, SWAP_CHAIN_BUFFERS> _mappedCbGameObjectInfo;

    bool _frustumCullingEnabled = true;

	// occlusion query를 위한 인덱스
    UINT _occlusionQueryIndex = 0xFFFFFFFF; // 초기값
    bool _skipOcclusion = false;

    UINT64 _lastUpdatedFrame = 0xFFFFFFFFFFFFFFFF;
};

