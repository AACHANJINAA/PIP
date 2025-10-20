#pragma once

class Mesh;

class ResourceManager : public Singleton<ResourceManager>
{
	friend class Singleton<ResourceManager>; // 싱글톤 접근 허용
	ResourceManager() = default;
	~ResourceManager() = default;
public:
    // release()
	virtual void release() override;

    // GameFramework가 OnCreate에서 호출하여 DX12 객체들을 설정합니다.
    void initialize(ID3D12Device* device);

    // 파일 경로를 기반으로 메시를 로드하거나, 이미 로드되었다면 캐시된 메시를 반환합니다.
    std::shared_ptr<Mesh> load_mesh(const std::string& file_path);

    // [추가] 대기중인 모든 메시를 GPU에 업로드하는 함수
    void upload_pending_meshes(ID3D12Device* device, ID3D12GraphicsCommandList* command_list);

	// [추가] 업로드에 사용된 임시 버퍼들을 해제하는 함수
    void release_upload_buffers();

	// [추가] 사용되지 않는 메시들을 메모리에서 해제하는 함수
    void unload_unused_meshes();

private:
    // 로드된 리소스들을 파일 경로를 키로 하여 저장하는 맵
    std::unordered_map<std::string, std::shared_ptr<Mesh>> _meshes;

    // [추가] 로드되었지만 아직 GPU에 업로드되지 않은 메시들의 목록
    std::vector<std::shared_ptr<Mesh>> _pending_meshes;
};
