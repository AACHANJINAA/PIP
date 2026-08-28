#pragma once
#include "Behavior.h"
class Mesh;
class GameObject;

struct ConnectingSocketInfo
{
	std::string bone_name;
	std::shared_ptr<GameObject> Object;
	DirectX::XMFLOAT4X4 _localMatrix;
};

class SocketComponent : public Behavior
{
public:
	SocketComponent();
	~SocketComponent() override = default;

public:
	void late_update(float deltaTime) override;

	// 연결하기 (연결할 뼈 이름, 연결할 메쉬(ex : 검), 로컬 변환) , 소켓 이름을 반환해줌
	// 메쉬를 연결하면 해당 메쉬를 가진 게임오브젝트가 새로 생성되어 오브젝트 매니저에 등록 및 소켓에 연결됨
	void add_connecting(std::string socket_name, const std::string& bone_name, const std::shared_ptr<Mesh>& mesh,
		XMFLOAT3 loacl_pos = {0.f,0.f,0.f}, XMFLOAT3 loacl_rotation = { 0.f,0.f,0.f }, XMFLOAT3 loacl_scale = { 1.f,1.f,1.f });

	::std::shared_ptr<GameObject> add_connecting(const std::string& socket_name,
	                                                           const std::string& bone_name, const std::string& mesh,
	                                                           XMFLOAT3 local_pos = {0.f, 0.f, 0.f},
	                                                           XMFLOAT3 local_rotation = {0.f, 0.f, 0.f},
	                                                           XMFLOAT3 local_scale = {1.f, 1.f, 1.f});

	// 연결된 것들 수정
	void fix_connecting(std::string socket_name, const std::string& bone_name, const std::shared_ptr<Mesh>& mesh,
		XMFLOAT3 loacl_pos = { 0.f,0.f,0.f }, XMFLOAT3 loacl_rotation = { 0.f,0.f,0.f }, XMFLOAT3 loacl_scale = { 1.f,1.f,1.f });

	// 연결된 것들 삭제
	void delete_connecting(std::string socket_name);

	void set_isFollowAnimation(bool isFollow) { _isFollowAnimation = isFollow; } // 애니메이션 따라서 움직일래 말래


private:
	// 객체 추가 함수
	void create_object(std::string socket_name, const std::string& bone_name, std::string mesh, XMFLOAT3 loacl_pos, XMFLOAT3 loacl_rotation, XMFLOAT3 loacl_scale);

private:

	// 연결된 객체들 (설정한 이름, 객체)
	// 관리해주는 것은 (설정한 소켓 이름, 연결할 뼈, 객체)
	std::vector<std::pair<std::string, ConnectingSocketInfo>> _connectedObjects;

	bool _isFollowAnimation = true;
};

