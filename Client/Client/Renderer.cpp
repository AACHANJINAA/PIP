#include "stdafx.h"

#include "Renderer.h"

#include "Shader.h"
#include "DebugShader.h"
#include "DefaultObjectShader.h"
#include "GlbShader.h"
#include "GltfShader.h"
#include "PlayerShader.h"
#include "SkyboxShader.h"
#include "GltfSkinnedShader.h"
#include "TerrainShader.h"
#include "UIShader.h"
#include "MonsterHPUIShader.h"
#include "ShadowDepthShader.h"
#include "ShadowDepthSkinnedShader.h"
#include "UIFrameShader.h"
#include "UIManager.h"

#include "GameObject.h"
#include "ObjectManager.h"
#include "AnimationComponent.h"

#include "Camera.h"
#include "CameraComponent.h"
#include "DebugDrawManager.h"
#include "LightManager.h"
#include "RenderComponent.h"
#include "OcclusionManager.h"
#include "OcclusionQueryShader.h"

#include "TerrainLoader.h"
#include "ResourceManager.h"
#include "ShadowManager.h"
#include "MinimapShader.h"

void Renderer::initialize(ID3D12Device* device)
{
    _device = device;
    
    _descriptor_size = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV); 
    _unitCube = Mesh::create_unit_cube();
    OcclusionManager::instance()->initialize(device, 10'0000);

    create_dynamic_descriptor_heap(1000000);

    // [추가] 사용할 루트 시그니처 생성기들을 등록합니다.
    _rootSignatureGenerators.push_back(std::make_unique<DebugRootSignatureGenerator>()); // 추가
    _rootSignatureGenerators.push_back(std::make_unique<DefaultRootSignatureGenerator>());
    _rootSignatureGenerators.push_back(std::make_unique<GltfRootSignatureGenerator>());
    _rootSignatureGenerators.push_back(std::make_unique<SkyBoxRootSignatureGenerator>());
    _rootSignatureGenerators.push_back(std::make_unique<SkinnedRootSignatureGenerator>());
    _rootSignatureGenerators.push_back(std::make_unique<TerrainRootSignatureGenerator>());
    _rootSignatureGenerators.push_back(std::make_unique<MonsterHPUIRootSignatureGenerator>());
    _rootSignatureGenerators.push_back(std::make_unique<UIRootSignatureGenerator>());
    _rootSignatureGenerators.push_back(std::make_unique<CsmDepthRootSignatureGenerator>());
	_rootSignatureGenerators.push_back(std::make_unique<CsmDepthSkinnedRootSignatureGenerator>());
	_rootSignatureGenerators.push_back(std::make_unique<UIFrameRootSignatureGenerator>());
	_rootSignatureGenerators.push_back(std::make_unique<MinimapRootSignatureGenerator>());
    _rootSignatureGenerators.push_back(std::make_unique<OcclusionRootSignatureGenerator>());
    // 새 루트 시그니처가 필요하면 여기에 생성기만 추가하면 끝입니다.

    // [추가] PSO를 생성할 셰이더 프로토타입들을 등록합니다.
    // [변경] 셰이더 프로토타입을 등록할 때, 셰이더의 이름을 키로 하여 map에 저장합니다.
	auto default_shader = std::make_shared<DefaultObjectShader>();
    _shaderPrototypes[default_shader->pso_name()] = default_shader;

    auto player_shader = std::make_shared<PlayerShader>();
    _shaderPrototypes[player_shader->pso_name()] = player_shader;

    auto debug_shader = std::make_shared<DebugShader>();
    _shaderPrototypes[debug_shader->pso_name()] = debug_shader;

    auto glb_shader = std::make_shared<GlbShader>();
    _shaderPrototypes[glb_shader->pso_name()] = glb_shader;

    auto gltf_shader = std::make_shared<GltfShader>();
    _shaderPrototypes[gltf_shader->pso_name()] = gltf_shader;

	auto skybox_shader = std::make_shared<SkyboxShader>();
	_shaderPrototypes[skybox_shader->pso_name()] = skybox_shader;

    auto gltf_animation_shader = std::make_shared<GltfSkinnedShader>();
    _shaderPrototypes[gltf_animation_shader->pso_name()] = gltf_animation_shader;

	auto terrain_shader = std::make_shared<TerrainShader>();
	_shaderPrototypes[terrain_shader->pso_name()] = terrain_shader;

    auto monster_hp_ui_shader = std::make_shared<MonsterHPUIShader>();
    _shaderPrototypes[monster_hp_ui_shader->pso_name()] = monster_hp_ui_shader;

    auto ui_shader = std::make_shared<UIShader>();
    _shaderPrototypes[ui_shader->pso_name()] = ui_shader;

	auto shadow_depth_shader = std::make_shared<ShadowDepthShader>();
	_shaderPrototypes[shadow_depth_shader->pso_name()] = shadow_depth_shader;

    auto shadow_depth_skinned_shader = std::make_shared<ShadowDepthSkinnedShader>();
    _shaderPrototypes[shadow_depth_skinned_shader->pso_name()] = shadow_depth_skinned_shader;

    auto ui_frame_shader = std::make_shared<UIFrameShader>();
    _shaderPrototypes[ui_frame_shader->pso_name()] = ui_frame_shader;

    auto minimap_shader = std::make_shared<MinimapShader>();
    _shaderPrototypes[minimap_shader->pso_name()] = minimap_shader;

    auto occlusion_shader = std::make_shared<OcclusionQueryShader>();
    _shaderPrototypes[occlusion_shader->pso_name()] = occlusion_shader;

    create_root_signatures(device);
    create_pipeline_state_objects(device);
}
void Renderer::create_root_signatures(ID3D12Device* device)
{
    _rootSignatures.clear();
    for (const auto& generator : _rootSignatureGenerators)
    {
        _rootSignatures[generator->name()] = generator->create(device);
    }
}   

