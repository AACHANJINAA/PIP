#pragma once
class Mesh;
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
    void allocate_srv_descriptor(D3D12_CPU_DESCRIPTOR_HANDLE& outCpuHandle, 
        D3D12_GPU_DESCRIPTOR_HANDLE& outGpuHandle);

    // 파일 경로를 기반으로 메시를 로드하거나, 이미 로드되었다면 캐시된 메시를 반환합니다.
    std::shared_ptr<Mesh> load_mesh(const std::string& filePath);

    void release_upload_buffers();
    // (향후 확장) 텍스처 로드 함수
    // std::shared_ptr<Texture> load_texture(const std::string& filePath);

private:
    // 로드된 리소스들을 파일 경로를 키로 하여 저장하는 맵
    std::unordered_map<std::string, std::shared_ptr<Mesh>> _meshes;
    // std::map<std::string, std::shared_ptr<Texture>> _textures;

    // 리소스 생성에 필요한 DX12 객체 포인터
    ID3D12Device* _device = nullptr;

    // [추가] SRV 디스크립터 힙 관련 멤버
    ComPtr<ID3D12DescriptorHeap> _srvDescriptorHeap;
    UINT _srvDescriptorIncrementSize = 0;
    UINT _allocatedSrvCount = 0;
};
