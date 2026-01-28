#include "stdafx.h"

#include "Renderer.h"

#include "Shader.h"
#include "DebugShader.h"
#include "DefaultObjectShader.h"
#include "GlbShader.h"
#include "GltfShader.h"
#include "PlayerShader.h"
#include "GltfHpShader.h"
#include "SkyboxShader.h"
#include "GltfSkinnedShader.h"
#include "TerrainShader.h"
#include "UIShader.h"
#include "MonsterHPUIShader.h"

#include "GameObject.h"
#include "ObjectManager.h"

#include "Camera.h"
#include "CameraComponent.h"
#include "RenderComponent.h"
#include "SkyboxRenderComponent.h"

#include "TerrainLoader.h"

void Renderer::initialize(ID3D12Device* device)
{
    _device = device;
    
    _descriptor_size = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV); 

    create_dynamic_descriptor_heap(100000);

    // [추가] 사용할 루트 시그니처 생성기들을 등록합니다.
    _rootSignatureGenerators.push_back(std::make_unique<DefaultRootSignatureGenerator>());
    _rootSignatureGenerators.push_back(std::make_unique<GltfRootSignatureGenerator>());
    _rootSignatureGenerators.push_back(std::make_unique<GltfHpRootSignatureGenerator>());
    _rootSignatureGenerators.push_back(std::make_unique<SkyBoxRootSignatureGenerator>());
    _rootSignatureGenerators.push_back(std::make_unique<SkinnedRootSignatureGenerator>());
    _rootSignatureGenerators.push_back(std::make_unique<TerrainRootSignatureGenerator>());
    _rootSignatureGenerators.push_back(std::make_unique<UIRootSignatureGenerator>());
	_rootSignatureGenerators.push_back(std::make_unique<MonsterHPUIRootSignatureGenerator>());
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

    auto gltf_hp_shader = std::make_shared<GltfHpShader>();
    _shaderPrototypes[gltf_hp_shader->pso_name()] = gltf_hp_shader;

	auto skybox_shader = std::make_shared<SkyboxShader>();
	_shaderPrototypes[skybox_shader->pso_name()] = skybox_shader;

    auto gltf_animation_shader = std::make_shared<GltfSkinnedShader>();
    _shaderPrototypes[gltf_animation_shader->pso_name()] = gltf_animation_shader;

	auto terrain_shader = std::make_shared<TerrainShader>();
	_shaderPrototypes[terrain_shader->pso_name()] = terrain_shader;

    auto ui_shader = std::make_shared<UIShader>();
    _shaderPrototypes[ui_shader->pso_name()] = ui_shader;

	auto monster_hp_ui_shader = std::make_shared<MonsterHPUIShader>();
	_shaderPrototypes[monster_hp_ui_shader->pso_name()] = monster_hp_ui_shader;

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
    draw_render_list(commandList, camera,  frame_index);
}

void Renderer::build_render_list(CameraComponent* camera)
{
    _renderMap.clear();
    const auto& allGameObjects = ObjectManager::instance()->get_all_game_objects();
    const BoundingFrustum& frustum = camera->frustum();

    int totalObjects = 0;
    int visibleObjects = 0;
    int invalidBoundingBoxCount = 0;
    int uiObjects = 0; 

    for (const auto& gameObject : allGameObjects)
    {
        if (!gameObject || gameObject->is_destroyed()) continue;

        // 스카이박스는 Scene에서 렌더링하므로 제외
        auto skyboxComp = gameObject->get_component<SkyboxRenderComponent>();
        if (skyboxComp) continue;

        auto renderComp = gameObject->get_component<RenderComponent>();

        if (renderComp && renderComp->is_enabled())
        {
            totalObjects++;
            try
            {
                // UI는 bounding box가 없어도 렌더링
                if (renderComp->pso_name() == "ui" || renderComp->pso_name() == "Monster_HP_UI")
                {
                    //CLOG("UI has invalid BB, but adding to render list anyway");
                    _renderMap[renderComp->pso_name()].push_back(gameObject);
                    visibleObjects++;
                    continue;
                }

                BoundingOrientedBox obb = renderComp->get_world_bounding_box();

                if (obb.Extents.x <= 0.0f || std::isnan(obb.Center.x))
                {
                    invalidBoundingBoxCount++;

                    // ========== 여기 수정! ==========
                    // CERROR 대신 CLOG로 변경 (프로그램 멈추지 않음)
                    CLOG("Warning: Invalid bounding box for: " << gameObject->name() << " - skipping");
                    // ================================
                    continue;
                }

                if (renderComp->is_visible(frustum))
                {
                    visibleObjects++;
                    _renderMap[renderComp->pso_name()].push_back(gameObject);
                }
            }
            catch (...)
            {
                CERROR("Exception during frustum culling for: " << gameObject->name());
            }
        }
    }

    /*CLOG("Culling: " << visibleObjects << "/" << totalObjects << " visible, "
        << invalidBoundingBoxCount << " invalid BB");*/
}

