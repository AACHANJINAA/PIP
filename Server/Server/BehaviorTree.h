#pragma once
namespace PIP::GAME
{
	class GameObject;
}

namespace PIP::GAME
{

    // --- 1. Blackboard (데이터 공유) ---
    class Blackboard {
    public:
        // 어떤 데이터든 담을 수 있게 std::any 사용 (Vec3 등 사용자 정의 타입 완벽 지원)
        void set(const std::string& key, const std::any& value) { _data[key] = value; }

        template <typename T>
        T get(const std::string& key) const {
            if (auto it = _data.find(key); it != _data.end()) {
                try {
                    return std::any_cast<T>(it->second);
                }
                catch (...)
                {
	                return T{};
                }
            }
            return T{};
        }
        bool has(const std::string& key) const { return _data.contains(key); }

    private:
        std::unordered_map<std::string, std::any> _data;
    };

    // --- 2. Node Status ---
    enum class NodeStatus { SUCCESS, FAILURE, RUNNING };

    // --- 3. Base Node ---
    class BTNode {
    public:
        virtual ~BTNode() = default;
        virtual NodeStatus tick(float dt, JPH::TempAllocator* allocator) = 0;

        virtual void set_blackboard(std::shared_ptr<Blackboard> bb) { _blackboard = bb; }

    protected:
        std::shared_ptr<Blackboard> _blackboard;
    };

    // --- 4. Composite Nodes (Selector, Sequence) ---
    class Composite : public BTNode {
    protected:
        std::vector<std::shared_ptr<BTNode>> _children;
    public:
        void add_child(std::shared_ptr<BTNode> c) { _children.push_back(c); }

        void set_blackboard(std::shared_ptr<Blackboard> bb) override {
            _blackboard = bb;
            for (auto& child : _children) {
                child->set_blackboard(bb);
            }
        }
    };

    class Selector : public Composite {
    public:
        NodeStatus tick(float dt, JPH::TempAllocator* allocator) override {
            for (auto& c : _children) {
                auto s = c->tick(dt, allocator);
                if (s != NodeStatus::FAILURE) return s;
            }
            return NodeStatus::FAILURE;
        }
    };

    class Sequence : public Composite {
    public:
        NodeStatus tick(float dt, JPH::TempAllocator* allocator) override {
            for (auto& c : _children) {
                auto s = c->tick(dt, allocator);
                if (s != NodeStatus::SUCCESS) return s;
            }
            return NodeStatus::SUCCESS;
        }
    };

    // --- 5. Decorator Nodes (자식을 하나만 가지는 래퍼) ---
    class Decorator : public BTNode {
    protected:
        std::shared_ptr<BTNode> _child;
    public:
        Decorator(std::shared_ptr<BTNode> child) : _child(std::move(child)) {}

        // 블랙보드가 설정될 때 자식에게도 전파
        virtual void on_blackboard_set() { if (_child) _child->set_blackboard(_blackboard); }

        void set_blackboard(std::shared_ptr<Blackboard> bb) override {
            _blackboard = bb;
            if (_child) _child->set_blackboard(bb);
        }
    };

    // 결과 반전
    class Inverter : public Decorator {
    public:
        using Decorator::Decorator;
        NodeStatus tick(float dt, JPH::TempAllocator* allocator) override {
            if (!_child) return NodeStatus::FAILURE;
            auto s = _child->tick(dt, allocator);
            if (s == NodeStatus::SUCCESS) return NodeStatus::FAILURE;
            if (s == NodeStatus::FAILURE) return NodeStatus::SUCCESS;
            return NodeStatus::RUNNING;
        }
    };

    // 무조건 성공
    class Succeeder : public Decorator {
    public:
        using Decorator::Decorator;
        NodeStatus tick(float dt, JPH::TempAllocator* allocator) override {
            if (!_child) return NodeStatus::SUCCESS;
            auto s = _child->tick(dt, allocator);
            if (s == NodeStatus::RUNNING) return NodeStatus::RUNNING;
            return NodeStatus::SUCCESS;
        }
    };

    // --- 6. Leaf Nodes (사용자가 상속해서 행동 정의) ---

    // 행동 클래스 (예: MoveTo, Attack 등)
    class Action : public BTNode {
    public:
        virtual NodeStatus tick(float dt, JPH::TempAllocator* allocator) override = 0;
    };

    // 조건 클래스 (예: IsPlayerNear, IsHealthLow 등)
    class Condition : public BTNode {
    public:
        virtual bool check() = 0; // True/False만 판단
        NodeStatus tick(float dt, JPH::TempAllocator* allocator) override {
            return check() ? NodeStatus::SUCCESS : NodeStatus::FAILURE;
        }
    };

	// --- 7. Behavior Tree Builder ---

	enum class DecoratorType { None, Inverter, Succeeder }; // 필요시 확장 가능 (위의 데코레이터 종류마다)

	class BTBuilder {
        std::shared_ptr<BTNode> _root;
        std::vector<std::shared_ptr<Composite>> _compositeStack;

    public:
        BTBuilder& selector() {
            auto node = std::make_shared<Selector>();
            add_node(node);
            _compositeStack.push_back(node);
            return *this;
        }

        BTBuilder& sequence() {
            auto node = std::make_shared<Sequence>();
            add_node(node);
            _compositeStack.push_back(node);
            return *this;
        }

		// [기존 leaf 함수 - 데코레이터 없음]
        template <typename T, typename... Args>
        BTBuilder& leaf(Args&&... args) {
            return leaf<T>(DecoratorType::None, std::forward<Args>(args)...);
        }
        // [개선된 leaf 함수]
		// DecType: 데코레이터 종류
		// T: 리프 노드 클래스 (Action, Condition 등)
		// Args: 리프 노드 생성자 인자들
        template <typename T, typename... Args>
        BTBuilder& leaf(DecoratorType decType, Args&&... args) {
            // 1. 리프 노드 생성
            std::shared_ptr<BTNode> node = std::make_shared<T>(std::forward<Args>(args)...);

            // 2. 데코레이터 래핑
            switch (decType) {
            case DecoratorType::Inverter:
                node = std::make_shared<Inverter>(node);
                break;
            case DecoratorType::Succeeder:
                node = std::make_shared<Succeeder>(node);
                break;
            default:
                break;
            }

            // 3. 트리에 추가
            add_node(node);
            return *this;
        }

        BTBuilder& end() {
            if (!_compositeStack.empty()) _compositeStack.pop_back();
            return *this;
        }

        std::shared_ptr<BTNode> build() { return _root; }

    private:
        void add_node(std::shared_ptr<BTNode> node) {
            if (_compositeStack.empty()) {
                _root = node;
            }
            else {
                _compositeStack.back()->add_child(node);
            }
        }
    };
}