void Renderer::create_pipeline_state_objects(ID3D12Device* device)
{
    _pipelineStates.clear(); // 기존 PSO 맵 비우기

    // 등록된 모든 셰이더 프로토타입을 순회합니다.
    for (const auto& [shader_name, shader_prototype] : _shaderPrototypes)
    {
        // 1. 셰이더가 요구하는 루트 시그니처를 가져옵니다.
        ID3D12RootSignature* root_signature =
            get_root_signature(shader_prototype->required_root_signature());
        if (!root_signature) continue;

        // 2. 셰이더 프로토타입에게 PSO 생성을 위임합니다.
        ComPtr<ID3D12PipelineState> pso = shader_prototype->create_pso(device, root_signature);

        // 3. 반환된 PSO를 맵에 저장합니다. 이름은 셰이더가 직접 알려줍니다.
        if (pso)
        {
            _pipelineStates[shader_prototype->pso_name()] = pso;
        }
    }
}

void Renderer::render(ID3D12GraphicsCommandList* commandList, UINT frame_index)
{
    // 프레임 렌더링 시작 시, 동적 디스크립터 힙의 인덱스를 리셋
    _current_dynamic_descriptor_index = frame_index * _max_descriptors_per_frame;

    CameraComponent* camera = CameraComponent::get_main();
    if (!camera)
    {
        // 렌더링할 카메라가 없으면 아무것도 하지 않습니다.
        CERROR("렌더시에 카메라 1개이상은 필요함")
        return;
    }

    // 1. 이번 프레임에 그릴 객체들을 추려낸다.
    build_render_list(camera);

    // 2. 추려낸 목록을 바탕으로 실제 그리기를 수행한다.
	//draw_render_list(commandList, camera, frame_index);
    draw_render_occlusion_culling_list(commandList, camera,  frame_index);

#ifdef _DEBUG_PHYSICS_VISUALIZATION
    // [수정] viewProj가 아니라 frame_index를 넘겨야 합니다!
    DebugDrawManager::instance()->Render(commandList, frame_index);
#endif
}

