#include "stdafx.h"
#include "GameObject.h"
#include "Behavior.h"
#include "LayerManager.h"
#include "ScriptComponent.h"
#include "TransformComponent.h"
#include "AnimationComponent.h"
#include "SocketComponenet.h"
#include "ReadGLTFMesh.h"

GameObject::GameObject(const std::string& name) : Object(name), _transform{ nullptr }
{
}


void GameObject::init()
{
	_transform = add_component<TransformComponent>();
}

void GameObject::awake() const
{
	_isIterating = true; // 플래그 On
	for (const auto& component : _components)
	{
		// 컴포넌트가 Behaviour를 상속받았는지 확인
		if (auto behaviour = std::dynamic_pointer_cast<Behavior>(component))
		{
			if (behaviour->is_enabled())
			{
				behaviour->awake();
			}
		}
	}
	_isIterating = false; // 플래그 Off
}

void GameObject::start() const
{
	for (const auto& component : _components)
	{
		if (auto behaviour = std::dynamic_pointer_cast<Behavior>(component))
		{
			if (behaviour->is_enabled())
			{
				behaviour->start();
			}
		}
	}
}
void GameObject::update(float deltaTime) const
{
	_isIterating = true;
	// 자신의 모든 Behaviour 컴포넌트의 update를 호출합니다.
	for (const auto& component : _components)
	{
		if (auto behavior = std::dynamic_pointer_cast<Behavior>(component))
		{
			if (behavior->is_enabled())
			{
				behavior->update(deltaTime);
			}
		}
	}
	_isIterating = false;
}
void GameObject::fixed_update(float fixed_delta_time) const
{
	for (const auto& component : _components)
	{
		if (auto behavior = std::dynamic_pointer_cast<Behavior>(component))
		{
			if (behavior->is_enabled())
			{
				behavior->fixed_update(fixed_delta_time);
			}
		}
	}
}

void GameObject::late_update(float delta_time)
{
	for (const auto& component : _components)
	{
		if (auto behavior = std::dynamic_pointer_cast<Behavior>(component))
		{
			if (behavior->is_enabled())
			{
				behavior->late_update(delta_time);
			}
		}
	}
}

void GameObject::destroy()
{
	Object::destroy(shared_from_this());
}

void GameObject::prepare_render() const
{
	// DW수정 : 애니메이션 컴포넌트가 렌더러에서 넘겨주기 때문에 여기서 기존에 하던 것은 해주지 않아도 된다.
	
	// 만약 애니메이션 컴포넌트가 있다면, 본 팔레트 버퍼를 갱신
	//for (const auto& animation : _components)
	//{
	//	// 애니메이션 컴포넌트 가져오기
	//	if (auto animation_component = std::dynamic_pointer_cast<AnimationComponent>(animation))
	//	{
	//		std::shared_ptr<Mesh> now_mesh = nullptr;

	//		// 렌더러로부터 메쉬 가져오기
	//		for (const auto& component : _components)
	//		{
	//			if (auto render_component = std::dynamic_pointer_cast<RenderComponent>(component))
	//			{
	//				now_mesh = render_component->mesh();
	//			}
	//		}

	//		// 본 팔레트 버퍼 설정
	//		auto bone_palette_buffer = animation_component->get_bone_palette_buffer();
	//		if(bone_palette_buffer)
	//		{
	//			std::dynamic_pointer_cast<ReadGLTFMesh>(now_mesh)->set_bone_palette_buffer_from_animation_component(bone_palette_buffer.Get());
	//		}
	//	}
	//}
}

void GameObject::on_collision_enter(const std::shared_ptr<GameObject>& other)
{
	// 이 객체에 붙은 모든 스크립트의 콜백을 호출
	for (const auto& component : _components)
	{
		if (auto script = std::dynamic_pointer_cast<ScriptComponent>(component))
		{
			script->on_collision_enter(other);
		}
	}
}
void GameObject::on_collision_stay(const std::shared_ptr<GameObject>& other)
{
	// 이 객체에 붙은 모든 스크립트의 콜백을 호출
	for (const auto& component : _components)
	{
		if (auto script = std::dynamic_pointer_cast<ScriptComponent>(component))
		{
			script->on_collision_stay(other);
		}
	}
	
}
void GameObject::on_collision_exit(const std::shared_ptr<GameObject>& other)
{
	// 이 객체에 붙은 모든 스크립트의 콜백을 호출
	for (const auto& component : _components)
	{
		if (auto script = std::dynamic_pointer_cast<ScriptComponent>(component))
		{
			script->on_collision_exit(other);
		}
	}
	
}

