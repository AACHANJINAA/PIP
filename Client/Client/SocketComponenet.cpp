#include "stdafx.h"
#include "SocketComponenet.h"
#include "ReadGLTFMesh.h"
#include "RenderComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "ObjectManager.h"

SocketComponenet::SocketComponenet()
{
}

void SocketComponenet::late_update(float deltaTime)
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
		const auto& socket_info = pair.second;
		int bone_index = mesh->get_bone_index_by_name(socket_info.bone_name);
		if (bone_index < 0) continue;

		// 소켓 오브젝트의 TransformComponent 가져오기
		auto socket_object = socket_info.Object;
		if (!socket_object) continue;
		auto socket_transform_comp = socket_object->get_component<TransformComponent>();
		if (!socket_transform_comp) continue;

		// 최종 월드 행렬을 소켓 오브젝트에 적용
		XMFLOAT4X4 object_world_matrix = object->transform()->world_matrix(); // 플레이어 월드 행렬
		XMFLOAT4X4 socket_transform = mesh->get_socket_transform(socket_info.bone_name); // 뼈대 행렬
		XMFLOAT4X4 final_world_float4x4 = socket_transform_comp->world_matrix(); // 검의 로컬 행렬
		
		// 검의 로컬 * 애니메이션에서 계산한 원하는 뼈대의 행렬 * 소켓을 들고있는 플레이어의 월드 행렬
		final_world_float4x4 = Matrix4x4::Multiply(final_world_float4x4, socket_transform);
		final_world_float4x4 = Matrix4x4::Multiply(final_world_float4x4, object_world_matrix);

		socket_info.Object->transform()->set_world_matrix(final_world_float4x4);
	}
}

void SocketComponenet::add_connecting(std::string socket_name, const std::string& bone_name, const std::shared_ptr<Mesh>& mesh, XMFLOAT3 loacl_pos, XMFLOAT3 loacl_rotation, XMFLOAT3 loacl_scale)
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
	_connectedObjects.emplace_back(bone_name, info);
}

void SocketComponenet::fix_connecting(std::string socket_name, const std::string& bone_name, const std::shared_ptr<Mesh>& mesh, 
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
			break;
		}
	}
}