void Renderer::build_render_list(const CameraComponent* camera)
{
    _renderMap.clear();
    const auto& allGameObjects = ObjectManager::instance()->get_all_game_objects();
    const BoundingFrustum& frustum = camera->frustum();

    for (const auto& gameObject : allGameObjects)
    {
        if (!gameObject || !gameObject->is_enable() || gameObject->is_destroyed()) continue;

        auto renderComp = gameObject->get_component<RenderComponent>();

        if (renderComp && renderComp->is_enabled())
        {
            // UI와 Skybox는 bounding box 체크 없이 렌더링
            if (renderComp->pso_name() == "ui" ||
                renderComp->pso_name() == "Monster_HP_UI" ||
                renderComp->pso_name() == "ui_frame" ||
                renderComp->pso_name() == "skybox")
            {
                if (renderComp->pso_name() == "ui")
                {
                    // ui는 넘기기
                    continue;
                }
                _renderMap[renderComp->pso_name()].push_back(gameObject);
                continue;
            }

            // 일반 객체는 frustum culling
            BoundingOrientedBox obb = renderComp->get_world_bounding_box();
            if (obb.Extents.x <= 0.0f || std::isnan(obb.Center.x))
            {
                continue;
            }

            if (renderComp->is_visible(frustum))
            {
                _renderMap[renderComp->pso_name()].push_back(gameObject);
            }
        }
    }

    UIManager::instance()->set_render_vector();
    const auto& render_vec = UIManager::instance()->ui_render_vector();
    for (const auto& vec : render_vec)
    {
        for (const auto& gameObject : vec)
        {
			if (!gameObject || !gameObject->is_enable() || gameObject->is_destroyed())
			{
				continue;
			}
            auto renderComp = gameObject->get_component<RenderComponent>();
            if (renderComp)
            {
                _renderMap[renderComp->pso_name()].push_back(gameObject);
            }
        }
    }

    /*CLOG("Culling: " << visibleObjects << "/" << totalObjects << " visible, "
        << invalidBoundingBoxCount << " invalid BB");*/
}

