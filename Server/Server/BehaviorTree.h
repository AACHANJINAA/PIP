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
            auto it = _data.find(key);
            if (it != _data.end()) {
                // 1. 정확한 타입 매칭 시도
                const T* ptr = std::any_cast<T>(&it->second);
                if (ptr) return *ptr;

                // 2. 숫자 타입 간의 유연한 변환 지원 (정수형에 한함)
                if constexpr (std::is_integral_v<T>) {
                    if (const int* i_ptr = std::any_cast<int>(&it->second)) return static_cast<T>(*i_ptr);
                    if (const int64_t* l_ptr = std::any_cast<int64_t>(&it->second)) return static_cast<T>(*l_ptr);
                    if (const uint64_t* ul_ptr = std::any_cast<uint64_t>(&it->second)) return static_cast<T>(*ul_ptr);
                }

                // 타입 불일치 시 로그 남기기
                MYERROR("Bad any_cast for key: " << key << " (Stored type: " << it->second.type().name() << ")" << std::endl);
            }
            return T{}; // 기본값 반환
        }
        bool has(const std::string& key) const {
            auto it = _data.find(key);
            return it != _data.end() && it->second.has_value();
        }

    private:
        std::unordered_map<std::string, std::any> _data;
    };

    // --- 2. Node Status ---
    enum class NodeStatus { SUCCESS, FAILURE, RUNNING };

    // --- 3. Base Node ---
    class BTNode {
    public:
        virtual ~BTNode() = default;

        // 노드 식별을 위한 이름 추가
        void set_name(const std::string& name) { _nodeName = name; }
        const std::string& get_name() const { return _nodeName; }

        virtual NodeStatus tick(float dt, JPH::TempAllocator* allocator) = 0;

        virtual void set_blackboard(std::shared_ptr<Blackboard> bb) { _blackboard = bb; }

    protected:
        std::shared_ptr<Blackboard> _blackboard;
        std::string _nodeName; // 노드 이름
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
                // [디버그] 실행 중이거나 성공한 노드 정보를 블랙보드에 실시간 기록
                if (!c->get_name().empty() && (s == NodeStatus::RUNNING || s == NodeStatus::SUCCESS)) {
                    _blackboard->set("debug_node_name", c->get_name());
                    _blackboard->set("debug_node_status", (int)s);
                }
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
                // [디버그] 현재 진행 중인 노드 기록
                 // [수정] 자식의 이름이 비어있지 않을 때만 기록!
                if (!c->get_name().empty()) {
                    _blackboard->set("debug_node_name", c->get_name());
                    _blackboard->set("debug_node_status", (int)s);
                }
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

        template <typename T, typename... Args>
        BTBuilder& leaf_name(const std::string& name, Args&&... args) {
            std::shared_ptr<BTNode> node = std::make_shared<T>(std::forward<Args>(args)...);
            node->set_name(name); // 이름 설정

            add_node(node);
            return *this;
        }

        template <typename T, typename... Args>
        BTBuilder& leaf_name(const std::string& name, DecoratorType decType, Args&&... args) {
            std::shared_ptr<BTNode> node = std::make_shared<T>(std::forward<Args>(args)...);
            node->set_name(name); // 이름 설정
            // 1. 리프 노드 생성

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
