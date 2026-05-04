#pragma once
namespace PIP
{   
	class PhysicsManager : public Singleton<PhysicsManager> {
		friend class Singleton<PhysicsManager>;
        PhysicsManager() = default;
		~PhysicsManager() override = default;
    public:
        void initialize() override;
        void Shutdown();

        // 팩토리 초기화 상태 확인용
        bool IsInitialized() const { return _isInitialized; }
    private:
        bool _isInitialized = false;
        
	};
}

