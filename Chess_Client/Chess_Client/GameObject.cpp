#include "stdafx.h"
#include "GameObject.h"
#include "Shader.h"
#include "BoardCube.h"

Material_Shader::Material_Shader()
{
	_material = new Material();
}
Material_Shader::~Material_Shader()
{
	if (_material) 
		delete _material;
	/*if (_Shader)
		_Shader->Release();*/
}

void Material_Shader::set_shader(const std::shared_ptr<Shader>& shader)
{
	if (_shader) _shader->Release();
	_shader = shader;
	if (_shader) _shader->AddRef();
}

void Material_Shader::set_shader_root_signature(ID3D12RootSignature* root_signature)
{
	if (_shader)
	{
		_rootSignature = root_signature;
	}
}

void Material_Shader::set_root_signature(ID3D12GraphicsCommandList* command_list)
{
	if(_rootSignature)
	{
		command_list->SetGraphicsRootSignature(_rootSignature);
	}
}



GameObject::GameObject()
{
	add_component<TransformComponent>(this);
}
GameObject::~GameObject()
{
	if (_mesh) _mesh->Release();
	/*if (m_pShader)
	{
		m_pShader->ReleaseShaderVariables();
		m_pShader->Release();
	}*/
	if (_materialShader) _materialShader->Release(); 
}

void GameObject::set_shader(std::shared_ptr<Shader> shader)
{
	if (!_materialShader) _materialShader = new Material_Shader(); 
	if (_materialShader) _materialShader->set_shader(shader);
}

void GameObject::set_material(Material_Shader* material)
{
	if (_materialShader) _materialShader->Release();
	_materialShader = material;
	if (_materialShader) _materialShader->AddRef();
}

void GameObject::set_mesh(Mesh* mesh)
{
	if (_mesh) 
	{
		_mesh->Release();
	}

	_mesh = mesh;

	if (_mesh) 
	{
		_mesh->AddRef();
	}
}

void GameObject::release_upload_buffers()
{
	if (_mesh) 
		_mesh->ReleaseUploadBuffers();
}

void GameObject::on_prepare_render(ID3D12GraphicsCommandList* command_List)
{
	if (_materialShader) 
	{
		if (_materialShader->_shader) 
		{
			_materialShader->set_root_signature(command_List);
			_materialShader->_shader->OnPrepareRender(command_List);
		}
	}
}

void GameObject::render(ID3D12GraphicsCommandList* command_list, Camera* camera)
{
	if (is_visible(camera))
	{
		//OnPrepareRender(pd3dCommandList);

		UpdateShaderVariables(command_list);
		camera->UpdateShaderVariables(command_list);

		if (_mesh) _mesh->Render(command_list);
	}
}

void GameObject::CreateShaderVariables(ID3D12Device* pd3dDevice,
	ID3D12GraphicsCommandList* pd3dCommandList)
{
	_cbGameObject = ::CreateBufferResource(pd3dDevice, pd3dCommandList, nullptr, sizeof(CbGameObjectInfo), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);

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

void GameObject::ReleaseShaderVariables()
{

}

void GameObject::UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList)
{
	auto Transform = get_component<TransformComponent>();
	if (Transform)
	{
		XMStoreFloat4x4(&_cbMappedGameObject->_world, XMMatrixTranspose(XMLoadFloat4x4(&Transform->get_world_matrix())));

	}

	D3D12_GPU_VIRTUAL_ADDRESS cbGpuAddress = _cbGameObject->GetGPUVirtualAddress();
	pd3dCommandList->SetGraphicsRootConstantBufferView(0, cbGpuAddress);
}

void GameObject::generate_ray_for_picking(XMVECTOR& xmvPickPosition, XMMATRIX& xmmtxView, XMVECTOR& xmvPickRayOrigin, XMVECTOR& xmvPickRayDirection)
{
	auto Transform = get_component<TransformComponent>();
	if (Transform) {
		XMMATRIX xmmtxToModel = XMMatrixInverse(NULL, XMLoadFloat4x4(&Transform->get_world_matrix()) * xmmtxView);

		XMFLOAT3 xmf3CameraOrigin(0.0f, 0.0f, 0.0f);
		xmvPickRayOrigin = XMVector3TransformCoord(XMLoadFloat3(&xmf3CameraOrigin), xmmtxToModel);
		xmvPickRayDirection = XMVector3TransformCoord(xmvPickPosition, xmmtxToModel);
		xmvPickRayDirection = XMVector3Normalize(xmvPickRayDirection - xmvPickRayOrigin);
	}
}

int GameObject::pick_object_by_ray_intersection(XMVECTOR& pick_position, XMMATRIX& view_matrix, float* hit_distance)
{
	int nIntersected = 0;
	if (_mesh)
	{
		XMVECTOR xmvPickRayOrigin, xmvPickRayDirection;
		generate_ray_for_picking(pick_position, view_matrix, xmvPickRayOrigin, xmvPickRayDirection);
		nIntersected = _mesh->CheckRayIntersection(xmvPickRayOrigin, xmvPickRayDirection, hit_distance);
	}
	return(nIntersected);
}

bool GameObject::pick_model_obb(XMVECTOR& pick_position, XMMATRIX& view_matrix, float* hit_distance)
{
	if (_mesh)
	{
		XMVECTOR xmvPickRayOrigin, xmvPickRayDirection;
		generate_ray_for_picking(pick_position, view_matrix, xmvPickRayOrigin, xmvPickRayDirection);
		return(_mesh->m_xmOOBB.Intersects(xmvPickRayOrigin, xmvPickRayDirection, *hit_distance));
	}
	return false;
}

void GameObject::update_bounding_box()
{
	auto Transform = get_component<TransformComponent>();
	if (_mesh)
	{
		if (Transform)
		{
			XMVectorScale(XMLoadFloat3(&_mesh->m_xmOOBB.Extents), Transform->get_size().x);
		}
		_mesh->m_xmOOBB.Transform(_mesh->m_xmOOBB, XMLoadFloat4x4(&Transform->get_world_matrix()));
		XMStoreFloat4(&_mesh->m_xmOOBB.Orientation, XMQuaternionNormalize(XMLoadFloat4(&_mesh->m_xmOOBB.Orientation)));
	}
}

bool GameObject::is_visible(Camera * camera)
{
	//return true;
	//OnPrepareRender();
	if (!camera) return false; 

	auto Transform = get_component<TransformComponent>();
	//if (!Transform) return true; //만약, TransformComponent가 없으면 이거 쓰기

	BoundingOrientedBox worldOOBB = _mesh->GetBoundingBox();
	worldOOBB.Transform(worldOOBB, XMLoadFloat4x4(&Transform->get_world_matrix()));

	XMVECTOR orientationQuat = XMLoadFloat4(&worldOOBB.Orientation);
	orientationQuat = XMQuaternionNormalize(orientationQuat);
	XMStoreFloat4(&worldOOBB.Orientation, orientationQuat);

	return camera->IsInFrustum(worldOOBB);
}


void GameObject::update(float DeltaTime)
{
	for (const std::shared_ptr<Component>& component : _components)
	{
		component->update(DeltaTime);
	}
}