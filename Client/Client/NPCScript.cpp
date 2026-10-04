#include "stdafx.h"
#include "NPCScript.h"
#include "ServerClock.h"
#include "DebugDrawManager.h"
#include "ReplicationSystem.h"
#include "AnimationComponent.h"
#include "GameFramework.h"
#include "GameObject.h"
#include "TransformComponent.h"
#include "MonsterHPComponent.h"
#include "ObjectManager.h"
#include "ResourceManager.h"
#include "SoundManager.h"

void NPCScript::set_position(const XMFLOAT3& position)
{
	if (transform()) {
		transform()->set_local_position(position);
	}
}

void NPCScript::handle_animation_branching()
{
	auto anim = game_object()->get_component<AnimationComponent>();
	if (!anim) return;

	using namespace common::packet;

	// 엘리베이터와 다이나믹 박스는 애니메이션이 없으므로 분기 처리 생략
	if (_npcType == NPCType::Elevator || _npcType == NPCType::DynamicBox) return;

	// 1. 사망/피격 최우선 처리
	/*if (_state == EntityState::DEAD) {
		anim->play("Die", false);
		return;
	}
	if (_state == EntityState::HITTED) {
		anim->play("Hit", false);
		return;
	}*/

	// 1. 사망 처리
	if (_state == EntityState::DEAD) {
		anim->play("Death", false);
		return;
	}

	// 피격 처리 (피격 모션이 등록된 몬스터만, 없으면 기존처럼 아래 분기로 진행)
	if (_state == EntityState::HITTED && anim->has_animation("Hit")) {
		anim->play("Hit", false);
		return;
	}

	// 2. 액션(공격/스킬) 상태 분기
	if (_state == EntityState::ACTION) 
	{
		// [고도화] 보스 액션 ID에 따른 세부 애니메이션 분기
		if (_npcType == NPCType::Tainer) {
			using namespace common::packet;
			switch (_actionId) {
			case ActionID::Tainer::GrabCharge: 
				anim->play("walk", true, 2.0f); 
				break; // 돌진 (빠른 이동)
			case ActionID::Tainer::GrabCarry:  
				anim->play("attack", true, 1.5f); 
				break; // 난타 (연타 모션)
			case ActionID::Tainer::GrabSlam:   
				anim->play("attack", false, 0.8f); 
				break; // 슬램 (강한 공격)
			case ActionID::Tainer::Roar:       
				anim->play("Idle", false); 
				break;
			default: 
				anim->play("Attack", false); 
				break;
			}
		}
		else {
			anim->play("Attack", false);
		}
		return;
	}

	// 3. 이동 상태 분기 (속도에 따라 Walk/Run 결정)
	if (_state == EntityState::MOVE) {
		float speed = common::Length(_serverVel);
		/*if (speed > 5.0f) anim->play("Run");
		else anim->play("Walk");*/
		anim->play("Walk");
	}
	else {
		// 4. 대기 상태
		anim->play("Idle");
	}
}

const XMFLOAT3& NPCScript::position() const
{
	static XMFLOAT3 dummy = { 0, 0, 0 };
	return transform() ? transform()->local_position() : dummy;
}

NPCScript::NPCScript() : ScriptComponent("NPCScript")
{

}


NPCScript::~NPCScript()
{
	
}

