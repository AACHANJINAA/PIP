#pragma once
#include "stdafx.h"
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

class RenderComponent : public Component
{
public:
	RenderComponent(GameObject* Owner)
		: Component(Owner)
	{
	}
	~RenderComponent() override = default;
public:
	void start() override;
	void update(float DeltaTime) override;
	virtual void render(ComPtr<ID3D12GraphicsCommandList> command_list, Camera* camera) override;

public:
	virtual void set_mesh(std::shared_ptr<Mesh> mesh);
	virtual void set_shader(std::shared_ptr<Shader> shader);
	void set_material(std::shared_ptr<Material_Shader> material);

	void release_upload_buffers();

	virtual void on_prepare_render(ComPtr<ID3D12GraphicsCommandList> command_List);

	bool is_visible(Camera* camera = nullptr);

	virtual void CreateShaderVariables(ComPtr<ID3D12Device>pd3dDevice, ComPtr<ID3D12GraphicsCommandList> pd3dCommandList);
	virtual void UpdateShaderVariables(ComPtr<ID3D12GraphicsCommandList> pd3dCommandList);
	virtual void ReleaseShaderVariables();

	std::shared_ptr<Mesh> get_mesh() const { return _mesh; }
	std::shared_ptr<Material_Shader> get_material_shader() const { return _materialShader; }

protected:
	std::shared_ptr<Mesh> _mesh;
	std::shared_ptr<Material_Shader> _materialShader;
	ComPtr<ID3D12Resource> _cbGameObject;
	// Map 직관적으로 표현하려면 raw 포인터로 계속 쓰는게 나을 듯
	CbGameObjectInfo* _cbMappedGameObject = nullptr;
};

