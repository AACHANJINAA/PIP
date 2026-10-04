#include "stdafx.h"
#include "HitReactionComponent.h"
#include "GameObject.h"
#include "TransformComponent.h"

namespace
{
	bool contains(const std::string& name, const char* token)
	{
		return name.find(token) != std::string::npos;
	}

	// 점과 선분 사이 거리
	float distance_to_segment(FXMVECTOR p, FXMVECTOR a, FXMVECTOR b)
	{
		const XMVECTOR ab = XMVectorSubtract(b, a);
		const float len_sq = XMVectorGetX(XMVector3LengthSq(ab));
		float t = len_sq > 1e-8f ? XMVectorGetX(XMVector3Dot(XMVectorSubtract(p, a), ab)) / len_sq : 0.0f;
		t = std::clamp(t, 0.0f, 1.0f);
		return XMVectorGetX(XMVector3Length(XMVectorSubtract(p, XMVectorAdd(a, XMVectorScale(ab, t)))));
	}
}

bool HitReactionComponent::is_reactive_joint(const ReadGLTFMesh& mesh, int joint)
{
	// 돌려도 화면에 변화가 없는 뼈(정점 가중치가 자신·자식 어디에도 없음: IK 뼈, 일부 장식·트위스트 뼈)는 제외
	if (!mesh.joint_moves_skin(joint)) return false;

	// 루트·골반을 돌리면 몸 전체가 돌아감 (다리는 포함: 보스처럼 다리를 주로 맞는 캐릭터가 있음)
	const std::string& name = mesh.get_joint_name(joint);
	return !contains(name, "root") && !contains(name, "pelvis");
}

HitReactionComponent::BoneState& HitReactionComponent::bone_state(int joint)
{
	for (auto& bone : _bones)
		if (bone.joint == joint) return bone;
	_bones.push_back({ joint, { 0, 0, 0 }, { 0, 0, 0 } });
	return _bones.back();
}

