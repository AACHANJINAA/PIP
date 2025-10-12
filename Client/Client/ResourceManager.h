#pragma once

class Mesh;

struct Texture;

class ResourceManager : public Singleton<ResourceManager>
{
	friend class Singleton<ResourceManager>; // 싱글톤 접근 허용
	ResourceManager() = default;
	~ResourceManager() = default;
public:
    // GameFramework가 OnCreate에서 호출하여 DX12 객체들을 설정합니다.
    void initialize(ID3D12Device* device);

    // [추가] SRV 디스크립터 힙을 반환하는 Getter
    ID3D12DescriptorHeap* get_srv_heap() const { return _srvDescriptorHeap.Get(); }
    // [추가] 다음 SRV 디스크립터를 할당하고 핸들을 반환하는 함수
    void allocate_srv_descriptor(D3D12_CPU_DESCRIPTOR_HANDLE& outCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE& outGpuHandle);

    // 파일 경로를 기반으로 메시를 로드하거나, 이미 로드되었다면 캐시된 메시를 반환합니다.
    std::shared_ptr<Mesh> load_mesh(const std::string& file_path);

    // [추가] 대기중인 모든 메시를 GPU에 업로드하는 함수
    void upload_pending_meshes(ID3D12Device* device, ID3D12GraphicsCommandList* command_list);

	// [추가] 업로드에 사용된 임시 버퍼들을 해제하는 함수
    void release_upload_buffers();

	// [추가] 사용되지 않는 메시들을 메모리에서 해제하는 함수
    void unload_unused_meshes();

	// 파일 경로를 기반으로 텍스처를 로드하거나, 이미 로드되었다면 캐시된 텍스처를 반환
    std::shared_ptr<Texture> load_texture(const std::string& file_path, ID3D12Device* device,ID3D12GraphicsCommandList* command_list);

private:
    // 로드된 리소스들을 파일 경로를 키로 하여 저장하는 맵
    std::unordered_map<std::string, std::shared_ptr<Mesh>> _meshes;
    // std::map<std::string, std::shared_ptr<Texture>> _textures;

    // [추가] 로드되었지만 아직 GPU에 업로드되지 않은 메시들의 목록
    std::vector<std::shared_ptr<Mesh>> _pending_meshes;


    // [추가] SRV 디스크립터 힙 관련 멤버
    ComPtr<ID3D12DescriptorHeap> _srvDescriptorHeap;
    UINT _srvDescriptorIncrementSize = 0;
    UINT _allocatedSrvCount = 0;

	// 로드된 텍스처들을 파일 경로를 키로 하여 저장하는 맵
    std::unordered_map<std::string, std::shared_ptr<Texture>> _textures;
};