void NPCScript::init_visual()
{
	auto NPC = game_object();
	auto animation_component = NPC->get_component<AnimationComponent>();
	auto render_comp = NPC->get_component<RenderComponent>();

	// [사운드] 몬스터/보스 공격음 로드 (3D 사운드)
	SoundManager::instance()->load_sound("MonsterAttack", "Resource/Sound/MonsterAttack.mp3", true);
	SoundManager::instance()->load_sound("BossDamage",   "Resource/Sound/BossDamage.mp3", false);
	SoundManager::instance()->load_sound("MagicGuardDamage", "Resource/Sound/MagicGuardDamage.wav", false);
	SoundManager::instance()->load_sound("MonsterDie",   "Resource/Sound/MonsterDie.mp3", false);

	// 보스 특수 액션 사운드 로드
	SoundManager::instance()->load_sound("BossCharge", "Resource/Sound/BossCharge.wav", true);
	SoundManager::instance()->load_sound("BossGrab",   "Resource/Sound/BossGrab.wav", true);
	SoundManager::instance()->load_sound("BossLanding",  "Resource/Sound/BossLanding.mp3", true);
	SoundManager::instance()->load_sound("BossRoar",   "Resource/Sound/BossRoar.wav", true);

	if (_npcType == common::packet::NPCType::Elevator) {
		// 엘리베이터 모델 설정 (실제 경로 적용)
		auto baseMesh = ResourceManager::instance()->load_mesh("Resource/Elevator/Elevator.gltf");
		render_comp->set_mesh(baseMesh);
		

		std::string material_name = "elevator_material_" + std::to_string(id());
		ResourceManager::instance()->create_material(material_name);
		ResourceManager::instance()->set_shader_for_material(material_name, "gltf");
		render_comp->set_pso_name("gltf");
		return;
	}

	if (_npcType == common::packet::NPCType::DynamicBox) {
		// 다이나믹 물리 박스 모델 설정
		auto baseMesh = ResourceManager::instance()->load_mesh("Resource/LeverAndPosition/Meshes/Cube_5E5A4B61.gltf", false);
		render_comp->set_mesh(baseMesh);

		std::string material_name = "dynamicbox_material_" + std::to_string(id());
		ResourceManager::instance()->create_material(material_name);
		ResourceManager::instance()->set_shader_for_material(material_name, "gltf");
		render_comp->set_pso_name("gltf");
		return;
	}

	if (_npcType == common::packet::NPCType::QuestNPC) {
		// QuestNPC 비주얼은 QuestNPCScript::awake()에서 개별적으로 로드하므로 여기서 스킵
		return;
	}

	if (_npcType == common::packet::NPCType::MagicGuard) {
		auto baseMesh = ResourceManager::instance()->load_mesh("Resource/Character/SK_MagicConstruct/SK_MagicConstruct.gltf", true);

		dynamic_pointer_cast<ReadGLTFMesh>(baseMesh)->load_animation_only("Resource/Character/SK_MagicConstruct/A_MagicConstruct_Idle01.gltf", "idle");
		dynamic_pointer_cast<ReadGLTFMesh>(baseMesh)->load_animation_only("Resource/Character/SK_MagicConstruct/A_MagicConstruct_Walk_Forward.gltf", "walk");
		dynamic_pointer_cast<ReadGLTFMesh>(baseMesh)->load_animation_only("Resource/Character/SK_MagicConstruct/A_MagicConstruct_Combat_Unarmed_Attack.gltf", "attack");
		dynamic_pointer_cast<ReadGLTFMesh>(baseMesh)->load_animation_only("Resource/Character/SK_MagicConstruct/A_MagicConstruct_Death.gltf", "die");

		render_comp->set_mesh(baseMesh);
		animation_component->add_animation("Idle", baseMesh, "idle");
		animation_component->add_animation("Walk", baseMesh, "walk");
		animation_component->add_animation("Attack", baseMesh, "attack");
		animation_component->add_animation("Death", baseMesh, "die");

		// [수정] 서버에서 애니메이션 패킷이 오기 전까지 초기 상태가 없으면 뼈(Bone)가 초기화되지 않아 부서진 것처럼 보이므로 기본 재생
		animation_component->play("Idle");

		std::string material_name = "npc_material_" + std::to_string(id());
		ResourceManager::instance()->create_material(material_name);
		ResourceManager::instance()->set_shader_for_material(material_name, "skinned");
		render_comp->set_pso_name("skinned");

		transform()->set_local_scale({ 1.5f,1.5f,1.5f });

		return;
	}

	{
		// 기본 Brute 모델 설정
		auto baseMesh = ResourceManager::instance()->load_mesh("Resource/Character/DragonBrute/SK_DragonBrute.gltf", true);

		dynamic_pointer_cast<ReadGLTFMesh>(baseMesh)->load_animation_only("Resource/Character/DragonBrute/animation/A_DragonBrute_Idle.gltf", "idle");
		dynamic_pointer_cast<ReadGLTFMesh>(baseMesh)->load_animation_only("Resource/Character/DragonBrute/animation/A_DragonBrute_Walk.gltf", "walk");
		dynamic_pointer_cast<ReadGLTFMesh>(baseMesh)->load_animation_only("Resource/Character/DragonBrute/animation/A_DragonBrute_Attack.gltf", "attack");
		dynamic_pointer_cast<ReadGLTFMesh>(baseMesh)->load_animation_only("Resource/Character/DragonBrute/animation/A_DragonBrute_Death.gltf", "die");
		dynamic_pointer_cast<ReadGLTFMesh>(baseMesh)->load_animation_only("Resource/Character/DragonBrute/animation/A_DragonBrute_Hit.gltf", "hit");

		render_comp->set_mesh(baseMesh);
		animation_component->add_animation("Idle", baseMesh, "idle");
		animation_component->add_animation("Walk", baseMesh, "walk");
		animation_component->add_animation("Attack", baseMesh, "attack");
		animation_component->add_animation("Death", baseMesh, "die");
		animation_component->add_animation("Hit", baseMesh, "hit");

		// [수정] 서버에서 애니메이션 패킷이 오기 전까지 초기 상태가 없으면 뼈(Bone)가 초기화되지 않아 부서진 것처럼 보이므로 기본 재생
		animation_component->play("Idle");

		std::string material_name = "npc_material_" + std::to_string(id());
		ResourceManager::instance()->create_material(material_name);
		ResourceManager::instance()->set_shader_for_material(material_name, "skinned");
		render_comp->set_pso_name("skinned");
	}
}

