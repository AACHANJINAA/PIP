#pragma once
#include "Shader.h"

class Mesh;
constexpr int MAX_UPLOADS_PER_FRAME = 2;  // 프레임당 최대 업로드 수
class ResourceManager : public Singleton<ResourceManager>
{
	friend class Singleton<ResourceManager>; // 싱글톤 접근 허용
	ResourceManager() = default;
	~ResourceManager() = default;
public:
    // release()
	virtual void release() override;

    // GameFramework가 OnCreate에서 호출하여 DX12 객체들을 설정합니다.
    void initialize(ID3D12Device* device, ID3D12GraphicsCommandList* command_list);

    // 파일 경로를 기반으로 메시를 로드하거나, 이미 로드되었다면 캐시된 메시를 반환합니다.
    std::shared_ptr<Mesh> load_mesh(const std::string& file_path, bool _isAnimated = false, std::string animation_name = "null_name");

	// [신규] 메인 루프에서 호출하여 대기 중인 메쉬 업로드
    void process_pending_uploads(ID3D12Device* device, ID3D12GraphicsCommandList* command_list, 
        UINT64 targetFenceValue, size_t maxCount = MAX_UPLOADS_PER_FRAME);
    // [신규] 파일이 아닌 코드로 생성한 메쉬를 대기열에 등록
    void register_manual_mesh(const std::string& name, std::shared_ptr<Mesh> mesh);

    // SkyBox Load 함수 추가 및 SRV 핸들러 추가

    ID3D12DescriptorHeap* get_static_srv_heap() const { return _static_srv_heap.Get(); }
	
    void load_skybox(const std::string& file_path);
    D3D12_GPU_DESCRIPTOR_HANDLE get_skybox_srv();
    D3D12_CPU_DESCRIPTOR_HANDLE get_skybox_srv_cpu() const;
    D3D12_GPU_DESCRIPTOR_HANDLE get_skybox_srv_gpu() const;

    // IBL Maps Load 함수 및 SRV 핸들러
    void load_ibl_maps();
    D3D12_GPU_DESCRIPTOR_HANDLE get_ibl_irradiance_srv();
    D3D12_GPU_DESCRIPTOR_HANDLE get_ibl_prefiltered_srv();
    D3D12_GPU_DESCRIPTOR_HANDLE get_ibl_brdf_lut_srv();
    D3D12_GPU_DESCRIPTOR_HANDLE get_ibl_diffuse_gpu() const { return _ibl_diffuse_gpu_handle; }
    D3D12_GPU_DESCRIPTOR_HANDLE get_ibl_specular_gpu() const { return _ibl_specular_gpu_handle; }
    D3D12_GPU_DESCRIPTOR_HANDLE get_ibl_brdf_lut_gpu() const { return _ibl_brdf_gpu_handle; }

    //// [추가] 대기중인 모든 메시를 GPU에 업로드하는 함수
    //void upload_pending_meshes(ID3D12Device* device, ID3D12GraphicsCommandList* command_list);

    // [변경] Fence 값을 인자로 받아서 완료된 것만 지움
    void release_upload_buffers(UINT64 completedFenceValue);
    // [신규] 업로드 버퍼를 삭제 대기열에 등록 (업로드 직후 호출)
    void register_upload_buffer(ComPtr<ID3D12Resource> buffer, UINT64 targetFenceValue);

	// [추가] 사용되지 않는 메시들을 메모리에서 해제하는 함수
    void unload_unused_meshes();

    // 1. glTF 파일에서 모든 재질과 텍스처를 로드하고 내부적으로 저장합니다.
    //    성공적으로 로드된 재질들의 이름을 반환합니다.
    std::vector<std::string> load_materials_from_gltf(const std::string & file_path);
    // 2. 이름으로 새로운 빈 재질을 생성합니다.
    void create_material(const std::string & material_name);
    // 3. 지정된 이름의 재질에 셰이더를 할당합니다.
    void set_shader_for_material(const std::string & material_name, const std::string& shader_name);
    // 4. 지정된 이름의 재질에 텍스처를 추가합니다.
    void add_texture_to_material(const std::string & material_name, const std::string & texture_path);
    // 5. 이름으로 재질을 찾아, 해당 재질의 셰이더와 텍스처를 렌더링 파이프라인에 바인딩합니다.
    void bind_material(const std::string & material_name, ID3D12GraphicsCommandList * command_list);

	void create_default_textures(ID3D12Device* device, ID3D12GraphicsCommandList* command_list);

    void set_current_command_list(ID3D12GraphicsCommandList* command_list){ _command_list = command_list; }

    // 텍스처의 GPU 리소스와 관련 정보를 저장하는 내부 구조체
    struct TextureInfo
    {
        std::string name;
        ComPtr<ID3D12Resource> resource = nullptr;
        ComPtr<ID3D12Resource> upload_heap = nullptr;
        D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle{};
        D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle{};
    };

private:
    // GlTF PBR 재질 속성을 셰이더로 전달하기 위한 상수 버퍼 구조체
    // 이 구조체는 HLSL의 cbMaterial과 1:1로 매칭됩니다.
    struct GltfMaterialConstantBuffer
    {
        XMFLOAT4 BaseColorFactor;
        XMFLOAT3 EmissiveFactor;
        float MetallicFactor;
        float RoughnessFactor;
        float NormalTextureScale;
        float AlphaCutoff;
        int AlphaMode;          // 0 = OPAQUE, 1 = MASK, 2 = BLEND
        int DoubleSided;        // 0 = false, 1 = true
        int HasBaseColorTexture;
        int HasMetallicRoughnessTexture;
        int HasNormalTexture;
        int HasEmissiveTexture;
        int HasOcclusionTexture;
        float SpecularFactor;
    };
    
