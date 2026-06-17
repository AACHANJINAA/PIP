import sys
import re

with open('c:\\Github\\PIP\\Client\\Client\\NetworkManager.cpp', 'r', encoding='utf-8') as f:
    code = f.read()

player_attack_pattern = r'void NetworkManager::HANDLE_S2C_PLAYER_ATTACK\(common::packet::PacketStream& stream\).*?\}\s*\}'
def replace_player_attack(match):
    return '''void NetworkManager::HANDLE_S2C_PLAYER_ATTACK(common::packet::PacketStream& stream)
{
	common::packet::SC_PACKET_PLAYER_ATTACK attack_header;
	stream >> attack_header;
	CLOG("[S->C] Received PLAYER_ATTACK. Attacker: " << attack_header._attacker_id << " HitCount: " <<
		(int)attack_header._hit_count);

	bool isSkill = false;
	if (attack_header._attacker_id == _my_session_id)
	{
		auto player = ObjectManager::instance()->find_by_name("MainPlayer");
		if (player) {
			auto player_logic = player->get_component<MainPlayerScript>();
			if (player_logic) isSkill = player_logic->is_skilling();
		}
	}

	if (attack_header._hit_count > 0 && attack_header._attacker_id == _my_session_id)
	{
		TimerManager::instance()->SetHitStop(0.12f, 0.05f); // 0.12초 동안 5% 속도
		
		if (isSkill) {
			auto mainCam = CameraComponent::get_main();
			if (mainCam && mainCam->game_object()) {
				auto freeCamScript = mainCam->game_object()->get_component<FreeCameraScript>();
				if (freeCamScript) {
					freeCamScript->add_trauma(0.5f);
				}
			}
		}
	}

	for (uint8_t i = 0; i < attack_header._hit_count; ++i)
	{
		common::packet::PlayerHitInfo hit_info;
		stream >> hit_info;
		
		DamageTextManager::instance()->add_damage_text(
			DirectX::XMFLOAT3(hit_info._target_position.x, hit_info._target_position.y, hit_info._target_position.z),
			(float)hit_info._damage,
			isSkill
		);
		SoundManager::instance()->play_3d("HitSound", DirectX::XMFLOAT3(hit_info._target_position.x, hit_info._target_position.y, hit_info._target_position.z));

		if (_my_session_id == hit_info._target_id)
		{
			auto player = ObjectManager::instance()->find_by_name("MainPlayer");
			if (player)
			{
				auto player_logic = player->get_component<MainPlayerScript>();
				if (player_logic && hit_info._target_id == player_logic->id())
				{
					CLOG("[Network] Hit Packet Received! Current Logic HP: " << player_logic->hp() 
						<< " -> New HP: " << hit_info._target_current_hp);
					player_logic->set_hp(hit_info._target_current_hp);
					player_logic->set_position(hit_info._target_position);

					continue;
				}
			}
		}
	}
}'''

code = re.sub(player_attack_pattern, replace_player_attack, code, flags=re.DOTALL)

npc_attack_pattern = r'void NetworkManager::HANDLE_S2C_NPC_ATTACK\(common::packet::PacketStream& stream\).*?\}\s*\}'
def replace_npc_attack(match):
    return '''void NetworkManager::HANDLE_S2C_NPC_ATTACK(common::packet::PacketStream& stream)
{
	common::packet::SC_PACKET_NPC_ATTACK attack_header;
	stream >> attack_header;

	bool isSkill = false;
	if (attack_header._attacker_id == _my_session_id)
	{
		auto player = ObjectManager::instance()->find_by_name("MainPlayer");
		if (player) {
			auto player_logic = player->get_component<MainPlayerScript>();
			if (player_logic) isSkill = player_logic->is_skilling();
		}
	}

	if (attack_header._hit_count > 0 && attack_header._attacker_id == _my_session_id)
	{
		TimerManager::instance()->SetHitStop(0.12f, 0.05f); // 0.12초 간 5% 속도
		
		if (isSkill) {
			auto mainCam = CameraComponent::get_main();
			if (mainCam && mainCam->game_object()) {
				auto freeCamScript = mainCam->game_object()->get_component<FreeCameraScript>();
				if (freeCamScript) {
					freeCamScript->add_trauma(0.5f);
				}
			}
		}
	}

	for (uint8_t i = 0; i < attack_header._hit_count; ++i)
	{
		common::packet::NPCHitInfo hit_info;
		stream >> hit_info;

		auto enemy_layer = LayerManager::instance()->get_layer_value("Enemy");
		auto npcs = ObjectManager::instance()->find_by_layer(enemy_layer);
		auto it = std::ranges::find_if(npcs, 
			[&](const std::shared_ptr<GameObject>& npc)
			{
				auto npc_script = npc->get_component<NPCScript>();
				return npc_script && hit_info._target_id == npc_script->id();
			});
			
		if (it != npcs.end())
		{
			(*it)->get_component<NPCScript>()->set_hp(hit_info._target_current_hp);
			auto npc_pos = (*it)->transform()->position();
			
			DamageTextManager::instance()->add_damage_text(
				DirectX::XMFLOAT3(npc_pos.x, npc_pos.y, npc_pos.z),
				(float)hit_info._damage,
				isSkill
			);
			SoundManager::instance()->play_3d("HitSound", DirectX::XMFLOAT3(npc_pos.x, npc_pos.y, npc_pos.z));
		}
	}
}'''

code = re.sub(npc_attack_pattern, replace_npc_attack, code, flags=re.DOTALL)

with open('c:\\Github\\PIP\\Client\\Client\\NetworkManager.cpp', 'w', encoding='utf-8-sig') as f:
    f.write(code)

