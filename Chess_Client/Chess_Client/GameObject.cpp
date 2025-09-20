#include "stdafx.h"
#include "GameObject.h"

GameObject::GameObject()
{
	add_component<TransformComponent>(this);
	add_component<RenderComponent>(this);
}
GameObject::~GameObject()
{
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

	auto Render = get_component<RenderComponent>();

	if (Render->get_mesh())
	{
		XMVECTOR xmvPickRayOrigin, xmvPickRayDirection;
		generate_ray_for_picking(pick_position, view_matrix, xmvPickRayOrigin, xmvPickRayDirection);
		nIntersected = Render->get_mesh()->CheckRayIntersection(xmvPickRayOrigin, xmvPickRayDirection, hit_distance);
	}
	return(nIntersected);
}

bool GameObject::pick_model_obb(XMVECTOR& pick_position, XMMATRIX& view_matrix, float* hit_distance)
{
	auto Render = get_component<RenderComponent>();

	if (Render->get_mesh())
	{
		XMVECTOR xmvPickRayOrigin, xmvPickRayDirection;
		generate_ray_for_picking(pick_position, view_matrix, xmvPickRayOrigin, xmvPickRayDirection);
		return(Render->get_mesh()->m_xmOOBB.Intersects(xmvPickRayOrigin, xmvPickRayDirection, *hit_distance));
	}
	return false;
}

void GameObject::update_bounding_box()
{
	auto Transform = get_component<TransformComponent>();
	auto Render = get_component<RenderComponent>();

	if (Render)
	{
		auto mesh = Render->get_mesh();
		if (mesh)
		{
			if (Transform)
			{
				XMVectorScale(XMLoadFloat3(&Render->get_mesh()->m_xmOOBB.Extents), Transform->get_size().x);
			}
			Render->get_mesh()->m_xmOOBB.Transform(Render->get_mesh()->m_xmOOBB, XMLoadFloat4x4(&Transform->get_world_matrix()));
			XMStoreFloat4(&Render->get_mesh()->m_xmOOBB.Orientation, XMQuaternionNormalize(XMLoadFloat4(&Render->get_mesh()->m_xmOOBB.Orientation)));
		}
	}
}


void GameObject::update(float DeltaTime)
{
	for (const std::shared_ptr<Component>& component : _components)
	{
		component->update(DeltaTime);
	}
}