void NPCScript::awake()
{
	_serverPos = transform()->local_position();
	_serverRot = transform()->local_rotation();
	_serverVel = { 0, 0, 0 };
	_isFirstUpdate = true;

	// [수정] awake에서 등록하지 않고 set_id 호출 시점에 명시적으로 등록하도록 변경
}

void NPCScript::on_destroy()
{
	auto rs = GameFramework::instance()->get_replication_system();
	if (rs) rs->unregister_entity(this->id());
}

void NPCScript::on_server_update(const common::packet::SC_PACKET_NPC_MOVE& npc_move_packet)
{
	// 단일 이동 패킷도 배치와 같은 스냅샷 경로로 처리
	NetSnapshot snapshot;
	snapshot.pos = npc_move_packet._position;
	snapshot.vel = npc_move_packet._velocity;
	snapshot.rot = npc_move_packet._rotation;
	snapshot.state = npc_move_packet._state;
	snapshot.timestamp = npc_move_packet._time_stamp;
	snapshot.action_id = npc_move_packet._action_id;
	snapshot.grabbed_by_id = -1; // 단일 이동 패킷엔 그랩 정보가 없음
	snapshot.grab_slot = -1;
	snapshot.hp = npc_move_packet._hp;
	on_receive_snapshot(snapshot);
}

void NPCScript::initialize_from_server(const common::packet::SC_PACKET_NPC_SPAWN& spawnPkt)
{
	_serverPos = spawnPkt._position;
	_serverVel = { 0, 0, 0 };
	_serverRot = spawnPkt._rotation; // [수정] 서버에서 받은 회전값 사용

	// 생성(또는 AOI 재진입) 시 좌표가 우선: 쌓여 있던 표본을 버리고 생성 위치에서 시작
	_positionBuffer.clear();
	_pendingStates.clear();
	_motions.clear();
	{
		// 지금 렌더 시각에 표본을 둔다 (추정 서버 시각에 두면 그보다 이른 서버 시각으로 이어 오는 이동 패킷이 버려짐)
		SnapshotBuffer::Sample sample;
		sample.time = ServerClock::instance()->is_synced() ? render_time() : 0.0;
		sample.pos = _serverPos;
		sample.rot = _serverRot;
		_positionBuffer.push(sample);
	}
	_isFirstUpdate = false;

	_state = spawnPkt._state;
	_actionId = spawnPkt._action_id;
	_hp = spawnPkt._hp;
	set_id(spawnPkt._npc_id);
	_npcType = spawnPkt._npc_type;

	if (transform()) {
		transform()->set_local_position(_serverPos);
		transform()->set_local_rotation(_serverRot);
	}

	auto hp_component = game_object()->get_component<MonsterHPComponent>();
	if (hp_component) {
		hp_component->set_max_hp(spawnPkt._max_hp);
		hp_component->set_current_hp(spawnPkt._hp);
	}

	// --- 1. 서버에서 받은 회전값(rot)에 Y축 180도 추가 회전 적용 ---
	XMVECTOR qServer = XMLoadFloat4((XMFLOAT4*)&_serverRot);
	XMVECTOR qRotate180 = XMQuaternionRotationRollPitchYaw(0, XM_PI, 0); // Y축 180도(PI) 회전
	XMVECTOR qFinal = XMQuaternionMultiply(qServer, qRotate180);         // 회전 결합

	// 보정된 회전값을 _serverRot에 저장
	XMStoreFloat4(&_serverRot, qFinal);

	if (transform()) {
		transform()->set_local_position(spawnPkt._position);
		// [추가] 초기 회전값도 안전하게 설정
		XMVECTOR qRotate180 = XMQuaternionRotationRollPitchYaw(0, XM_PI, 0);
		XMStoreFloat4(&_serverRot, qRotate180);
		transform()->set_local_rotation(_serverRot);
	}

	_isFirstUpdate = false; // 이제 업데이트 가능 상태로 전환
    
    init_visual();
}


