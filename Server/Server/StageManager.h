#pragma once
#include "Stage.h"

namespace PIP::SERVER
{
    class StageManager : public Singleton<StageManager>
    {
        friend class Singleton<StageManager>;

    public:
        void initialize() override;

        // 스테이지 타입 등록 (클라이언트 register_scene과 동일 방식)
        template<typename T>
        void register_stage(const std::string& name)
        {
            _creators[name] = []() { return std::make_unique<T>(); };
        }

        // 특정 이름의 스테이지 인스턴스 생성
        std::unique_ptr<Stage> create_stage(const std::string& name)
        {
            if (!_creators.contains(name)) return nullptr;
            return _creators[name]();
        }

        bool is_existing_stage(std::string_view name) const
        {
            return _creators.contains(name.data());
		}

    private:
        std::unordered_map<std::string, std::function<std::unique_ptr<Stage>()>> _creators;
    };
}