    // --- glTF PBR 표준에 맞춘 MaterialInfo 구조체 ---
    struct MaterialInfo
    {
        std::string name;
        std::string shader_name = "gltf";
    
            // --- PBR Metallic-Roughness Properties (glTF 2.0 Spec) ---
        XMFLOAT4 base_color_factor = { 1.0f, 1.0f, 1.0f, 1.0f };
        float metallic_factor = 1.0f;
        float roughness_factor = 1.0f;
        DirectX::XMFLOAT3 emissive_factor = { 0.0f, 0.0f, 0.0f };
    
            // Texture paths (keys to the _textures map)
        std::string base_color_texture_path;
        std::string metallic_roughness_texture_path; // ORM
        std::string occlusion_texture_path;          // Occlusion (별도 분리 시)
        std::string normal_texture_path;
        std::string emissive_texture_path;
    
            // --- Other Material Properties ---
        std::string alpha_mode = "OPAQUE";
        float alpha_cutoff = 0.5f;
        bool double_sided = false;
        float normal_texture_scale = 1.0f;
        float specular_factor = 0.5f;
    
            // --- Per-material Constant Buffer Resources ---
        ComPtr<ID3D12Resource> material_cbuffer_gpu = nullptr;
        UINT8 * material_cbuffer_cpu_address = nullptr;
    };

    // --- GPU 리소스 ---
    ID3D12Device * _device = nullptr;
    ID3D12GraphicsCommandList * _command_list = nullptr;

    // 로드된 리소스들을 파일 경로를 키로 하여 저장하는 맵
    std::unordered_map<std::string, std::shared_ptr<Mesh>> _meshes;
    std::unordered_map<std::string, TextureInfo> _textures;      // Key: 텍스처 파일 경로
    std::unordered_map<std::string, MaterialInfo> _materials;    // Key: 재질 이름
	std::vector<TextureInfo*> _pending_textures; // 아직 GPU에 업로드되지 않은 텍스처 목록


    // Skybox/IBL 전용 SHADER_VISIBLE 힙
    ComPtr<ID3D12DescriptorHeap> _static_srv_heap;
    UINT _static_heap_descriptor_size = 0;

    // skybox 파일 경로
    std::string _skybox_texture_path;

    // ibl 맵 파일 경로
    std::string _ibl_irradiance_path;
    std::string _ibl_prefiltered_path;
    std::string _ibl_brdf_lut_path;

	// skybox SRV 핸들러
    D3D12_CPU_DESCRIPTOR_HANDLE _skybox_cpu_handle = {};
    D3D12_GPU_DESCRIPTOR_HANDLE _skybox_gpu_handle = {};

    D3D12_CPU_DESCRIPTOR_HANDLE _ibl_diffuse_cpu_handle = {};
    D3D12_GPU_DESCRIPTOR_HANDLE _ibl_diffuse_gpu_handle = {};

    D3D12_CPU_DESCRIPTOR_HANDLE _ibl_specular_cpu_handle = {};
    D3D12_GPU_DESCRIPTOR_HANDLE _ibl_specular_gpu_handle = {};

    D3D12_CPU_DESCRIPTOR_HANDLE _ibl_brdf_cpu_handle = {};
    D3D12_GPU_DESCRIPTOR_HANDLE _ibl_brdf_gpu_handle = {};


    // [추가] 로드되었지만 아직 GPU에 업로드되지 않은 메시들의 목록
	std::deque<std::shared_ptr<Mesh>> _pending_meshes;

    struct PendingDeleteBuffer {
        ComPtr<ID3D12Resource> buffer; // ComPtr로 생명주기 연장
        UINT64 targetFenceValue;       // 이 펜스 값에 도달하면 안전함
    };
	std::deque<PendingDeleteBuffer> _pendingDeleteBuffers; // 삭제 대기열

    TextureInfo _default_white_texture;

public:
    // 파일 경로로 텍스처를 로드하고, GPU에 업로드한 뒤, TextureInfo를 반환합니다.
    // CJ251128 - view_dimension 매개변수를 추가하여 텍스처 뷰의 차원을 지정할 수 있도록 함.
    TextureInfo* load_texture(const std::string& file_path, bool is_srgb, D3D12_SRV_DIMENSION view_dimension = D3D12_SRV_DIMENSION_TEXTURE2D);
    void upload_pending_textures(ID3D12GraphicsCommandList* command_list);

    TextureInfo* load_cubemap_from_dds(const std::string& file_path);

    // CJ251201 - 높이맵 관련
    TextureInfo* get_texture(const std::string& file_path);

    MaterialInfo* get_material_info(const std::string& material_name)
    {
        auto it = _materials.find(material_name);
        if (it != _materials.end())
            return &it->second;
        return nullptr;
    }

    TextureInfo* load_heightmap_from_raw(const std::string& file_path, int width, int height);
};
