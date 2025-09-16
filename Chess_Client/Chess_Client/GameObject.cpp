#include "stdafx.h"
#include "GameObject.h"
#include "Shader.h"
#include "BoardCube.h"

// (추가) 머터리얼 생성자 & 소멸자 & SetShader [PONG]

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
	if (_shader) // 셰이더가 없으면 루트 시그너쳐도 있을 이유가 없음
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
	XMStoreFloat4x4(&_worldMatrix, XMMatrixIdentity());
}
GameObject::~GameObject()
{
	if (_mesh) _mesh->Release();
	/*if (m_pShader)
	{
		m_pShader->ReleaseShaderVariables();
		m_pShader->Release();
	}*/
	if (_materialShader) _materialShader->Release(); // (수정) 머터리얼로 바뀜 [PONG]
}

// (수정) [PONG]
void GameObject::set_shader(std::shared_ptr<Shader> shader)
{
	if (!_materialShader) _materialShader = new Material_Shader(); // 재질이 없으면 새로 생성
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
	//정점 버퍼를 위한 업로드 버퍼를 소멸시킨다. 
	if (_mesh) 
		_mesh->ReleaseUploadBuffers();
}

void GameObject::on_prepare_render(ID3D12GraphicsCommandList* command_List)
{
	if (_materialShader) // 머터리얼이 있는지 확인
	{
		if (_materialShader->_shader) // 머터리얼에 셰이더가 있는지 확인
		{
			_materialShader->set_root_signature(command_List);
			// 머터리얼의 셰이더를 사용하여 렌더링
			_materialShader->_shader->OnPrepareRender(command_List);
		}
	}
}

// (수정) [PONG]
void GameObject::render(ID3D12GraphicsCommandList* command_list, Camera* camera)
{
	if (is_visible(camera))
	{
		// 렌더링 상태 설정
		//OnPrepareRender(pd3dCommandList);

		// 개별 데이터 업데이트 (본인, 카메라)
		UpdateShaderVariables(command_list);
		camera->UpdateShaderVariables(command_list);

		// 그리기 실행
		if (_mesh) _mesh->Render(command_list);
	}
}

void GameObject::CreateShaderVariables(ID3D12Device* pd3dDevice,
	ID3D12GraphicsCommandList* pd3dCommandList)
{
	// 1. 상수 버퍼 리소스 생성
	_cbGameObject = ::CreateBufferResource(pd3dDevice, pd3dCommandList, nullptr, sizeof(CbGameObjectInfo), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);

	// [중요] 1-1. 버퍼 생성에 성공했는지 반드시 확인!
	if (!_cbGameObject)
	{
		// 여기서 에러 로그를 남기거나, 예외를 던지거나, 메시지 박스를 띄워
		// 프로그램이 즉시 문제를 인지하고 멈추도록 해야 합니다.
		MessageBox(NULL, L"GameObject Constant Buffer Creation Failed!", L"Error", MB_OK);
		return; // 실패했으므로 더 이상 진행하지 않음
	}

	// 2. 생성된 버퍼를 CPU 주소에 맵핑
	D3D12_RANGE d3dReadRange{ 0, 0 };
	HRESULT hResult = _cbGameObject->Map(0, &d3dReadRange, reinterpret_cast<void**>(&_cbMappedGameObject));

	// [중요] 2-1. 맵핑에 성공했는지 반드시 확인!
	if (FAILED(hResult))
	{
		// Map()이 실패하면 m_pcbMappedGameObject는 nullptr인 상태로 남게 됩니다.
		// 여기서 문제를 인지하고 멈춰야 합니다.
		_cbMappedGameObject = nullptr; // 안전을 위해 명시적으로 nullptr 처리
		MessageBox(NULL, L"GameObject Constant Buffer Map Failed!", L"Error", MB_OK);
		return;
	}
}

void GameObject::ReleaseShaderVariables()
{

}