void HitReactionComponent::react(const TriggerHit& hit, float strength)
{
	auto owner = game_object();
	auto anim = owner ? owner->get_component<AnimationComponent>() : nullptr;
	if (!anim || !anim->has_pose() || !anim->pose_mesh() || anim->bone_palette().empty()) return;

	const ReadGLTFMesh& mesh = *anim->pose_mesh();
	const auto& joints = anim->joint_model_matrices();
	if (_mesh != &mesh)
	{
		_bones.clear();
		_mesh = &mesh;
	}

	// 1. 현재 화면 자세 그대로 스키닝한 정점 (모델 공간) -> 월드
	std::vector<XMFLOAT3> positions;
	std::vector<UINT> indices;
	std::vector<SkinInfluence> influences;
	if (!mesh.get_skinned_geometry(anim->bone_palette(), positions, indices, &influences)) return;

	const XMFLOAT4X4 world4 = owner->transform()->world_matrix();
	const XMMATRIX world = XMLoadFloat4x4(&world4);
	const XMMATRIX to_model = XMMatrixInverse(nullptr, world);
	const XMVECTOR hit_point = XMLoadFloat3(&hit.point);

	std::vector<XMFLOAT3> world_positions(positions.size());
	for (size_t i = 0; i < positions.size(); ++i)
		XMStoreFloat3(&world_positions[i], XMVector3TransformCoord(XMLoadFloat3(&positions[i]), world));

	// 2. 칼이 쓸고 간 캡슐(직전 -> 현재 자세를 나눠서)에 닿은 정점
	std::vector<std::pair<size_t, float>> contacts; // 정점, 가중치
	if (hit.has_self_segment)
	{
		const float reach = hit.self_radius + kContactMargin;
		const XMVECTOR pa = XMLoadFloat3(&hit.self_prev_a), pb = XMLoadFloat3(&hit.self_prev_b);
		const XMVECTOR ca = XMLoadFloat3(&hit.self_cur_a), cb = XMLoadFloat3(&hit.self_cur_b);
		for (size_t i = 0; i < world_positions.size(); ++i)
		{
			const XMVECTOR p = XMLoadFloat3(&world_positions[i]);
			if (XMVectorGetX(XMVector3LengthSq(XMVectorSubtract(p, hit_point))) > kSearchRadius * kSearchRadius) continue;
			for (int s = 0; s <= kSweepSteps; ++s)
			{
				const float t = static_cast<float>(s) / kSweepSteps;
				if (distance_to_segment(p, XMVectorLerp(pa, ca, t), XMVectorLerp(pb, cb, t)) <= reach)
				{
					contacts.emplace_back(i, 1.0f);
					break;
				}
			}
		}
	}
	const bool blade_touched_mesh = !contacts.empty();

	// 칼이 메쉬에 직접 안 닿음 (피격 캡슐만 스침): 맞은 지점 주변 정점, 가까울수록 크게
	if (contacts.empty())
	{
		size_t nearest = 0;
		float nearest_dist = FLT_MAX;
		for (size_t i = 0; i < world_positions.size(); ++i)
		{
			const float d = XMVectorGetX(XMVector3Length(XMVectorSubtract(XMLoadFloat3(&world_positions[i]), hit_point)));
			if (d < nearest_dist) { nearest_dist = d; nearest = i; }
			if (d <= kFallbackRadius) contacts.emplace_back(i, 1.0f - d / kFallbackRadius);
		}
		if (contacts.empty() && !world_positions.empty()) contacts.emplace_back(nearest, 1.0f);
	}
	if (contacts.empty()) return;

	// 3. 닿은 정점들의 중심과 뼈별 가중치 합 (리액션 대상 뼈만)
	std::unordered_map<int, float> joint_weight;
	XMVECTOR centroid = XMVectorZero();
	float centroid_weight = 0.0f;
	for (const auto& [index, w] : contacts)
	{
		centroid = XMVectorAdd(centroid, XMVectorScale(XMLoadFloat3(&world_positions[index]), w));
		centroid_weight += w;
		const SkinInfluence& influence = influences[index];
		for (int k = 0; k < 4; ++k)
		{
			const int joint = static_cast<int>(influence.joints[k]);
			if (influence.weights[k] <= 0.001f || joint >= static_cast<int>(joints.size())) continue;
			if (!is_reactive_joint(mesh, joint)) continue;
			joint_weight[joint] += influence.weights[k] * w;
		}
	}
	if (joint_weight.empty() || centroid_weight <= 0.0f)
	{
		CLOG("[HitReaction] " << owner->name() << ": 닿은 정점 " << contacts.size() << "개에 리액션 대상 뼈 없음");
		return;
	}
	centroid = XMVectorScale(centroid, 1.0f / centroid_weight);

	float max_weight = 0.0f;
	for (const auto& [joint, w] : joint_weight) max_weight = std::max(max_weight, w);

	// 4. 뼈마다 관절을 중심으로 닿은 부위가 칼 방향으로 밀리게 회전 (축 = 관절→닿은 중심 × 칼 방향, 모델 공간)
	const XMVECTOR centroid_m = XMVector3TransformCoord(centroid, to_model);
	const XMVECTOR dir_m = XMVector3Normalize(XMVector3TransformNormal(XMLoadFloat3(&hit.direction), to_model));
	if (!(XMVectorGetX(XMVector3LengthSq(dir_m)) >= 1e-8f)) return; // 0이거나 NaN

	std::unordered_map<int, XMFLOAT3> push; // 조인트 -> 이번 타격의 회전 벡터 (같은 뼈가 여러 번 나오면 큰 쪽)
	for (const auto& [hit_joint, w] : joint_weight)
	{
		const float share = w / max_weight;
		if (share < kMinShare) continue;

		float angle = XMConvertToRadians(_settings.peak_angle_deg) * strength * share;
		int joint = hit_joint;
		for (int i = 0; i < kChainLength && joint >= 0; ++i, joint = mesh.get_joint_parent(joint), angle *= kChainFalloff)
		{
			if (!is_reactive_joint(mesh, joint)) break;

			const XMVECTOR pivot = XMVectorSet(joints[joint]._41, joints[joint]._42, joints[joint]._43, 1.0f);
			XMVECTOR axis = XMVector3Cross(XMVectorSubtract(centroid_m, pivot), dir_m);
			if (!(XMVectorGetX(XMVector3LengthSq(axis)) >= 1e-10f)) continue; // 칼 방향이 관절을 향함(꺾을 방향 없음) 또는 NaN
			axis = XMVector3Normalize(axis);

			auto it = push.find(joint);
			if (it == push.end() || XMVectorGetX(XMVector3Length(XMLoadFloat3(&it->second))) < angle)
			{
				XMFLOAT3 rotvec;
				XMStoreFloat3(&rotvec, XMVectorScale(axis, angle));
				push[joint] = rotvec;
			}
		}
	}

	// 5. 바로 꺾인 자세에서 시작해 스프링으로 돌아옴 (히트스톱 동안 꺾인 채로 보임). 연타는 더하되 최대 각도 제한
	const float max_angle = XMConvertToRadians(_settings.max_angle_deg) * strength; // 강한 공격은 최대 각도도 같이 늘림
	std::string log;
	for (const auto& [joint, rotvec] : push)
	{
		BoneState& bone = bone_state(joint);
		XMVECTOR total = XMVectorAdd(XMLoadFloat3(&bone.angle), XMLoadFloat3(&rotvec));
		const float total_angle = XMVectorGetX(XMVector3Length(total));
		if (total_angle > max_angle) total = XMVectorScale(total, max_angle / total_angle);
		XMStoreFloat3(&bone.angle, total);
		bone.velocity = { 0, 0, 0 };

		log += " " + mesh.get_joint_name(joint) + "(" + std::to_string(static_cast<int>(XMConvertToDegrees(XMVectorGetX(XMVector3Length(XMLoadFloat3(&rotvec)))))) + "도)";
	}
	CLOG("[HitReaction] " << owner->name() << ": " << (blade_touched_mesh ? "칼이 닿은" : "맞은 지점 주변") << " 정점 " << contacts.size() << "개, 뼈" << log);
}

