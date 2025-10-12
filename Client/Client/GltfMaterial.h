#pragma once
#include "stdafx.h"

class Shader;
struct Texture;

class GltfMaterial
{
public:
	GltfMaterial(const std::string& name);
	~GltfMaterial() = default;

	void set_shader(std::shared_ptr<Shader> shader);
	void set_texture(std::shared_ptr<Texture> texture, UINT index = 0); // 여러 개의 텍스쳐를 위해 인덱스 추가

	const std::string& name() const { return _name; }
	std::shared_ptr<Shader> shader() const { return _shader; }

	// 렌더링 시 호출되어 텍스처를 파이프라인에 바인딩하는 함수
	void bind(ID3D12GraphicsCommandList* command_list) const;

private:
	std::string _name;
	std::shared_ptr<Shader> _shader;
	std::vector<std::shared_ptr<Texture>> _textures; // 여러 개의 텍스처를 저장할 벡터
};

