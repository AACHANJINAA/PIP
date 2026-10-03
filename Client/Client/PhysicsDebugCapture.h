#pragma once

class GameObject;

namespace JPH
{
	class DebugRendererRecorder;
	class StreamOutWrapper;
}

// 클라이언트 물리 상태를 한 프레임씩 JoltViewer용 파일(client_physics_dump.bin)로 기록
// - JPH_DEBUG_RENDERER가 정의된 빌드(Debug)에서만 동작하고, 그 외에는 모든 함수가 빈 함수
// - 기록 내용: Jolt 바디 전체, QueryOnly 콜라이더(현재/직전 프레임), 부착 뼈 축, 기준 프리미티브(실제 메쉬 와이어)
class PhysicsDebugCapture : public Singleton<PhysicsDebugCapture>
{
	friend class Singleton<PhysicsDebugCapture>;
public:
	// 이번 프레임 끝에 한 프레임 기록 (label은 뷰어에 텍스트로 표시)
	void request_capture(const std::string& label = "");
	// GameFramework에서 모든 late_update가 끝난 뒤 매 프레임 호출
	void process_end_of_frame();

	// 기록 시 함께 그릴 기준 형상: owner의 bone에 강체로 붙은 프리미티브 (재질 이름으로 찾음)
	void add_reference_primitive(const std::shared_ptr<GameObject>& owner, const std::string& material_name, const std::string& bone_name);
	void remove_reference_primitives(const GameObject* owner);

	// 파일 닫기 (프레임마다 flush하므로 기록 중에도 뷰어로 열 수 있음)
	void close_session();

private:
	PhysicsDebugCapture(); // 기록기 타입이 불완전하므로 생성자/소멸자는 cpp에서 정의
	~PhysicsDebugCapture() override;

#ifdef JPH_DEBUG_RENDERER
	bool open_session();
	void record_frame();

	struct ReferencePrimitive
	{
		std::weak_ptr<GameObject> owner;
		std::string bone_name;
		std::vector<XMFLOAT3> positions; // 메쉬(모델) 공간 정점
		std::vector<UINT> indices;
	};
	std::vector<ReferencePrimitive> _references;

	std::ofstream _file;
	std::unique_ptr<JPH::StreamOutWrapper> _stream;
	std::unique_ptr<JPH::DebugRendererRecorder> _recorder;

	bool _captureRequested = false;
	std::string _label;
	int _capturedFrames = 0;
#endif
};