void GameObject::set_layer(const std::string& name)
{
	_layerMask = LayerManager::instance()->get_layer_value(name);
}

bool GameObject::is_in_layer(const std::string& name) const
{
	uint32_t layerValue = LayerManager::instance()->get_layer_value(name);
	return (_layerMask & layerValue) != 0;
}

void GameObject::add_glTF_conponent_pack()
{
	// 애니메이션, 소켓 컴포넌트 추가
	// 여기에 있는 add 순서는 꼭 지켜져야 함 -> 그래서 이렇게 pack 함수로 묶어둠
	add_component<RenderComponent>();
	add_component<AnimationComponent>();
	add_component<SocketComponent>();
}

void GameObject::remove_component(const std::shared_ptr<Component>& component)
{
	if (component)
	{
		if (auto behavior = std::dynamic_pointer_cast<Behavior>(component))
		{
			// 비활성화 상태여도 파괴 시점의 정리는 필요하므로 무조건 호출합니다.
			behavior->on_destroy();
		}
		if (_transform == component)
		{
			return;
		}
		std::erase(_components, component);
	}
}

//
//GameObject::~GameObject()
//{
//}
//
//void GameObject::generate_ray_for_picking(XMVECTOR& xmvPickPosition, XMMATRIX& xmmtxView, XMVECTOR& xmvPickRayOrigin, XMVECTOR& xmvPickRayDirection)
//{
//	auto Transform = get_component<TransformComponent>();
//	if (Transform) {
//		XMMATRIX xmmtxToModel = XMMatrixInverse(NULL, XMLoadFloat4x4(&Transform->get_world_matrix()) * xmmtxView);
//
//		XMFLOAT3 xmf3CameraOrigin(0.0f, 0.0f, 0.0f);
//		xmvPickRayOrigin = XMVector3TransformCoord(XMLoadFloat3(&xmf3CameraOrigin), xmmtxToModel);
//		xmvPickRayDirection = XMVector3TransformCoord(xmvPickPosition, xmmtxToModel);
//		xmvPickRayDirection = XMVector3Normalize(xmvPickRayDirection - xmvPickRayOrigin);
//	}
//}
//
//int GameObject::pick_object_by_ray_intersection(XMVECTOR& pick_position, XMMATRIX& view_matrix, float* hit_distance)
//{
//	int nIntersected = 0;
//
//	auto Render = get_component<RenderComponent>();
//
//	if (Render->get_mesh())
//	{
//		XMVECTOR xmvPickRayOrigin, xmvPickRayDirection;
//		generate_ray_for_picking(pick_position, view_matrix, xmvPickRayOrigin, xmvPickRayDirection);
//		nIntersected = Render->get_mesh()->CheckRayIntersection(xmvPickRayOrigin, xmvPickRayDirection, hit_distance);
//	}
//	return(nIntersected);
//}
//
//bool GameObject::pick_model_obb(XMVECTOR& pick_position, XMMATRIX& view_matrix, float* hit_distance)
//{
//	auto Render = get_component<RenderComponent>();
//
//	if (Render->get_mesh())
//	{
//		XMVECTOR xmvPickRayOrigin, xmvPickRayDirection;
//		generate_ray_for_picking(pick_position, view_matrix, xmvPickRayOrigin, xmvPickRayDirection);
//		return(Render->get_mesh()->m_xmOOBB.Intersects(xmvPickRayOrigin, xmvPickRayDirection, *hit_distance));
//	}
//	return false;
//}
//
//void GameObject::update_bounding_box()
//{
//	auto Transform = get_component<TransformComponent>();
//	auto Render = get_component<RenderComponent>();
//
//	if (Render)
//	{
//		auto mesh = Render->get_mesh();
//		if (mesh)
//		{
//			if (Transform)
//			{
//				XMVectorScale(XMLoadFloat3(&Render->get_mesh()->m_xmOOBB.Extents), Transform->get_size().x);
//			}
//			Render->get_mesh()->m_xmOOBB.Transform(Render->get_mesh()->m_xmOOBB, XMLoadFloat4x4(&Transform->get_world_matrix()));
//			XMStoreFloat4(&Render->get_mesh()->m_xmOOBB.Orientation, XMQuaternionNormalize(XMLoadFloat4(&Render->get_mesh()->m_xmOOBB.Orientation)));
//		}
//	}
//}