void Renderer::draw_render_list(ID3D12GraphicsCommandList* commandList, CameraComponent* camera, UINT frame_index)
{
    // 렌더링에 실제 사용될 '동적 힙'을 커맨드 리스트에 설정합니다
    ID3D12DescriptorHeap* heaps[] = { _dynamic_descriptor_heap.Get() };

    commandList->SetDescriptorHeaps(_countof(heaps), heaps);

    // ---------------------------------------------------------
    for (auto const& [psoName, gameObjects] : _renderMap)
    {
        if (gameObjects.empty()) continue;

        // 1. psoName으로 PSO를 가져옵니다.
        ID3D12PipelineState* pso = get_pso(psoName);
        if (!pso) {
            CERROR("PSO not found for: " << psoName);
            continue;
        }
        //CLOG("PSO found for: " << psoName);

        // 2. psoName으로 이 PSO를 만든 셰이더 프로토타입을 찾습니다.
        auto proto_it = _shaderPrototypes.find(psoName);
        if (proto_it == _shaderPrototypes.end()) 
        {
			CLOG("[Renderer] Shader prototype not found for PSO name: " << psoName);
            continue;
        }
        const auto& shader_prototype = proto_it->second;

        // 3. 찾은 셰이더에게 필요한 루트 시그니처의 '이름'을 물어봅니다.
        const std::string& root_sig_name = shader_prototype->required_root_signature();

        // 4. 그 이름으로 실제 루트 시그니처 '객체'를 가져옵니다.
        ID3D12RootSignature* root_signature = get_root_signature(root_sig_name);
        if (!root_signature) {
            CERROR("Root signature not found for: " << root_sig_name);
            continue;
        }

        // 5. 가져온 PSO와 루트 시그니처를 설정합니다. (이제 if문이 완전히 사라졌습니다!)
        commandList->SetPipelineState(pso);
        commandList->SetGraphicsRootSignature(root_signature);

        // DW수정 : 카메라 중복 설정 제거
        //if (psoName != "ui" && camera)  // UI는 카메라 설정 안 함!
        //{
        //    camera->update_shader_variables(commandList, frame_index);
        //    camera->set_viewports_and_scissor_rects(commandList);
        //}

        if (camera)
        {
            camera->update_shader_variables(commandList, frame_index);
            camera->set_viewports_and_scissor_rects(commandList);
        }

        // 6. 이 PSO 그룹에 속한 모든 오브젝트를 그립니다.
        for (const auto& gameObject : gameObjects)
        {
            auto renderComp = gameObject->get_component<RenderComponent>();
            if (!renderComp) {
                CERROR("No RenderComponent for object: " << gameObject->name());
                continue;
            }

            auto mesh = renderComp->mesh();
            if (!mesh) {
                CERROR("No mesh for object: " << gameObject->name());
                continue;
            }

            // --- [추가] GPU 업로드 확인 및 실행 ---
            /*if (!mesh->is_uploaded())
            {
                // TODO: 현재는 Renderer에서 메쉬의 gpu업로드를 플래그로 한번씩 처리하고 있음(레거시코드처럼 한번에 만들고 업로드 한게 아님)
				// TODO: 추후 ResourceManager에서 메쉬를 관리하게 될 때, ResourceManager에서 한번에 업로드하는 방식으로 변경할 예정
                mesh->upload_to_gpu(_device, commandList);
            }*/
            // ------------------------------------
            
            // 셰이더에게 객체별 리소스 바인딩을 맡김
            shader_prototype->update_per_object(commandList, this, gameObject.get());

            renderComp->render(commandList, frame_index);
        }
    }
    //KJ 설명: OnPrepareRender 함수는 더 이상 필요 없으며, 그 역할은 Renderer가 더 효율적인 방식으로 수행하게 됩니다.
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
        CERROR("Dynamic descriptor heap segment for this frame is full! Increase capacity.");
        return;
    }
    
    CD3DX12_CPU_DESCRIPTOR_HANDLE dest_cpu_handle_start(_dynamic_descriptor_heap->GetCPUDescriptorHandleForHeapStart(),
    _current_dynamic_descriptor_index, _descriptor_size);
    CD3DX12_GPU_DESCRIPTOR_HANDLE dest_gpu_handle_start(_dynamic_descriptor_heap->GetGPUDescriptorHandleForHeapStart(),
    _current_dynamic_descriptor_index, _descriptor_size);
    
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