void NPCScript::set_hp(int hp)
{
	int prevHp = _hp;
	_hp = hp;
	auto hp_component = game_object()->get_component<MonsterHPComponent>();
	if (hp_component) {
		hp_component.get()->set_current_hp(hp);
	}

	// [사운드] 보스가 피격 당할 때 BossDamage 사운드 재생
	if (hp < prevHp && _npcType == common::packet::NPCType::Tainer) {
		if (transform()) {
			SoundManager::instance()->play_3d("BossDamage", transform()->get_world_position(), SoundType::SFX, 1.0f, false);
		}
	}
	
	// [사운드] 매직가드 피격 사운드 재생
	if (hp < prevHp && _npcType == common::packet::NPCType::MagicGuard) {
		if (transform()) {
			SoundManager::instance()->play_3d("MagicGuardDamage", transform()->get_world_position(), SoundType::SFX, 1.0f, false);
		}
	}
}

void NPCScript::update(float deltaTime)
{
	// 0. 잡기 상태일 때 본 부착 처리 (NPC)
	if (_grabbedById != -1) {
		auto ownerObj = ObjectManager::instance()->find_npc(_grabbedById);
		if (ownerObj) {
			auto bossAnim = ownerObj->get_component<AnimationComponent>();
			auto bossRender = ownerObj->get_component<RenderComponent>();
			if (bossAnim && bossRender) {
				auto bossMesh = std::dynamic_pointer_cast<ReadGLTFMesh>(bossRender->mesh());
				if (bossMesh) {
					// [수정] 대소문자 구분: hand_L, hand_R
					std::string boneName = (_grabSlot == 0) ? "hand_L" : "hand_R";
					XMFLOAT4X4 boneSocketTransform = bossMesh->get_socket_transform(boneName);
					XMFLOAT4X4 bossWorldMatrix = ownerObj->transform()->world_matrix();

					XMMATRIX matBone = XMLoadFloat4x4(&boneSocketTransform);
					XMMATRIX matBoss = XMLoadFloat4x4(&bossWorldMatrix);
					XMMATRIX matFinal = matBone * matBoss;

					// [핵심] 보스의 스케일 제거 및 위치/회전만 추출
					XMVECTOR scale, rot, pos;
					XMMatrixDecompose(&scale, &rot, &pos, matFinal);

					// NPC 스케일 1.0 유지 (혹은 자신의 원래 스케일 기반 재조합)
					XMMATRIX matNpc = XMMatrixRotationQuaternion(rot) * XMMatrixTranslationFromVector(pos);

					XMFLOAT4X4 finalWorld;
					XMStoreFloat4x4(&finalWorld, matNpc);
					transform()->set_world_matrix(finalWorld);

					_serverPos = transform()->local_position();
					return;
				}
			}
		}
	}

	// 개별 NPC의 업데이트는 매우 짧으므로,
	// 특정 임계치를 넘는 경우만 확인하거나 누적해서 보는 것이 좋습니다.
	// 여기서는 0.1ms(100us)를 넘는 경우만 체크합니다.
	auto start = std::chrono::high_resolution_clock::now();

	if (_isFirstUpdate || !transform()) return;

	// 서버 시각 기준 보간 (NetMotionSync_Design_KR.md 5.2): 렌더 시각 = 추정 서버 시각 - 보간 지연
	if (!_positionBuffer.empty())
	{
		const double renderTime = render_time();
		const SnapshotBuffer::Result interp = _positionBuffer.sample(renderTime);
		XMFLOAT3 pos = interp.pos;
		apply_motion(renderTime, pos); // 넉백 중이면 수평 위치는 곡선 (서버와 같은 곡선·시각)
		transform()->set_local_position(pos);
		transform()->set_local_rotation(interp.rot);
		_serverPos = pos;
		_serverVel = interp.vel; // TainerScript 등이 이동 애니메이션 선택에 사용
		_serverRot = interp.rot;
		ServerClock::instance()->count_interp_mode(ServerClock::InterpTarget::Npc, static_cast<int>(interp.mode));

		// [디버그] 서버가 마지막으로 보낸 위치 (F6 창에서 켬)
		if (ServerClock::instance()->show_server_ghost())
		{
			const auto& latest = _positionBuffer.latest();
			DebugDrawManager::instance()->AddDebugShape(common::packet::DebugShapeType::BOX,
				{ latest.pos.x, latest.pos.y + 0.9f, latest.pos.z }, latest.rot, { 0.3f, 0.9f, 0.3f }, deltaTime * 1.5f);
		}
	}

	/*auto end = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

	if (duration > 100) {
		CLOG("[Profiling] NPC Update Overload (ID: " << _id << "): " << duration << "us");
	}*/

	handle_animation_branching();
}

