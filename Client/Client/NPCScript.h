#pragma once
#include "ScriptComponent.h"

class NPCScript : public ScriptComponent {
public:
	using required_components = std::tuple<TransformComponent>;

	void set_position(const f3& position);
	const f3& position() const;
	virtual void awake() override;
	virtual void update(float deltaTime) override;
};
