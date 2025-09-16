#include "stdafx.h"
#include "GameObject.h"
#include "Shader.h"
#include "BoardCube.h"

// (추가) 머터리얼 생성자 & 소멸자 & SetShader [PONG]

Material_Shader::Material_Shader()
{
	m_pMaterial = new MATERIAL();
}
Material_Shader::~Material_Shader()
{
	if (m_pMaterial) 
		delete m_pMaterial;
	/*if (_Shader)
		_Shader->Release();*/
}

void Material_Shader::SetShader(std::shared_ptr<Shader> pShader)
{
	if (_Shader) _Shader->Release();
	_Shader = pShader;
	if (_Shader) _Shader->AddRef();
}

void Material_Shader::SetShaderRootSignature(ID3D12RootSignature* RootSignature)
{
	if (_Shader) // 셰이더가 없으면 루트 시그너쳐도 있을 이유가 없음
	{
		_RootSignature = RootSignature;
	}
}

void Material_Shader::SetRootSignature(ID3D12GraphicsCommandList* pd3dCommandList)
{
	if(_RootSignature)
	{
		pd3dCommandList->SetGraphicsRootSignature(_RootSignature);
	}
}



GameObject::GameObject()
{
	AddComponent<TransformComponent>(this);
}
GameObject::~GameObject()
{
	if (m_pMesh) m_pMesh->Release();
	/*if (m_pShader)
	{
		m_pShader->ReleaseShaderVariables();
		m_pShader->Release();
	}*/
	if (m_pMaterial) m_pMaterial->Release(); // (수정) 머터리얼로 바뀜 [PONG]
}

// (수정) [PONG]
void GameObject::SetShader(std::shared_ptr<Shader> pShader)
{
	if (!m_pMaterial) m_pMaterial = new Material_Shader(); // 재질이 없으면 새로 생성
	if (m_pMaterial) m_pMaterial->SetShader(pShader);
}

void GameObject::SetMaterial(Material_Shader* pMaterial)
{
	if (m_pMaterial) m_pMaterial->Release();
	m_pMaterial = pMaterial;
	if (m_pMaterial) m_pMaterial->AddRef();
}

void GameObject::SetMesh(Mesh* pMesh)
{
	if (m_pMesh) 
	{
		m_pMesh->Release();
	}

	m_pMesh = pMesh;

	if (m_pMesh) 
	{
		m_pMesh->AddRef();
	}
}

void GameObject::ReleaseUploadBuffers()
{
	//정점 버퍼를 위한 업로드 버퍼를 소멸시킨다. 
	if (m_pMesh) 
		m_pMesh->ReleaseUploadBuffers();
}

void GameObject::OnPrepareRender(ID3D12GraphicsCommandList* pd3dCommandList)
{
	if (m_pMaterial) // 머터리얼이 있는지 확인
	{
		if (m_pMaterial->_Shader) // 머터리얼에 셰이더가 있는지 확인
		{
			m_pMaterial->SetRootSignature(pd3dCommandList);
			// 머터리얼의 셰이더를 사용하여 렌더링
			m_pMaterial->_Shader->OnPrepareRender(pd3dCommandList);
		}
	}
}

// (수정) [PONG]
void GameObject::Render(ID3D12GraphicsCommandList* pd3dCommandList, Camera* pCamera)
{
	if (IsVisible(pCamera))
	{
		// 렌더링 상태 설정
		//OnPrepareRender(pd3dCommandList);

		// 개별 데이터 업데이트 (본인, 카메라)
		UpdateShaderVariables(pd3dCommandList);
		pCamera->UpdateShaderVariables(pd3dCommandList);

		// 그리기 실행
		if (m_pMesh) m_pMesh->Render(pd3dCommandList);
	}
}

