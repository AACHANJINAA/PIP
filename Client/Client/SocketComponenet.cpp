#include "stdafx.h"
#include "SocketComponenet.h"
#include "ReadGLTFMesh.h"
#include "RenderComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "ObjectManager.h"
#include "ResourceManager.h"

SocketComponent::SocketComponent()
{
}

void SocketComponent::late_update(float deltaTime)
{
	if(_isFollowAnimation)
	{
		auto object = game_object();
		if (!object) return;

		auto renderComp = object->get_component<RenderComponent>();
		if (!renderComp) return;
		auto mesh = std::dynamic_pointer_cast<ReadGLTFMesh>(renderComp->mesh());
		if (!mesh) return;
		// 모든 연결된 객체들에 대해 위치 갱신
		for (auto& pair : _connectedObjects)
		{
			auto& socket_info = pair.second;
			int bone_index = mesh->get_bone_index_by_name(socket_info.bone_name);
			if (bone_index < 0) continue;

			// 소켓 오브젝트의 TransformComponent 가져오기
			auto socket_object = socket_info.Object;
			if (!socket_object) continue;

			// 최종 월드 행렬을 소켓 오브젝트에 적용
			XMFLOAT4X4 object_world_matrix = object->transform()->world_matrix(); // 플레이어 월드 행렬
			XMFLOAT4X4 socket_transform = mesh->get_socket_transform(socket_info.bone_name); // 뼈대 행렬

			XMFLOAT4X4 final_world_float4x4 = pair.second._localMatrix; // 검의 로컬 행렬

			// 검의 로컬 * 애니메이션에서 계산한 원하는 뼈대의 행렬 * 소켓을 들고있는 플레이어의 월드 행렬
			final_world_float4x4 = Matrix4x4::Multiply(final_world_float4x4, socket_transform);
			final_world_float4x4 = Matrix4x4::Multiply(final_world_float4x4, object_world_matrix);

			socket_info.Object->transform()->set_world_matrix(final_world_float4x4);
		}
	}
}

void SocketComponent::add_connecting(std::string socket_name, const std::string& bone_name, const std::shared_ptr<Mesh>& mesh, XMFLOAT3 loacl_pos, XMFLOAT3 loacl_rotation, XMFLOAT3 loacl_scale)
{
	// 추가하고자 하는 소켓 이름이 이미 존재하는지 확인
	for (const auto& pair : _connectedObjects)
	{
		if (pair.first == socket_name)
		{
			CERROR("DW Socket Error : Socket name '" << socket_name << "' already exists. Use fix_connecting to modify it.");
			return;
		}
	}

	// 새로운 소켓 연결 정보를 추가하는 함수임
	ConnectingSocketInfo info;
	info.bone_name = bone_name;
	info.Object = ObjectManager::instance()->create_game_object("SocketObject");
	auto renderComp = info.Object->add_component<RenderComponent>();
	// 메쉬 설정
	renderComp->set_mesh(mesh);
	// 로컬 변환 설정
	auto transform = info.Object->transform();
	if (transform)
	{
		transform->set_local_position(loacl_pos);
		transform->set_local_rotation(loacl_rotation.x, loacl_rotation.y, loacl_rotation.z);
		transform->set_local_scale(loacl_scale);
	}
	// 로컬 행렬 만들어서 구조체에 저장
	XMMATRIX matScale = XMMatrixScaling(loacl_scale.x, loacl_scale.y, loacl_scale.z);

	float pitchRad = XMConvertToRadians(loacl_rotation.x); // X축 회전
	float yawRad = XMConvertToRadians(loacl_rotation.y); // Y축 회전
	float rollRad = XMConvertToRadians(loacl_rotation.z); // Z축 회전
	XMMATRIX matRotation = XMMatrixRotationRollPitchYaw(pitchRad, yawRad, rollRad);

	XMMATRIX matTranslation = XMMatrixTranslation(loacl_pos.x, loacl_pos.y, loacl_pos.z);

	XMMATRIX localMatrix = matScale * matRotation * matTranslation;

	XMStoreFloat4x4(&info._localMatrix, localMatrix);

	// 구조체 벡터에 넣기
	_connectedObjects.emplace_back(socket_name, info);
}