void NPCScript::late_update(float deltaTime)
{
	// HP 바 업데이트 등 필요한 로직 수행
}

void NPCScript::set_id(int64_t npc_id)
{
	_id = npc_id;
}

// --- INetSync 인터페이스 구현 ---
void NPCScript::on_receive_snapshot(const NetSnapshot& snapshot)
{
	// 위치는 보간 버퍼에, 상태는 서버 시각에 적용하도록 대기열에 쌓음 (한 프레임에 여러 개가 와도 모두 보관)
	const double time = ServerClock::instance()->unwrap_server_time(snapshot.timestamp);

	SnapshotBuffer::Sample sample;
	sample.time = time;
	sample.pos = snapshot.pos;
	sample.vel = snapshot.vel;
	sample.rot = to_visual_rotation(snapshot.rot);
	_positionBuffer.push(sample);
	_pendingStates.push_back({ time, snapshot });

	// 첫 스냅샷은 보간 없이 바로 적용
	if (_isFirstUpdate)
	{
		_serverPos = sample.pos;
		_serverRot = sample.rot;
		if (transform()) {
			transform()->set_local_position(_serverPos);
			transform()->set_local_rotation(_serverRot);
		}
		while (!_pendingStates.empty())
		{
			apply_state(_pendingStates.front().snapshot);
			_pendingStates.pop_front();
		}
		_isFirstUpdate = false;
	}
}

void NPCScript::apply_snapshot()
{
	auto owner = game_object();
	if (!owner || !owner->is_enable() || owner->is_destroyed())
	{
		return;
	}

	// 상태·액션·잡기·HP는 그 스냅샷의 서버 시각이 렌더 시각에 닿았을 때 적용 (모션 전환과 위치를 같은 시점에 맞춤)
	const double now = render_time();
	while (!_pendingStates.empty() && _pendingStates.front().time <= now)
	{
		apply_state(_pendingStates.front().snapshot);
		_pendingStates.pop_front();
	}
}

void NPCScript::apply_state(const NetSnapshot& snapshot)
{
	common::packet::EntityState prevState = _state;

	_state = snapshot.state;

	// 공격 시작 시 사운드 재생
	if (prevState != common::packet::EntityState::ACTION && _state == common::packet::EntityState::ACTION) {
		if (transform()) {
			SoundManager::instance()->play_3d("MonsterAttack", transform()->get_world_position(), SoundType::SFX, 1.0f, false);
		}
	}

	// 사망 시 사운드 재생
	if (prevState != common::packet::EntityState::DEAD && _state == common::packet::EntityState::DEAD) {
		if (transform()) {
			SoundManager::instance()->play_3d("MonsterDie", transform()->get_world_position(), SoundType::SFX, 1.0f, false);
			if (_npcType == common::packet::NPCType::Tainer) {
				SoundManager::instance()->stop("BossCharge");
				SoundManager::instance()->stop("BossGrab");
				SoundManager::instance()->stop("BossLanding");
				SoundManager::instance()->stop("BossRoar");
			}
		}
	}

	if (_actionId != snapshot.action_id) {
		_actionId = snapshot.action_id; // NetSnapshot에 action_id가 포함되어 있어야 함
		
		// 보스 액션별 사운드 구간 재생 (원하는 시작/종료 시간 문자열로 지정)
		if (_npcType == common::packet::NPCType::Tainer) {
			if (_actionId == common::packet::ActionID::Tainer::Charge || _actionId == common::packet::ActionID::Tainer::GrabCharge) {
				SoundManager::instance()->play_3d_section("BossCharge", transform()->get_world_position(), "00:03:00", "00:04:00");
			}
			else if (_actionId == common::packet::ActionID::Tainer::Grab) {
				SoundManager::instance()->play_3d_section("BossGrab", transform()->get_world_position(), "00:00:00", "00:00:500");
			}
			else if (_actionId == common::packet::ActionID::Tainer::Slam || _actionId == common::packet::ActionID::Tainer::GrabSlam) {
				SoundManager::instance()->play_3d("BossLanding", transform()->get_world_position(), SoundType::SFX, 1.5f, false);
			}
			else if (_actionId == common::packet::ActionID::Tainer::Roar) {
				SoundManager::instance()->play_3d_section("BossRoar", transform()->get_world_position(), "00:00:00", "00:01:00", SoundType::SFX, 1.5f);
			}
		}
	}

	_grabbedById = snapshot.grabbed_by_id; // [추가]
	_grabSlot = snapshot.grab_slot;         // [추가]
	set_hp(snapshot.hp);                   // [추가] HP 동기화
}