void Renderer::draw_render_list(ID3D12GraphicsCommandList* commandList, CameraComponent* camera, UINT frame_index)
{
    ID3D12DescriptorHeap* heaps[] = { _dynamic_descriptor_heap.Get() };
    commandList->SetDescriptorHeaps(_countof(heaps), heaps);

    // 렌더링 순서 명시: 일반 객체 → Skybox → UI
    std::vector<std::string> render_order = {
        "terrain",      // 지형
        "gltf",         // 일반 메시
        "skinned",      // 애니메이션 메시
        "skybox",       // Skybox
        "Monster_HP_UI",// 몬스터 HP UI
        "ui_frame",      // UI Frame
        "ui"            // UI
    };

    // CJ 주절주절 : ui가 먼저 렌더링 되는게 맞지 않을까란 생각. 왜냐하면 ui가 3d mesh들 위에 그려짐으로 발생하는 RT의 픽셀 낭비 발생
    // CJ 비난 : 어차피 alpha 테스트가 되어 있기 때문에 굳이임 -> 처음에 렌더링해버리면 우리 예전처럼 ui가 가려지는 현상 발생함

    for (const auto& psoName : render_order)
    {
        auto it = _renderMap.find(psoName);
        if (it == _renderMap.end() || it->second.empty()) continue;

        const auto& gameObjects = it->second;
        // 어차피 _renderMap에서 게임오브젝트의 상태를 보고 컬링해서 들어옴

        // PSO와 루트 시그니처 설정
        ID3D12PipelineState* pso = get_pso(psoName);
        if (!pso) continue;

        auto proto_it = _shaderPrototypes.find(psoName);
        if (proto_it == _shaderPrototypes.end()) continue;

        const auto& shader_prototype = proto_it->second;
        const std::string& root_sig_name = shader_prototype->required_root_signature();
        ID3D12RootSignature* root_signature = get_root_signature(root_sig_name);
        if (!root_signature) continue;

        commandList->SetPipelineState(pso);
        commandList->SetGraphicsRootSignature(root_signature);

        if (psoName == "gltf" || psoName == "skinned")
        {
            LightManager::instance()->bind(commandList, 3);
            ShadowManager::instance()->bind_for_lighting(commandList, 10, 11, this);
        }

        if (psoName != "skybox")
        {
            ID3D12DescriptorHeap* heaps[] = {
                    _dynamic_descriptor_heap.Get() };
                    commandList->SetDescriptorHeaps(_countof(heaps),
                    heaps);
        }
        if (camera)
        {
            camera->update_shader_variables(commandList, frame_index);
            camera->set_viewports_and_scissor_rects(commandList);
        }

        // Skybox 전용 처리
        if (psoName == "skybox")
        {
           /* auto static_heap = ResourceManager::instance()->get_static_srv_heap();
            if (static_heap)
            {
                ID3D12DescriptorHeap* heaps[] = { static_heap };
                commandList->SetDescriptorHeaps(1, heaps);
            }*/

            // Skybox 상수 버퍼
            if (camera && camera->get_cb_skybox())
            {
                D3D12_GPU_VIRTUAL_ADDRESS cbAddress =
                    camera->get_cb_skybox()->GetGPUVirtualAddress();
                commandList->SetGraphicsRootConstantBufferView(2, cbAddress);
            }

            // Skybox 텍스처 (고정 힙의 GPU 핸들 직접 사용)
            D3D12_CPU_DESCRIPTOR_HANDLE skybox_cpu_handle = ResourceManager::instance()->get_skybox_srv_cpu();
            
            // 안전장치: 스카이박스 핸들이 없으면 블랙 텍스처라도 넣어줌
            if (skybox_cpu_handle.ptr == 0) {
                skybox_cpu_handle = ResourceManager::instance()->get_texture("__DEFAULT_BLACK__")->cpu_handle;
            }

            bind_texture_table(commandList, 4, { skybox_cpu_handle });

            // Skybox 렌더링
            for (const auto& gameObject : gameObjects)
            {
                auto renderComp = gameObject->get_component<RenderComponent>();
                if (renderComp && renderComp->is_enabled())
                {
                    renderComp->render(commandList, frame_index);
                }
            }
            continue; // 다음 PSO로
        }

        // 일반 객체 렌더링
        for (const auto& gameObject : gameObjects)
        {
            auto renderComp = gameObject->get_component<RenderComponent>();
            if (!renderComp) continue;

            auto mesh = renderComp->mesh();
            if (!mesh) continue;

            if (psoName == "skinned") // DW설명 : 애니메이션 컴포넌트에서 뼈대 상수 버퍼 주소 받아오기
            {
                auto animComp = gameObject->get_component<AnimationComponent>();
                if (animComp)
                {
                    // AnimationComponent(선형 할당기)에서 받아둔 뼈대 주소 획득
                    D3D12_GPU_VIRTUAL_ADDRESS boneGpuAddr = animComp->get_bone_gpu_virtual_address();

                    if (boneGpuAddr != 0)
                    {
                        // 12번 루트 파라미터(b4 레지스터)에 뼈대 상수 버퍼 바인딩
                        commandList->SetGraphicsRootConstantBufferView(12, boneGpuAddr);
                    }
                }
            }

            shader_prototype->update_per_object(commandList, this, gameObject.get());
            gameObject->prepare_render();
            renderComp->render(commandList, frame_index);
        }
    }
}