void GameObject::UpdateShaderVariables(ID3D12GraphicsCommandList* pd3dCommandList)
{
	// 1. 데이터 준비
	XMStoreFloat4x4(&_cbMappedGameObject->_world, XMMatrixTranspose(XMLoadFloat4x4(&_worldMatrix)));

	// 2. GPU에 바인딩
	D3D12_GPU_VIRTUAL_ADDRESS cbGpuAddress = _cbGameObject->GetGPUVirtualAddress();
	pd3dCommandList->SetGraphicsRootConstantBufferView(0, cbGpuAddress);
}

void GameObject::rotate(const XMFLOAT3& axis, float angle)
{
	XMMATRIX mtxRotate = XMMatrixRotationAxis(XMLoadFloat3(&axis),
		XMConvertToRadians(angle));
	_worldMatrix = Matrix4x4::Multiply(mtxRotate, _worldMatrix);
}

void GameObject::set_position(float x, float y, float z)
{
	_position = { x,y,z };
}
void GameObject::set_position(XMFLOAT3 position)
{
	set_position(position.x, position.y, position.z);
}


void GameObject::SetScale(float x, float y, float z)
{
	_scale = {x,y,z};
}

XMFLOAT3 GameObject::position()
{
	return _position;
}
//게임 객체의 로컬 z-축 벡터를 반환한다. 
XMFLOAT3 GameObject::look()
{
	return(Vector3::Normalize(XMFLOAT3(_worldMatrix._31, _worldMatrix._32, _worldMatrix._33)));
}

XMFLOAT3 GameObject::size()
{
	return _scale;
}

//게임 객체의 로컬 y-축 벡터를 반환한다. 
XMFLOAT3 GameObject::up()
{
	return(Vector3::Normalize(XMFLOAT3(_worldMatrix._21, _worldMatrix._22, _worldMatrix._23)));
}
//게임 객체의 로컬 x-축 벡터를 반환한다. 
XMFLOAT3 GameObject::right()
{
	return(Vector3::Normalize(XMFLOAT3(_worldMatrix._11, _worldMatrix._12, _worldMatrix._13)));
}
//게임 객체를 주어진 각도로 회전한다. 
void GameObject::rotate(float pitch, float yaw, float roll)
{
	_rotate = { pitch, yaw, roll };
}

void GameObject::move(XMFLOAT3& direction, float speed)
{
	_position.x = _position.x + direction.x * speed;
	_position.y = _position.y + direction.y * speed;
	_position.z = _position.z + direction.z * speed;
}

void GameObject::move(float x, float y, float z)
{
	_position.x += x;
	_position.y += y;
	_position.z += z;
}

void GameObject::look_to(XMFLOAT3& look_to, XMFLOAT3& up)
{
	XMFLOAT4X4 xmf4x4View = Matrix4x4::LookToLH(position(), look_to, up);
	_worldMatrix._11 = xmf4x4View._11; _worldMatrix._12 = xmf4x4View._21; _worldMatrix._13 = xmf4x4View._31;
	_worldMatrix._21 = xmf4x4View._12; _worldMatrix._22 = xmf4x4View._22; _worldMatrix._23 = xmf4x4View._32;
	_worldMatrix._31 = xmf4x4View._13; _worldMatrix._32 = xmf4x4View._23; _worldMatrix._33 = xmf4x4View._33;
}

void GameObject::look_to(XMFLOAT3& look_to)
{
	XMFLOAT4X4 xmf4x4View = Matrix4x4::LookToLH(position(), look_to, up());
	_worldMatrix._11 = xmf4x4View._11; _worldMatrix._12 = xmf4x4View._21; _worldMatrix._13 = xmf4x4View._31;
	_worldMatrix._21 = xmf4x4View._12; _worldMatrix._22 = xmf4x4View._22; _worldMatrix._23 = xmf4x4View._32;
	_worldMatrix._31 = xmf4x4View._13; _worldMatrix._32 = xmf4x4View._23; _worldMatrix._33 = xmf4x4View._33;
}