void NPCScript::on_motion_start(const common::packet::SC_PACKET_MOTION_START& packet)
{
	ActiveMotion motion;
	motion.id = packet._motion_id;
	motion.curve = packet._curve;
	motion.start_time = packet._start_time;
	motion.end_time = packet._start_time + (packet._hold + packet._duration) * 1000.0;
	motion.start_pos = packet._start_pos;
	motion.end_pos = packet._end_pos;
	motion.hold = packet._hold;
	motion.duration = packet._duration;

	// 새 모션이 시작되면 앞선 모션은 그 시각에 끝남 (넉백 중 다시 맞은 경우)
	for (auto& prev : _motions)
		prev.end_time = std::min(prev.end_time, motion.start_time);
	_motions.push_back(motion);
}

void NPCScript::on_motion_end(const common::packet::SC_PACKET_MOTION_END& packet)
{
	for (auto& motion : _motions)
		if (motion.id == packet._motion_id)
			motion.end_time = std::min(motion.end_time, packet._end_time);
}

bool NPCScript::apply_motion(double render_time, XMFLOAT3& pos)
{
	// 끝난 지 오래된 모션 정리 (보간 버퍼 보관 시간과 같은 1초)
	while (!_motions.empty() && _motions.front().end_time < render_time - SnapshotBuffer::kKeepMs)
		_motions.pop_front();

	for (const auto& motion : _motions)
	{
		if (render_time < motion.start_time || render_time >= motion.end_time) continue;

		const float elapsed = static_cast<float>((render_time - motion.start_time) / 1000.0);
		const float t = common::motion::Progress(motion.curve, motion.hold, motion.duration, elapsed);
		pos.x = motion.start_pos.x + (motion.end_pos.x - motion.start_pos.x) * t;
		pos.z = motion.start_pos.z + (motion.end_pos.z - motion.start_pos.z) * t;
		return true;
	}
	return false;
}

double NPCScript::render_time() const
{
	auto clock = ServerClock::instance();
	// 동기화 전에는 서버 시각을 모르므로 최신 표본을 그대로 보여줌
	if (!clock->is_synced()) return _positionBuffer.empty() ? 0.0 : _positionBuffer.latest().time;
	return clock->arrival_now_ms() - ServerClock::kNpcInterpDelayMs;
}

XMFLOAT4 NPCScript::to_visual_rotation(const common::Quat& server_rot) const
{
	XMFLOAT4 rot;
	if (_npcType == common::packet::NPCType::Elevator)
	{
		XMStoreFloat4(&rot, XMLoadFloat4(reinterpret_cast<const XMFLOAT4*>(&server_rot)));
		return rot;
	}
	// 서버 회전에 Y축 180도 추가 (모델 정면 방향 보정)
	XMVECTOR qServer = XMLoadFloat4(reinterpret_cast<const XMFLOAT4*>(&server_rot));
	XMVECTOR qRotate180 = XMQuaternionRotationRollPitchYaw(0, XM_PI, 0);
	XMStoreFloat4(&rot, XMQuaternionMultiply(qServer, qRotate180));
	return rot;
}