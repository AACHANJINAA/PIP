#pragma once
#include "Shader.h"

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
    void initialize(ID3D12Device* device, ID3D12GraphicsCommandList* command_list);

    // 파일 경로를 기반으로 메시를 로드하거나, 이미 로드되었다면 캐시된 메시를 반환합니다.
    std::shared_ptr<Mesh> load_mesh(const std::string& file_path);

    // [추가] 대기중인 모든 메시를 GPU에 업로드하는 함수
    void upload_pending_meshes(ID3D12Device* device, ID3D12GraphicsCommandList* command_list);

	// [추가] 업로드에 사용된 임시 버퍼들을 해제하는 함수
    void release_upload_buffers();

	// [추가] 사용되지 않는 메시들을 메모리에서 해제하는 함수
    void unload_unused_meshes();

    // ----------------------------------------추가 내용----------------------------------------
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
        XMFLOAT2 Padding; // 16바이트 정렬을 위한 패딩
    };

    // 텍스처의 GPU 리소스와 관련 정보를 저장하는 내부 구조체
    struct TextureInfo
    {
         std::string name;
         ComPtr<ID3D12Resource> resource = nullptr;
         ComPtr<ID3D12Resource> upload_heap = nullptr;
         D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle{};
         D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle{};
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
        std::string normal_texture_path;
        std::string emissive_texture_path;
    
            // --- Other Material Properties ---
        std::string alpha_mode = "OPAQUE";
        float alpha_cutoff = 0.5f;
        bool double_sided = false;
        float normal_texture_scale = 1.0f;
    
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

    // [추가] 로드되었지만 아직 GPU에 업로드되지 않은 메시들의 목록
    std::vector<std::shared_ptr<Mesh>> _pending_meshes;

    // 파일 경로로 텍스처를 로드하고, GPU에 업로드한 뒤, TextureInfo를 반환합니다.
    TextureInfo * load_texture(const std::string & file_path);
};
