#include "stdafx.h"
#include "GameObject.h"
#include "Behaviour.h"
#include "TransformComponent.h"
GameObject::GameObject(const std::string& name) : Object(name), _transform{ nullptr }
{
	auto transform = add_component<TransformComponent>();
	_transform = transform.get(); // 빠른 접근을 위해 포인터 저장
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
void GameObject::update(float DeltaTime)
{
	for (const auto& component : _components)
	{
		if (auto behaviour = std::dynamic_pointer_cast<Behaviour>(component))
		{
			if (behaviour->is_enabled())
			{
				// 다음 단계에서 Behaviour/Script의 update 함수 시그니처를 수정할 예정
				behaviour->update(DeltaTime);
			}
		}
	}

	if (transform())
	{
		for (const auto& childTransform : transform()->children())
		{
			if (childTransform && childTransform->game_object())
			{
				childTransform->game_object()->update(deltaTime);
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

void GameObject::destory()
{
	Object::destroy(shared_from_this());
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