// TODO: KJ요청 : 메쉬가 없는 오브젝트도 추가할 수 있도록 하는 add_connecting 함수 오버로드
// KJ수정 : 오브젝트 리턴
std::shared_ptr<GameObject> SocketComponent::add_connecting(const std::string& socket_name,
                                                             const std::string& bone_name, const std::string& mesh,
                                                             XMFLOAT3 local_pos, XMFLOAT3 local_rotation,
                                                             XMFLOAT3 local_scale)
{
	// 추가하고자 하는 소켓 이름이 이미 존재하는지 확인
	for (const auto& pair : _connectedObjects)
	{
		if (pair.first == socket_name)
		{
			CERROR("DW Socket Error : Socket name '" << socket_name << "' already exists. Use fix_connecting to modify it.");
			return nullptr;
		}
	}

	// 새로운 소켓 연결 정보를 추가하는 함수임
	ConnectingSocketInfo info;
	info.bone_name = bone_name;
	info.Object = ObjectManager::instance()->create_game_object("SocketObject");
	auto renderComp = info.Object->add_component<RenderComponent>();
	// 메쉬 설정
	auto socket_mesh = ResourceManager::instance()->load_mesh(mesh);
	std::string material = "Socket_Material";
	ResourceManager::instance()->create_material(material);
	ResourceManager::instance()->set_shader_for_material(material, "gltf");
	renderComp->set_pso_name("gltf");

	renderComp->set_mesh(socket_mesh);
	// 로컬 변환 설정
	auto transform = info.Object->transform();
	if (transform)
	{
		transform->set_local_position(local_pos);
		transform->set_local_rotation(local_rotation.x, local_rotation.y, local_rotation.z);
		transform->set_local_scale(local_scale);
	}
	// 로컬 행렬 만들어서 구조체에 저장
	XMMATRIX matScale = XMMatrixScaling(local_scale.x, local_scale.y, local_scale.z);

	float pitchRad = XMConvertToRadians(local_rotation.x); // X축 회전
	float yawRad = XMConvertToRadians(local_rotation.y); // Y축 회전
	float rollRad = XMConvertToRadians(local_rotation.z); // Z축 회전
	XMMATRIX matRotation = XMMatrixRotationRollPitchYaw(pitchRad, yawRad, rollRad);

	XMMATRIX matTranslation = XMMatrixTranslation(local_pos.x, local_pos.y, local_pos.z);

	XMMATRIX localMatrix = matScale * matRotation * matTranslation;

	XMStoreFloat4x4(&info._localMatrix, localMatrix);

	// 구조체 벡터에 넣기
	_connectedObjects.emplace_back(socket_name, info);
	return info.Object;
}

void SocketComponent::fix_connecting(std::string socket_name, const std::string& bone_name, const std::shared_ptr<Mesh>& mesh, 
	XMFLOAT3 loacl_pos, XMFLOAT3 loacl_rotation, XMFLOAT3 loacl_scale)
{
	// 고치 싶은 소켓을 찾아 수정하는 함수임

	for (auto& pair : _connectedObjects)
	{
		if (pair.first == socket_name)
		{
			pair.second.bone_name = bone_name;
			if (pair.second.Object)
			{
				auto transform = pair.second.Object->transform();
				if (transform)
				{
					transform->set_local_position(loacl_pos);
					transform->set_local_rotation(loacl_rotation.x, loacl_rotation.y, loacl_rotation.z);
					transform->set_local_scale(loacl_scale);
				}
			}

			XMMATRIX matScale = XMMatrixScaling(loacl_scale.x, loacl_scale.y, loacl_scale.z);
			XMMATRIX matRotation = XMMatrixRotationRollPitchYaw(
				XMConvertToRadians(loacl_rotation.x),
				XMConvertToRadians(loacl_rotation.y),
				XMConvertToRadians(loacl_rotation.z));
			XMMATRIX matTranslation = XMMatrixTranslation(loacl_pos.x, loacl_pos.y, loacl_pos.z);

			XMMATRIX localMatrix = matScale * matRotation * matTranslation;
			XMStoreFloat4x4(&pair.second._localMatrix, localMatrix);

			break;
		}
	}
}

void SocketComponent::delete_connecting(std::string socket_name)
{
	// 지우고 싶은 소켓을 찾아 삭제하는 함수임
	for (auto it = _connectedObjects.begin(); it != _connectedObjects.end(); ++it)
	{
		if (it->first == socket_name)
		{
			// 소켓 오브젝트가 존재하면 파괴
			if (it->second.Object)
			{
				Object::destroy(it->second.Object);
			}
			_connectedObjects.erase(it);
			break;
		}
	}
}

void SocketComponent::create_object(std::string socket_name, const std::string& bone_name, std::string mesh, XMFLOAT3 loacl_pos, XMFLOAT3 loacl_rotation, XMFLOAT3 loacl_scale)
{

}