void GameObject::CreateShaderVariables(ID3D12Device* pd3dDevice,
	ID3D12GraphicsCommandList* pd3dCommandList)
{
	// 1. 상수 버퍼 리소스 생성
	m_pd3dcbGameObject = ::CreateBufferResource(pd3dDevice, pd3dCommandList, nullptr, sizeof(CB_GAMEOBJECT_INFO), D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr);

	// [중요] 1-1. 버퍼 생성에 성공했는지 반드시 확인!
	if (!m_pd3dcbGameObject)
	{
		// 여기서 에러 로그를 남기거나, 예외를 던지거나, 메시지 박스를 띄워
		// 프로그램이 즉시 문제를 인지하고 멈추도록 해야 합니다.
		MessageBox(NULL, L"GameObject Constant Buffer Creation Failed!", L"Error", MB_OK);
		return; // 실패했으므로 더 이상 진행하지 않음
	}

	// 2. 생성된 버퍼를 CPU 주소에 맵핑
	D3D12_RANGE d3dReadRange{ 0, 0 };
	HRESULT hResult = m_pd3dcbGameObject->Map(0, &d3dReadRange, reinterpret_cast<void**>(&_pcbMappedGameObject));

	// [중요] 2-1. 맵핑에 성공했는지 반드시 확인!
	if (FAILED(hResult))
	{
		// Map()이 실패하면 m_pcbMappedGameObject는 nullptr인 상태로 남게 됩니다.
		// 여기서 문제를 인지하고 멈춰야 합니다.
		_pcbMappedGameObject = nullptr; // 안전을 위해 명시적으로 nullptr 처리
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
	auto Transform = GetComponent<TransformComponent>();
	if (Transform)
	{
		XMStoreFloat4x4(&_pcbMappedGameObject->_4x4World, XMMatrixTranspose(XMLoadFloat4x4(&Transform->GetWorldMatrix())));

	}

	// 2. GPU에 바인딩
	D3D12_GPU_VIRTUAL_ADDRESS cbGpuAddress = m_pd3dcbGameObject->GetGPUVirtualAddress();
	pd3dCommandList->SetGraphicsRootConstantBufferView(0, cbGpuAddress);
}

void GameObject::GenerateRayForPicking(XMVECTOR& xmvPickPosition, XMMATRIX& xmmtxView, XMVECTOR& xmvPickRayOrigin, XMVECTOR& xmvPickRayDirection)
{
	auto Transform = GetComponent<TransformComponent>();
	if (Transform) {
		XMMATRIX xmmtxToModel = XMMatrixInverse(NULL, XMLoadFloat4x4(&Transform->GetWorldMatrix()) * xmmtxView);

		XMFLOAT3 xmf3CameraOrigin(0.0f, 0.0f, 0.0f);
		xmvPickRayOrigin = XMVector3TransformCoord(XMLoadFloat3(&xmf3CameraOrigin), xmmtxToModel);
		xmvPickRayDirection = XMVector3TransformCoord(xmvPickPosition, xmmtxToModel);
		xmvPickRayDirection = XMVector3Normalize(xmvPickRayDirection - xmvPickRayOrigin);
	}
}

int GameObject::PickObjectByRayIntersection(XMVECTOR& xmvPickPosition, XMMATRIX& xmmtxView, float* pfHitDistance)
{
	int nIntersected = 0;
	if (m_pMesh)
	{
		XMVECTOR xmvPickRayOrigin, xmvPickRayDirection;
		GenerateRayForPicking(xmvPickPosition, xmmtxView, xmvPickRayOrigin, xmvPickRayDirection);
		nIntersected = m_pMesh->CheckRayIntersection(xmvPickRayOrigin, xmvPickRayDirection, pfHitDistance);
	}
	return(nIntersected);
	// DW질문 : nIntersections 이거 개수 왜 새는건지? 밖에서도 0 이상인지만 확인하고 끝남, 혹시 중요한 다른 의미가 있는지?
	// 영상에서도 nIntersections이 변수에 대한 언급은 딱히 없었음
}

bool GameObject::PickModelOBB(XMVECTOR& xmPickPosition, XMMATRIX& xmmtxView, float* pfHitDistance)
{
	if (m_pMesh)
	{
		XMVECTOR xmvPickRayOrigin, xmvPickRayDirection;
		GenerateRayForPicking(xmPickPosition, xmmtxView, xmvPickRayOrigin, xmvPickRayDirection);
		return(m_pMesh->m_xmOOBB.Intersects(xmvPickRayOrigin, xmvPickRayDirection, *pfHitDistance));
	}
	return false;
}

void GameObject::UpdateBoundingBox()
{
	auto Transform = GetComponent<TransformComponent>();
	if (m_pMesh)
	{
		if (Transform)
		{
			XMVectorScale(XMLoadFloat3(&m_pMesh->m_xmOOBB.Extents), Transform->GetSize().x);
		}
		m_pMesh->m_xmOOBB.Transform(m_xmOOBB, XMLoadFloat4x4(&Transform->GetWorldMatrix()));
		XMStoreFloat4(&m_xmOOBB.Orientation, XMQuaternionNormalize(XMLoadFloat4(&m_xmOOBB.Orientation)));
	}
}

bool GameObject::IsVisible(Camera* pCamera)
{
	//OnPrepareRender();
	if (!pCamera) return false; 

	auto Transform = GetComponent<TransformComponent>();

	BoundingOrientedBox worldOOBB = m_pMesh->GetBoundingBox();
	worldOOBB.Transform(worldOOBB, XMLoadFloat4x4(&Transform->GetWorldMatrix()));

	XMVECTOR orientationQuat = XMLoadFloat4(&worldOOBB.Orientation);
	orientationQuat = XMQuaternionNormalize(orientationQuat);
	XMStoreFloat4(&worldOOBB.Orientation, orientationQuat);

	return pCamera->IsInFrustum(worldOOBB);
}


void GameObject::Update(float DeltaTime)
{
	for (const std::shared_ptr<Component>& component : _components)
	{
		component->Update(DeltaTime);
	}

	//if (nullptr != m_pMaterial)
	//{
	//	// 이 셰이더를 쓰는 객체가 GLB 모델이라고 가정
	//	if (typeid(CObjectsShader) == typeid(*(m_pMaterial->_Shader)))
	//	{
	//		// Z축을 뒤집는 변환 행렬 생성
	//		XMMATRIX zFlipMatrix = XMMatrixScaling(1.0f, 1.0f, -1.0f);

	//		// 기존 월드 행렬 앞에 곱해서 최종 월드 행렬을 만듦
	//		worldMatrix = zFlipMatrix * worldMatrix;
	//		XMStoreFloat4x4(&_4x4World, worldMatrix);
	//	}
	//}
}