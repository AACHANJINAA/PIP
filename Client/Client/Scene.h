#pragma once
#include "stdafx.h"
#include "TimerManager.h"
#include "Shader.h"
#include "Camera.h"
#include "LightManager.h"
#include "RenderComponent.h"

// LIGHTS 구조체: 씬의 모든 조명 정보
struct LIGHT
{
	XMFLOAT4 m_xmf4Ambient;
	XMFLOAT4 m_xmf4Diffuse;
	XMFLOAT4 m_xmf4Specular;
	XMFLOAT3 m_xmf3Position;
	float m_fFalloff;
	XMFLOAT3 m_xmf3Direction;
	float m_fTheta; //cos(m_fTheta)
	XMFLOAT3 m_xmf3Attenuation;
	float m_fPhi; //cos(m_fPhi)
	bool m_bEnable;
	int m_nType;
	float m_fRange;
	float padding;
};

struct LIGHTS
{
	LIGHT m_pLights[MAX_LIGHTS];
	XMFLOAT4 m_xmf4GlobalAmbient;
};

// MATERIALS 구조체: 씬의 모든 재질 정보.
struct MATERIALS
{
	Material m_pReflections[MAX_MATERIALS];
};

struct SceneObjectData {
	std::string name;
	std::string meshFile;
	struct {
		XMFLOAT3 location;
		XMFLOAT3 rotation;
		XMFLOAT3 scale;
	} transform;;
};

// 전방 선언
struct ID3D12Device;
struct ID3D12GraphicsCommandList;

class Scene
{
public:
    Scene() = default;
	Scene(const std::string& name) : _sceneName(name) {}
    virtual ~Scene();

    // =================================================================
    // 1. 씬의 새로운 핵심 역할
    // =================================================================

    // [역할 유지] 파생 클래스는 이 함수를 구현하여 씬에 필요한 모든 GameObject를 생성하고 설정해야 합니다.
	virtual void build_objects(ID3D12Device* device, ID3D12GraphicsCommandList* commandList) = 0;

    // [역할 유지] 빌드 과정에서 사용된 업로드 버퍼를 해제합니다.
    virtual void release_upload_buffers() = 0;

	// 씬이 처리해야 할 일이 있다면 처리해주는 함수
	virtual void scene_process(float deltaTime) {}
	virtual void on_scene_loaded();

	void set_scene_name(const std::string& name) { _sceneName = name; }
	const std::string& scene_name() const { return _sceneName; }
    // =================================================================
    // 2. 유틸리티 함수 (선택적으로 유지)
    // =================================================================

    // [역할 유지] 씬 데이터를 파일에서 로드하는 기능은 유용하므로 남겨둡니다.
    // 단, 내부 구현은 새로운 아키텍처에 맞게 변경되어야 합니다. (Chess_Scene에서 재정의)
    virtual void load_scene_from_file(const std::string& filename, ID3D12Device* device, ID3D12GraphicsCommandList* commandList, bool IsTitle = false);
    void load_foliage_from_file(const std::string& filename, ID3D12Device* device, ID3D12GraphicsCommandList* commandList);
	void load_from_file_with_light(const std::string& filename, ID3D12Device* device, ID3D12GraphicsCommandList* commandList);

    virtual void render_post_process(ID3D12GraphicsCommandList* commandList, UINT frame_index) {}

protected:
	std::string _sceneName;
};