void Renderer::render_pso_group(ID3D12GraphicsCommandList* commandList, const std::string& psoName, CameraComponent* camera, UINT frame_index) {
    auto it = _renderMap.find(psoName);
    if (it == _renderMap.end() || it->second.empty()) return;

    // 디스크립터 힙 설정 (SRV 테이블 사용을 위해 필수)
    ID3D12DescriptorHeap* heaps[] = { _dynamic_descriptor_heap.Get() };
    commandList->SetDescriptorHeaps(_countof(heaps), heaps);

    // PSO 및 루트 시그니처 설정
    ID3D12PipelineState* pso = get_pso(psoName);
    auto proto_it = _shaderPrototypes.find(psoName);
    if (pso == nullptr || proto_it == _shaderPrototypes.end()) return;

    auto& proto = proto_it->second;
    commandList->SetPipelineState(pso);
    commandList->SetGraphicsRootSignature(get_root_signature(proto->required_root_signature()));

    // 셰이더별 바인딩 분기 (기존과 동일)
    if (psoName == "gltf" || psoName == "skinned") {
        LightManager::instance()->bind(commandList, 3);
        ShadowManager::instance()->bind_for_lighting(commandList, 10, 11, this);
    }
    else if (psoName == "terrain") {
        LightManager::instance()->bind(commandList, 3);
        // Terrain은 ShadowManager의 bind_for_lighting(6, 7)을 사용함
        ShadowManager::instance()->bind_for_lighting(commandList, 6, 7, this);
    }

    if (camera) camera->update_shader_variables(commandList, frame_index);

    for (auto& obj : it->second) {
        if (!obj) continue;
        proto->update_per_object(commandList, this, obj.get());
        obj->prepare_render();
        obj->get_component<RenderComponent>()->render(commandList, frame_index);
    }
}
void Renderer::draw_render_occlusion_culling_list(ID3D12GraphicsCommandList* commandList, CameraComponent* camera, UINT frame_index) {
    // 0. 디스크립터 힙 설정 (SRV 테이블 사용을 위해 필수)
	// render_pso_group이 지형이 없어 조기 리턴될 경우를 대비해 함수 시작 시점에 미리 설정합니다.
    ID3D12DescriptorHeap* heaps[] = { _dynamic_descriptor_heap.Get() };
    commandList->SetDescriptorHeaps(_countof(heaps), heaps);
	
    if (camera) {
        camera->set_viewports_and_scissor_rects(commandList);
        camera->update_shader_variables(commandList, frame_index);
    }


    // 1. Terrain (Occluder) 그리기
    render_pso_group(commandList, "terrain", camera, frame_index);

    f3 camPos = camera->game_object()->transform()->get_world_position();

    // 2. Query Pass - 가려짐 여부 판정용 박스 그리기
    auto occ_pso = get_pso("occlusion_query");
    auto occ_sig = get_root_signature("occlusion_sig");
    if (occ_pso && occ_sig && _unitCube) {
        commandList->SetPipelineState(occ_pso);
        commandList->SetGraphicsRootSignature(occ_sig);

        std::string query_targets[] = { "gltf", "skinned" };
        for (const std::string& target : query_targets) {
            auto it = _renderMap.find(target);
            if (it == _renderMap.end()) continue;

            for (auto& obj : it->second) {
                auto rc = obj->get_component<RenderComponent>();
                if (!rc) continue;

                // [최적화] 거리가 가까운 물체는 쿼리 생략
                f3 objPos = obj->transform()->get_world_position();
                float dist = Vector3::Length(Vector3::Subtract(camPos, objPos));

                rc->set_skip_occlusion(dist < 30.0f);

                if (!rc->skip_occlusion()) {
                    XMMATRIX boxWorld = XMMatrixTranspose(rc->get_occlusion_box_world_matrix());
                    commandList->SetGraphicsRoot32BitConstants(0, 16, &boxWorld, 0);

                    commandList->BeginQuery(OcclusionManager::instance()->get_query_heap(), D3D12_QUERY_TYPE_OCCLUSION, rc->get_occlusion_query_index());
                    _unitCube->render(commandList);
                    commandList->EndQuery(OcclusionManager::instance()->get_query_heap(), D3D12_QUERY_TYPE_OCCLUSION, rc->get_occlusion_query_index());
                }
            }
        }
    }

    OcclusionManager::instance()->resolve_queries(commandList, frame_index);

    // 3. 실제 렌더링
    ID3D12Resource* prevBuffer = OcclusionManager::instance()->get_result_buffer_for_predication(frame_index);
    std::string render_targets[] = { "gltf", "skinned" };

    for (const std::string& target : render_targets) {
        auto it = _renderMap.find(target);
        if (it == _renderMap.end() || it->second.empty()) continue;

        auto pso = get_pso(target);
        auto proto = _shaderPrototypes[target];
        commandList->SetPipelineState(pso);
        commandList->SetGraphicsRootSignature(get_root_signature(proto->required_root_signature()));
        commandList->SetDescriptorHeaps(_countof(heaps), heaps);

        LightManager::instance()->bind(commandList, 3);
        ShadowManager::instance()->bind_for_lighting(commandList, 10, 11, this);
        if (camera) camera->update_shader_variables(commandList, frame_index);

        for (auto& obj : it->second) {
            auto rc = obj->get_component<RenderComponent>();
            if (!rc) continue;

            if (target == "skinned") {
                auto animComp = obj->get_component<AnimationComponent>();
                if (animComp) {
                    D3D12_GPU_VIRTUAL_ADDRESS boneGpuAddr = animComp->get_bone_gpu_virtual_address();
                    if (boneGpuAddr != 0) commandList->SetGraphicsRootConstantBufferView(12, boneGpuAddr);
                }
            }
            proto->update_per_object(commandList, this, obj.get());
            obj->prepare_render();

            if (rc->skip_occlusion()) {
                rc->render(commandList, frame_index);
            }
            else {
                // 쿼리 결과(0이 아니면 보임)에 따라 조건부 렌더링
                commandList->SetPredication(prevBuffer, rc->get_occlusion_query_index() * sizeof(UINT64), D3D12_PREDICATION_OP_NOT_EQUAL_ZERO);
                rc->render(commandList, frame_index);
                commandList->SetPredication(nullptr, 0, D3D12_PREDICATION_OP_EQUAL_ZERO);
            }
        }
    }

    // 5. Skybox 렌더링 (오클루전 컬링 제외)
    auto itSky = _renderMap.find("skybox");
    if (itSky != _renderMap.end() && !itSky->second.empty()) {
		const std::string psoName = "skybox";
		ID3D12PipelineState* pso = get_pso(psoName);
		auto proto_it = _shaderPrototypes.find(psoName);

		if (pso && proto_it != _shaderPrototypes.end()) {
		    auto& proto = proto_it->second;
		    commandList->SetPipelineState(pso);
		    commandList->SetGraphicsRootSignature(get_root_signature(proto->required_root_signature()));
		    commandList->SetDescriptorHeaps(_countof(heaps), heaps);

		    // Skybox 전용 상수 데이터 바인딩 (슬롯 2)
		    if (camera && camera->get_cb_skybox()) {
		        D3D12_GPU_VIRTUAL_ADDRESS cbAddress = camera->get_cb_skybox()->GetGPUVirtualAddress();
		        commandList->SetGraphicsRootConstantBufferView(2, cbAddress);
		    }

		    // Skybox 텍스처 바인딩 (슬롯 4)
		    D3D12_CPU_DESCRIPTOR_HANDLE skybox_cpu_handle = ResourceManager::instance()->get_skybox_srv_cpu();
		    if (skybox_cpu_handle.ptr == 0) {
		        skybox_cpu_handle = ResourceManager::instance()->get_texture("__DEFAULT_BLACK__")->cpu_handle;
		    }
		    bind_texture_table(commandList, 4, { skybox_cpu_handle });

		    if (camera) camera->update_shader_variables(commandList, frame_index);

		    for (const auto& gameObject : itSky->second) {
		        auto renderComp = gameObject->get_component<RenderComponent>();
		        if (renderComp && renderComp->is_enabled()) {
		            renderComp->render(commandList, frame_index);
		        }
		    }
		}
    }
}

