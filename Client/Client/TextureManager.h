#pragma once

struct Texture;

class TextureManager : public Singleton<TextureManager>
{
	friend class Singleton<TextureManager>;
	TextureManager() = default;	
	~TextureManager() = default;

public:
	void initialize(ID3D12Device* device);

	// 파일 경로에 해당하는 텍스쳐 로드, 이미 로드되었으면 캐시된 텍스쳐를 반환
	std::shared_ptr<Texture> load_texture(const std::string& file_path, ID3D12GraphicsCommandList* command_list);
	// 임시 업로드 버퍼 해제
	void release_upload_buffers();

private:
	ID3D12Device* _device = nullptr;
	
	// 로드된 텍스쳐들을 파일 경로를 키로 하여 저장하는 맵
	std::unordered_map<std::string, std::shared_ptr<Texture>> _textures;
};

