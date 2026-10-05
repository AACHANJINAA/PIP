#pragma once

class GameObject;

// 개발용 ImGui 디버그 패널 (\ 키로 열고 닫음)
// - Network: 서버 시계·RTT, 보간 상태 집계, 서버 위치 유령, 인공 수신 지연·흔들림 (예전 F6 창)
// - Particle Test: 범용 파티클 테스트 방출기 (모드 4가지, 재생/정지, 버스트로 최대 개수 초과 확인)
// 창을 마우스로 조작하려면 ESC로 커서를 보이게 한다.
class DebugPanel : public Singleton<DebugPanel>
{
    friend class Singleton<DebugPanel>;
public:
    void toggle() { _open = !_open; }
    bool is_open() const { return _open; }

    // ImGui 프레임 안에서 호출 (GameFramework, 데미지 텍스트 다음)
    void draw();

private:
    DebugPanel() = default;

    void draw_particle_test();
    void apply_particle_mode();     // 모드에 맞춰 설정·방출 방향을 다시 정함
    void place_particle_test();     // 플레이어 앞(보는 방향 반대쪽 3m, 1m 위)으로 옮김

    bool _open = false;

    std::shared_ptr<GameObject> _particleTest;
    int _particleMode = 0;
    int _burstCount = 1500;
};