void HitReactionComponent::update(float deltaTime)
{
	auto anim = game_object() ? game_object()->get_component<AnimationComponent>() : nullptr;
	if (!anim) return;
	if (_bones.empty())
	{
		return;
	}
	if (anim->pose_mesh() != _mesh)
	{
		// 메쉬가 바뀌면 조인트 번호가 달라지므로 버림
		_bones.clear();
		anim->set_joint_offsets({});
		return;
	}

	// 감쇠 스프링: a = -w² x - 2ζw v (작은 단계로 나눠 반암시적 오일러)
	const float w = XM_2PI * _settings.frequency;
	const float k = w * w;
	const float c = 2.0f * _settings.damping_ratio * w;
	const int steps = std::max(1, static_cast<int>(std::ceil(deltaTime / 0.005f)));
	const float h = deltaTime / steps;

	std::vector<JointRotationOffset> offsets;
	offsets.reserve(_bones.size());
	for (auto it = _bones.begin(); it != _bones.end();)
	{
		XMVECTOR x = XMLoadFloat3(&it->angle);
		XMVECTOR v = XMLoadFloat3(&it->velocity);
		for (int s = 0; s < steps; ++s)
		{
			const XMVECTOR a = XMVectorSubtract(XMVectorScale(x, -k), XMVectorScale(v, c));
			v = XMVectorAdd(v, XMVectorScale(a, h));
			x = XMVectorAdd(x, XMVectorScale(v, h));
		}
		XMStoreFloat3(&it->angle, x);
		XMStoreFloat3(&it->velocity, v);

		// [방어] NaN이 생기면 그 뼈 리액션 버림 (뼈 행렬이 깨지면 메쉬가 사라지거나 튐)
		if (XMVector3IsNaN(x) || XMVector3IsNaN(v))
		{
			CERROR("[HitReaction] " << game_object()->name() << ": 뼈 " << it->joint << " 회전에 NaN, 리액션 버림");
			it = _bones.erase(it);
			continue;
		}

		// 충분히 돌아왔으면 제거 (0.05도, 초당 0.5도 미만)
		if (XMVectorGetX(XMVector3LengthSq(x)) < 1e-6f && XMVectorGetX(XMVector3LengthSq(v)) < 1e-4f)
		{
			it = _bones.erase(it);
			continue;
		}
		offsets.push_back({ it->joint, it->angle });
		++it;
	}
	anim->set_joint_offsets(std::move(offsets));
}