void GameObject::generate_ray_for_picking(XMVECTOR& pick_position, XMMATRIX& view_matrix, XMVECTOR& pick_ray_origin, XMVECTOR& pick_ray_direction)
{
	XMMATRIX xmmtxToModel = XMMatrixInverse(NULL, XMLoadFloat4x4(&_worldMatrix) * view_matrix);

	XMFLOAT3 xmf3CameraOrigin(0.0f, 0.0f, 0.0f);
	pick_ray_origin = XMVector3TransformCoord(XMLoadFloat3(&xmf3CameraOrigin), xmmtxToModel);
	pick_ray_direction = XMVector3TransformCoord(pick_position, xmmtxToModel);
	pick_ray_direction = XMVector3Normalize(pick_ray_direction - pick_ray_origin);
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
	// DW질문 : nIntersections 이거 개수 왜 새는건지? 밖에서도 0 이상인지만 확인하고 끝남, 혹시 중요한 다른 의미가 있는지?
	// 영상에서도 nIntersections이 변수에 대한 언급은 딱히 없었음
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
	if (_mesh)
	{
		XMVectorScale(XMLoadFloat3(&_mesh->m_xmOOBB.Extents), size().x);
		_mesh->m_xmOOBB.Transform(_orientedBoundingBox, XMLoadFloat4x4(&_worldMatrix));
		XMStoreFloat4(&_orientedBoundingBox.Orientation, XMQuaternionNormalize(XMLoadFloat4(&_orientedBoundingBox.Orientation)));
	}
}

bool GameObject::is_visible(Camera* camera)
{
	//OnPrepareRender();
	if (!camera) return false; 

	BoundingOrientedBox worldOOBB = _mesh->GetBoundingBox();
	worldOOBB.Transform(worldOOBB, XMLoadFloat4x4(&_worldMatrix));

	XMVECTOR orientationQuat = XMLoadFloat4(&worldOOBB.Orientation);
	orientationQuat = XMQuaternionNormalize(orientationQuat);
	XMStoreFloat4(&worldOOBB.Orientation, orientationQuat);

	return camera->IsInFrustum(worldOOBB);
}


void GameObject::update()
{
	_worldMatrix = Matrix4x4::Identity();

	// 1. 크기, 회전, 이동 행렬을 각각 생성
	XMMATRIX scaleMatrix = XMMatrixScaling(_scale.x, _scale.y, _scale.z);
	XMMATRIX rotateMatrix = XMMatrixRotationRollPitchYaw(XMConvertToRadians(_rotate.x), XMConvertToRadians(_rotate.y), XMConvertToRadians(_rotate.z));
	XMMATRIX translateMatrix = XMMatrixTranslation(_position.x, _position.y, _position.z);

	// 2. S-R-T 순서로 행렬을 곱하여 월드 행렬을 만듦
	// (참고: Right, Up, Look 벡터로 회전을 만들고 싶다면 rotateMatrix를 해당 행렬로 교체)
	XMMATRIX worldMatrix = scaleMatrix * rotateMatrix * translateMatrix;
	XMStoreFloat4x4(&_worldMatrix, worldMatrix);


	//if (nullptr != m_pMaterial)
	//{
	//	// 이 셰이더를 쓰는 객체가 GLB 모델이라고 가정
	//	if (typeid(CObjectsShader) == typeid(*(m_pMaterial->_Shader)))
	//	{
	//		// Z축을 뒤집는 변환 행렬 생성
	//		XMMATRIX zFlipMatrix = XMMatrixScaling(1.0f, 1.0f, -1.0f);

	//		// 기존 월드 행렬 앞에 곱해서 최종 월드 행렬을 만듦
	//		worldMatrix = zFlipMatrix * worldMatrix;
	//		XMStoreFloat4x4(&m_xmf4x4World, worldMatrix);
	//	}
	//}
}