ID3D12RootSignature* Renderer::get_root_signature(const std::string& name) const
{
    // _rootSignatures 맵에서 'name'을 키로 가지는 원소를 찾습니다.
    auto it = _rootSignatures.find(name);

    // 찾았다면, 해당 원소의 값(ComPtr<ID3D12RootSignature>)에 접근하여
    // .Get() 함수로 실제 포인터를 반환합니다.
    if (it != _rootSignatures.end())
    {
        return it->second.Get();
    }

    // 맵에 해당 이름의 루트 시그니처가 없으면 nullptr을 반환합니다.
    // (또는 에러를 로그로 남기거나 기본값을 반환할 수도 있습니다.)
    return nullptr;
}

ID3D12PipelineState* Renderer::get_pso(const std::string& name) const
{
    auto it = _pipelineStates.find(name);
    if (it != _pipelineStates.end())
    {
        return it->second.Get();
    }
    return nullptr;
}

std::shared_ptr<Shader> Renderer::get_shader(const std::string& name) const
{
    auto it = _shaderPrototypes.find(name);
    if (it != _shaderPrototypes.end())
    {
		return it->second;
    }
    return nullptr;
}

void Renderer::bind_texture_table(ID3D12GraphicsCommandList* command_list, UINT root_parameter_index, const std::vector<D3D12_CPU_DESCRIPTOR_HANDLE>& cpu_handles)
{
    // bind_material에서 만들어진 실제 GPU를 가르키는 포인터가 여기서 root parameter 슬롯에 꽂힌다.

    if (cpu_handles.empty()) return;
    
    UINT num_descriptors = static_cast<UINT>(cpu_handles.size());
    
    // [수정] 현재 프레임의 할당량이 꽉 찼는지 확인
     // 현재 인덱스 + 필요 개수 > (현재 프레임 + 1) * 구획 크기
     // frame_index를 여기서 알기 어려우므로, 간단히 _current_dynamic_descriptor_index가 범위를 넘는지 확인해도 됩니다.

     // 현재 인덱스가 힙 전체 용량을 넘거나, 다음 프레임 구획을 침범하려 하면 에러
     // (간단한 버전: 전체 용량만 체크해도 되지만, 엄격하게 하려면 아래처럼)
    UINT current_frame_start = (_current_dynamic_descriptor_index / _max_descriptors_per_frame) * _max_descriptors_per_frame;
    UINT limit = current_frame_start + _max_descriptors_per_frame;

    if (_current_dynamic_descriptor_index + num_descriptors > limit)
    {
		//TODO: pkj주석 npc 1000명이상일때 동적 디스크립터 힙이 여기서 터짐 -> 해결방법 고민해보기
        CERROR("Dynamic descriptor heap segment for this frame is full! Increase capacity.");
        return;
    }
    
    CD3DX12_CPU_DESCRIPTOR_HANDLE dest_cpu_handle_start(_dynamic_descriptor_heap->GetCPUDescriptorHandleForHeapStart(), _current_dynamic_descriptor_index, _descriptor_size);
    CD3DX12_GPU_DESCRIPTOR_HANDLE dest_gpu_handle_start(_dynamic_descriptor_heap->GetGPUDescriptorHandleForHeapStart(), _current_dynamic_descriptor_index, _descriptor_size);
    
    for (UINT i = 0; i < num_descriptors; ++i)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE dest_handle = dest_cpu_handle_start;
        dest_handle.ptr += i * _descriptor_size;
        _device->CopyDescriptorsSimple(1, dest_handle, cpu_handles[i], D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    }
    
    command_list->SetGraphicsRootDescriptorTable(root_parameter_index, dest_gpu_handle_start);
    
    _current_dynamic_descriptor_index += num_descriptors;
    
}

void Renderer::create_dynamic_descriptor_heap(UINT capacity)
{
    D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
    heap_desc.NumDescriptors = capacity;
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heap_desc.NodeMask = 0; // 명시적으로 설정

    HRESULT hr = _device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&_dynamic_descriptor_heap));
    if (FAILED(hr))
    {
        CERROR("Failed to create dynamic descriptor heap!");
        return;
    }
    _dynamic_descriptor_heap_capacity = capacity;

    _max_descriptors_per_frame = capacity / SWAP_CHAIN_BUFFERS;

    _current_dynamic_descriptor_index = 0;
}

void Renderer::post_initialize(ID3D12Device* device, ID3D12GraphicsCommandList* commandList)
{
    if (_unitCube) {
        _unitCube->upload_to_gpu(device, commandList, 0);
    }
}
