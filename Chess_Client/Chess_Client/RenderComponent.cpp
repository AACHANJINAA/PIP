#include "stdafx.h"
#include "RenderComponent.h"
#include "TransformComponent.h"
#include "GameObject.h"
#include "Shader.h"

Material_Shader::Material_Shader()
{
}
Material_Shader::~Material_Shader()
{
}

void Material_Shader::set_shader(const std::shared_ptr<Shader>& shader)
{
	_shader = shader;
}

void Material_Shader::set_shader_root_signature(ComPtr<ID3D12RootSignature> root_signature)
{
	if (_shader)
	{
		_rootSignature = root_signature;
	}
}

void Material_Shader::set_root_signature(ComPtr<ID3D12GraphicsCommandList> command_list)
{
	if (_rootSignature)
	{
		command_list->SetGraphicsRootSignature(_rootSignature.Get());
	}
}


void RenderComponent::set_shader(std::shared_ptr<Shader> shader)
{
	_materialShader->set_shader(shader);
}

void RenderComponent::set_material(std::shared_ptr<Material_Shader> material)
{
	_materialShader = material;
}

void RenderComponent::set_mesh(std::shared_ptr<Mesh> mesh)
{
	_mesh = mesh;
}

void RenderComponent::release_upload_buffers()
{
	if (_mesh)
		_mesh->ReleaseUploadBuffers();
}

void RenderComponent::on_prepare_render(ComPtr<ID3D12GraphicsCommandList> command_List)
{
	if (_materialShader)
	{
		if (_materialShader->get_shader())
		{
			_materialShader->set_root_signature(command_List.Get());
			_materialShader->get_shader()->OnPrepareRender(command_List.Get());
		}
	}
}

bool RenderComponent::is_visible(Camera* camera)
{
	return true;
	//OnPrepareRender();
	if (!camera) return false;

	auto Transform = get_GameObject()->get_component<TransformComponent>();
	//if (!Transform) return true; //만약, TransformComponent가 없으면 이거 쓰기

	BoundingOrientedBox worldOOBB = _mesh->GetBoundingBox();
	worldOOBB.Transform(worldOOBB, XMLoadFloat4x4(&Transform->get_world_matrix()));

	XMVECTOR orientationQuat = XMLoadFloat4(&worldOOBB.Orientation);
	orientationQuat = XMQuaternionNormalize(orientationQuat);
	XMStoreFloat4(&worldOOBB.Orientation, orientationQuat);

	return camera->IsInFrustum(worldOOBB);
}

void RenderComponent::CreateShaderVariables(ComPtr<ID3D12Device> pd3dDevice, ComPtr<ID3D12GraphicsCommandList> pd3dCommandList)
{
	_cbGameObject = ::CreateBufferResource(pd3dDevice.Get(), pd3dCommandList.Get(), nullptr, sizeof(CbGameObjectInfo), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);

	if (!_cbGameObject)
	{
		MessageBox(NULL, L"GameObject Constant Buffer Creation Failed!", L"Error", MB_OK);
		return;
	}

	D3D12_RANGE d3dReadRange{ 0, 0 };
	HRESULT hResult = _cbGameObject->Map(0, &d3dReadRange, reinterpret_cast<void**>(&_cbMappedGameObject));

	if (FAILED(hResult))
	{
		_cbMappedGameObject = nullptr;
		MessageBox(NULL, L"GameObject Constant Buffer Map Failed!", L"Error", MB_OK);
		return;
	}
}

void RenderComponent::UpdateShaderVariables(ComPtr<ID3D12GraphicsCommandList> pd3dCommandList)
{
	auto Transform = get_GameObject()->get_component<TransformComponent>();
	if (Transform)
	{
		XMStoreFloat4x4(&_cbMappedGameObject->_world, XMMatrixTranspose(XMLoadFloat4x4(&Transform->get_world_matrix())));

	}

	D3D12_GPU_VIRTUAL_ADDRESS cbGpuAddress = _cbGameObject->GetGPUVirtualAddress();
	pd3dCommandList->SetGraphicsRootConstantBufferView(0, cbGpuAddress);
}


void RenderComponent::ReleaseShaderVariables()
{
	if (_cbGameObject)
	{
		D3D12_RANGE d3dRange = { 0, 0 };
		_cbGameObject->Unmap(0, &d3dRange);
		_cbGameObject.Reset();
	}
}

void RenderComponent::start()
{
}

void RenderComponent::update(float DeltaTime)
{
}

void RenderComponent::render(ComPtr<ID3D12GraphicsCommandList> command_list, Camera* camera)
{
	auto Transform = get_GameObject()->get_component<TransformComponent>();

	if (is_visible(camera))
	{
		UpdateShaderVariables(command_list);
		camera->UpdateShaderVariables(command_list.Get());

		if (_mesh) _mesh->Render(command_list.Get());
	}
}
