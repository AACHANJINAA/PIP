#pragma once
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <string>
#include <variant>

// --- 1. Blackboard (데이터 공유) ---
class Blackboard {
public:
	// 간단하게 정수, 실수, 벡터, 불리언 등을 저장한다고 가정
	using ValueType = std::variant<int, float, bool, std::string /*, Vector3 등 추가*/>;

	void set(const std::string& key, ValueType value) { _data[key] = value; }

	template <typename T>
	T get(const std::string& key) {
		if (_data.contains(key)) {
			return std::get<T>(_data[key]);
		}
		return T{}; // 기본값
	}

	bool has(const std::string& key) const { return _data.contains(key); }

private:
	std::unordered_map<std::string, ValueType> _data;
};
// --- 2. Node 기본 구조 ---
enum class NodeStatus { Success, Failure, Running };

class BTNode {
public:
	virtual ~BTNode() = default;
	virtual NodeStatus tick(float dt) = 0;
};

// --- 3. Composites (제어 노드) ---
class Selector : public BTNode { // OR
	std::vector<std::shared_ptr<BTNode>> _children;
public:
	void add_child(std::shared_ptr<BTNode> c) { _children.push_back(c); }
	NodeStatus tick(float dt) override {
		for (auto& c : _children) {
			auto s = c->tick(dt);
			if (s != NodeStatus::Failure) return s;
		}
		return NodeStatus::Failure;
	}
};

class Sequence : public BTNode { // AND
	std::vector<std::shared_ptr<BTNode>> _children;
public:
	void add_child(std::shared_ptr<BTNode> c) { _children.push_back(c); }
	NodeStatus tick(float dt) override {
		for (auto& c : _children) {
			auto s = c->tick(dt);
			if (s != NodeStatus::Success) return s;
		}
		return NodeStatus::Success;
	}
};

// --- 4. Leaf Nodes (행동 노드) ---
class Action : public BTNode {
	std::function<NodeStatus(float)> _fn;
public:
	Action(std::function<NodeStatus(float)> fn) : _fn(fn) {}
	NodeStatus tick(float dt) override { return _fn(dt); }
};

class Condition : public BTNode { // 단순 조건 검사 (True/False)
	std::function<bool()> _pred;
public:
	Condition(std::function<bool()> pred) : _pred(pred) {}
	NodeStatus tick(float dt) override {
		return _pred() ? NodeStatus::Success : NodeStatus::Failure;
	}
};
// --- 5. Builder (편리한 생성) ---
class BTBuilder {
	std::shared_ptr<BTNode> _root;
	std::vector<std::shared_ptr<BTNode>> _stack; // 부모 노드들을 추적

public:
	BTBuilder() = default;

	BTBuilder& selector() {
		auto node = std::make_shared<Selector>();
		add_to_parent(node);
		_stack.push_back(node);
		return *this;
	}

	BTBuilder& sequence() {
		auto node = std::make_shared<Sequence>();
		add_to_parent(node);
		_stack.push_back(node);
		return *this;
	}

	BTBuilder& action(std::function<NodeStatus(float)> fn) {
		add_to_parent(std::make_shared<Action>(fn));
		return *this;
	}

	BTBuilder& condition(std::function<bool()> pred) {
		add_to_parent(std::make_shared<Condition>(pred));
		return *this;
	}

	BTBuilder& end() {
		if (!_stack.empty()) _stack.pop_back();
		return *this;
	}

	std::shared_ptr<BTNode> build() { return _root; }

private:
	void add_to_parent(std::shared_ptr<BTNode> node) {
		if (_stack.empty()) {
			_root = node;
		}
		else {
			auto parent = _stack.back();
			if (auto sel = std::dynamic_pointer_cast<Selector>(parent)) sel->add_child(node);
			else if (auto seq = std::dynamic_pointer_cast<Sequence>(parent)) seq->add_child(node);
		}
	}
};

