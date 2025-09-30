#pragma once
#include "stdafx.h"

#include "Behaviour.h"
#include "Component.h"
#include "Camera.h"
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

class RenderComponent : public Behaviour
{
public:
    RenderComponent();
    virtual ~RenderComponent() = default;

    // render 함수는 이제 Renderer에 의해 호출됩니다.
    virtual void render(ID3D12GraphicsCommandList* commandList, Camera* camera);
    virtual void awake() override;
    // --- Getters & Setters ---
    void set_mesh(std::shared_ptr<Mesh> mesh) { _mesh = mesh; }
    std::shared_ptr<Mesh> mesh() const { return _mesh; }

    // (기존에 get_material_shader 같은 함수가 있었다면, 유사한 역할을 할 함수)
    void set_shader(std::shared_ptr<Shader> shader) { _shader = shader; }
    std::shared_ptr<Shader> shader() const { return _shader; }

    // 이 컴포넌트가 사용할 PSO의 이름을 설정하고 가져옵니다.
    void set_pso_name(const std::string& name) { _psoName = name; }
    const std::string& pso_name() const { return _psoName; }

private:
    std::shared_ptr<Mesh> _mesh;
    std::shared_ptr<Shader> _shader; // 셰이더 또는 머티리얼
    std::string _psoName = "default"; // 사용할 PSO의 이름 (기본값 "default")

    // [추가] 이 RenderComponent만의 고유한 상수 버퍼 관련 멤버들
    ComPtr<ID3D12Resource> _cbGameObjectInfo;
    CbGameObjectInfo* _mappedCbGameObjectInfo = nullptr;
};

