#pragma once
#include "Behavior.h"
#include "AnimationComponent.h"
#include "ScriptComponent.h"

// 부위 피격 리액션 (HitFeel_Plan_KR.md 3단계)
// 칼이 이번 프레임에 쓸고 간 구간과 NPC 실제 메쉬(현재 자세로 스키닝한 정점)를 대조해 닿은 정점을 찾고,
// 그 정점들의 뼈 가중치 비율대로 여러 뼈(+ 부모 몇 개)를 칼이 지나가는 방향으로 꺾은 뒤 스프링으로 애니메이션 자세에 돌아오게 한다.
// 몸 전체가 밀리는 넉백과 달리 맞은 부위만 밀렸다 돌아온다. 결과는 AnimationComponent의 조인트 추가 회전으로 넘긴다.
// - 히트스톱 중에는 오브젝트 시간이 멈추므로 꺾인 자세로 멈춰 있다가 풀리면서 돌아온다 (피격자 흔들림 역할)
// - 스프링은 오브젝트 시간(update의 deltaTime)으로 진행
class HitReactionComponent : public Behavior
{
public:
	using required_components = std::tuple<AnimationComponent>;

	HitReactionComponent() : Behavior("HitReactionComponent") {}

	// 캐릭터마다 다른 세기·복귀 속도 (몸 크기, 체형에 따라 같은 각도도 강하거나 약해 보임)
	struct Settings
	{
		float peak_angle_deg = 20.0f;	// 가장 많이 닿은 뼈가 꺾이는 각도
		float max_angle_deg = 35.0f;	// 연타로 쌓여도 이 이상 꺾지 않음
		float frequency = 2.5f;			// 스프링 고유 진동수 (Hz). 클수록 빨리 돌아옴
		float damping_ratio = 0.5f;		// 1보다 작으면 살짝 반대로 넘어갔다 돌아옴
	};
	void set_settings(const Settings& settings) { _settings = settings; }
	const Settings& settings() const { return _settings; }

	// 칼날 예측 적중 정보로 리액션 시작. strength: 각도와 최대 누적 각도에 곱함 (1이면 설정 그대로, 대검 스킬은 1.5)
	void react(const TriggerHit& hit, float strength = 1.0f);

	void update(float deltaTime) override;

	static constexpr int kChainLength = 3;				// 닿은 뼈 + 부모 2개
	static constexpr float kChainFalloff = 0.5f;		// 부모로 갈수록 이 비율로 약해짐
	static constexpr float kMinShare = 0.2f;			// 가장 많이 닿은 뼈 대비 이 비율 미만인 뼈는 무시
	static constexpr float kContactMargin = 0.05f;		// 칼날 반지름에 더하는 여유 (m)
	static constexpr int kSweepSteps = 6;				// 직전 → 현재 칼 자세를 나눠 검사하는 단계 수
	static constexpr float kSearchRadius = 2.5f;		// 맞은 지점에서 이 거리(m) 밖 정점은 검사 안 함
	static constexpr float kFallbackRadius = 0.3f;		// 칼이 메쉬에 직접 안 닿았을 때 맞은 지점 주변 정점 반경 (m)

private:
	struct BoneState
	{
		int joint;
		XMFLOAT3 angle;		// 모델 공간 회전 벡터 (라디안)
		XMFLOAT3 velocity;	// 각속도
	};
	Settings _settings;
	std::vector<BoneState> _bones;
	const ReadGLTFMesh* _mesh = nullptr;	// _bones의 조인트 번호를 만든 메쉬 (바뀌면 리액션 버림)

	// 리액션을 받을 뼈인지 (정점을 움직이는 뼈 중 루트·골반 제외. 다리·손가락·눈·턱·장식 포함, IK 제외)
	static bool is_reactive_joint(const ReadGLTFMesh& mesh, int joint);
	BoneState& bone_state(int joint);
};
