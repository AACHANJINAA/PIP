#include "stdafx.h"
#include "GameObject.h"
#include "Behaviour.h"
#include "FreeCameraScript.h"
#include "LayerManager.h"
#include "TransformComponent.h"

GameObject::GameObject(const std::string& name) : Object(name), _transform{ nullptr }
{
}


void GameObject::init()
{
	_transform = add_component<TransformComponent>();
}

void GameObject::awake()
{
	for (const auto& component : _components)
	{
		// 컴포넌트가 Behaviour를 상속받았는지 확인
		if (auto behaviour = std::dynamic_pointer_cast<Behaviour>(component))
		{
			if (behaviour->is_enabled())
			{
				behaviour->awake();
			}
		}
	}
}

void GameObject::start()
{
	for (const auto& component : _components)
	{
		if (auto behaviour = std::dynamic_pointer_cast<Behaviour>(component))
		{
			if (behaviour->is_enabled())
			{
				behaviour->start();
			}
		}
	}
}
void GameObject::update(float delta_time)
{
	// [제거] TransformComponent는 더 이상 GameFramework의 업데이트 루프에 의존하지 않습니다.
	 // if (_transform) {
	 //     _transform->update();
	 // }

	 // 자신의 모든 Behaviour 컴포넌트의 update를 호출합니다.
	for (const auto& component : _components)
	{
		if (auto behaviour = std::dynamic_pointer_cast<Behaviour>(component))
		{
			if (behaviour->is_enabled())
			{
				behaviour->update(delta_time);
			}
		}
	}
}
void GameObject::fixed_update(float fixed_delta_time)
{
	for (const auto& component : _components)
	{
		if (auto behaviour = std::dynamic_pointer_cast<Behaviour>(component))
		{
			if (behaviour->is_enabled())
			{
				behaviour->fixed_update(fixed_delta_time);
			}
		}
	}
}

void GameObject::late_update(float delta_time)
{
	for (const auto& component : _components)
	{
		if (auto behaviour = std::dynamic_pointer_cast<Behaviour>(component))
		{
			if (behaviour->is_enabled())
			{
				behaviour->late_update(delta_time);
			}
		}
	}
}

void GameObject::destroy()
{
	Object::destroy(shared_from_this());
}

void GameObject::set_layer(const std::string& name)
{
	_layerMask = LayerManager::Instance()->get_layer_value(name);
}

bool GameObject::is_in_layer(const std::string& name) const
{
	uint32_t layerValue = LayerManager::Instance()->get_layer_value(name);
	return (_layerMask & layerValue) != 0;
}

void GameObject::remove_component(std::shared_ptr<Component> component)
{
	if (component)
	{
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


