#pragma once
#include "GltfShader.h"

class InstancedglTFShader : public GltfShader {
public:
    virtual const std::string& pso_name() const override {
        static const std::string name = "gltf_instanced";
        return name;
    }
    // GltfShader의 다른 가상 함수들을 그대로 상속받아 사용합니다.
};