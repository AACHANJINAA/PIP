#pragma once
#include "Behaviour.h"
#include "Mesh.h"

class SkyboxRenderComponent : public Behaviour
{
public:
    SkyboxRenderComponent();
    virtual ~SkyboxRenderComponent() = default;

    void render(ID3D12GraphicsCommandList* commandList, UINT frame_index);
    void set_mesh(const std::shared_ptr<Mesh>& mesh) { _mesh = mesh; }
    void set_pso_name(const std::string& name) { _psoName = name; }
    std::shared_ptr<Mesh> mesh() const { return _mesh; }
    const std::string& pso_name() const { return _psoName; }

private:
    std::shared_ptr<Mesh> _mesh;
    std::string _psoName = "skybox";
};