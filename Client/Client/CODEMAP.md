# 클라이언트 코드맵 (Client/Client)

DirectX 12 자체 엔진 클라이언트. 이 문서는 실제 소스(2026-10-04 기준)를 보고 작성했다. 코드를 바꾸면 해당 항목을 같이 고친다.
전체 색인은 저장소 루트 `CODEMAP.md`, 서버는 `Server/Server/CODEMAP.md`, 공용은 `Common/CODEMAP.md`.

---

## 1. 빌드·실행 기본

| 항목 | 내용 |
|---|---|
| 솔루션 | `Client/Client.sln`, 프로젝트 `Client.vcxproj` (출력 `Client/x64/<구성>/STL_Client.exe`) |
| 언어·옵션 | `stdcpplatest`, `/utf-8` (소스·문자열 UTF-8). 툴셋 v143(이 PC 기본 MSVC 14.38) |
| 미리 컴파일 헤더 | `stdafx.h` (STL, DX12, `Singleton<T>` 템플릿, `CLOG/CINFO/CERROR` 로그 매크로, `Packet.h` 포함) |
| 컴파일 스위치 (`stdafx.h`) | `_ONDEBUGCONSOLE`(디버그 콘솔 + 로그), `_DEBUG_PHYSICS_VISUALIZATION`(DebugDrawManager 렌더) |
| Jolt | Debug: `Jolt/lib/Debug`, Release: `Jolt/lib/ReleaseDebugRenderer`(디버그 렌더러 포함, MSVC 14.38·LTCG 없음). Release에도 `JPH_DEBUG_RENDERER` 정의 |
| 외부 | FMOD(`Fmod/`), ImGui(`imgui/`), Assimp NuGet(FBX 로더용), DDS/WIC 텍스처 로더(DirectXTK 파생), Lua 5.4.2(`ThirdParty/lua-5.4.2/`, 서버와 같은 빌드를 복사, 빌드 후 `lua54.dll`을 실행 파일 폴더로 복사) |
| 작업 폴더 | `Client/Client`. 리소스·셰이더(`*.hlsl`) 경로가 이 폴더 기준 |
| 인코딩 | `.editorconfig`: C++ 소스는 UTF-8 BOM(`/utf-8`이라 없어도 빌드됨, 맞추려면 `Tools/EnsureUtf8Bom.ps1`), 셰이더는 **BOM 없는** UTF-8(BOM이 있으면 `D3DCompileFromFile`이 "Illegal character"로 실패) |

로그: `CLOG(expr)`는 `DebugLogStream` → VS 출력 창(UTF-16 변환) + 콘솔(UTF-8). `CERROR`는 `DebugBreak()`까지 한다.

---

## 2. 프레임 흐름 (`GameFramework::FrameAdvance`)

1. ImGui 새 프레임, 씬 전환 요청 처리(`SceneManager::process_scene_change_if_requested`)
2. `TimerManager::Tick` → `deltaTime`
3. `ProcessNetwork` → `NetworkManager::process_queued_packets` (패킷 핸들러 실행)
4. `ServerClock::update` (시간 동기화 요청, 서버 시각 추정)
5. `ReplicationSystem::update` → 등록된 `INetSync::apply_snapshot` (NPC 상태 적용)
6. `ProcessInput` (F6/F11 등 전역 키)
7. `update_game_logic`: 새 오브젝트 awake/start → 모든 `GameObject::update` → 모든 `late_update` → `LightManager::update` → 메인 카메라 뷰 행렬 → `DebugDrawManager::Update`
8. `update_physics` (Jolt 고정 스텝 0.02초)
9. 현재 씬 `scene_process`, `SoundManager::update`
10. `PhysicsDebugCapture::process_end_of_frame` (모든 late_update 이후 기록)
11. 비동기 리소스 업로드 → `Renderer::render` → 후처리 훅 → `DamageTextManager`·`ServerClock` ImGui → ImGui 렌더 → Present

---

## 3. 엔티티(게임 오브젝트)·컴포넌트 시스템

Unity와 비슷한 구조: 씬의 모든 것이 `GameObject`이고 기능은 컴포넌트로 붙인다. 매니저류(렌더러, 네트워크, 사운드 등)는 컴포넌트가 아니라 `Singleton<T>`이다.

### 3.1 기반 클래스

| 클래스 | 파일 | 역할 |
|---|---|---|
| `Object` | `Object.h/.cpp` | 이름, 고유 ID(전역 원자 카운터), 파괴 플래그, `persistent`(씬 전환 때 유지). **복사·이동 금지**, 소유는 항상 `shared_ptr`. `Object::destroy(obj)`는 즉시 지우지 않고 `ObjectManager` 파괴 큐에 넣는다(persistent면 무시, `delay` 인자는 미구현) |
| `GameObject` | `GameObject.h/.cpp` | 컴포넌트 목록(`vector<shared_ptr<Component>>`), Transform 바로가기, 레이어 비트 마스크, 활성 플래그. `enable_shared_from_this`. **오브젝트별 시간**: `hit_stop(시간)`, `set_time_scale` → `update`·`late_update`에 deltaTime × 배율(히트스톱 중 0)을 넘김. `fixed_update`는 적용 안 함 |
| `Component` | `Component.h/.cpp` | 소유 GameObject를 `weak_ptr`로 보관(`game_object()`가 lock해서 반환). `required_components` 기본값은 빈 튜플. **갱신 함수가 없다** (`TransformComponent`, `CameraComponent`는 Component라 update가 호출되지 않음) |
| `Behavior` | `Behavior.h/.cpp` | 갱신되는 컴포넌트. 활성 플래그(`set_enabled`가 바뀔 때 `on_enable/on_disable` 호출), 수명 함수 `awake/start/update/late_update/fixed_update/on_destroy` |
| `ScriptComponent` | `ScriptComponent.h/.cpp` | 게임 로직 스크립트 기반(Unity MonoBehaviour 격). `transform()` 편의 함수, `on_message`, `on_collision_enter/stay/exit`, `on_trigger_enter(other, TriggerHit)` |

### 3.2 컴포넌트 추가 규칙 (`GameObject::add_component<T>(args...)`)

1. **같은 타입이 이미 있으면 새로 만들지 않고 기존 것을 반환한다.** `get_component<T>`가 `dynamic_pointer_cast`라 파생 타입도 걸린다. 결과적으로 **한 오브젝트에 같은 타입 컴포넌트는 하나뿐**이다(예: 플레이어의 칼날 캡슐은 플레이어 오브젝트의 유일한 `PhysicsColliderComponent`. 콜라이더가 더 필요하면 자식 오브젝트를 만든다).
2. `T::required_components` 튜플의 컴포넌트를 먼저 재귀적으로 추가한 뒤 `T`를 추가한다. **추가 순서 = 목록 순서 = 갱신 순서**라, 의존 대상이 항상 먼저 갱신된다.
3. `awake`·`update` 순회 중에 `add_component`를 부르면 assert로 막는다(반복자 무효화 방지). 필요한 컴포넌트는 `required_components`에 넣는다.
4. `ObjectManager::create_game_object`가 `init()`에서 `TransformComponent`를 첫 번째로 붙인다.

주요 `required_components`:

| 스크립트 | 자동으로 붙는 컴포넌트 |
|---|---|
| `MainPlayerScript` | Render, Animation, Socket, Targeting, PhysicsCollider |
| `OtherPlayerScript` | Render, Animation, Socket |
| `NPCScript` (Tainer·QuestNPC 상속) | Transform, MonsterHP, Animation, Render (`QuestNPCScript`는 Render, Animation) |
| `LeverScript` | Render, Animation |
| `WeaponScript` | PhysicsCollider |
| `FreeCameraScript`, `ToolCameraScript` | Camera |

### 3.3 수명 주기

| 시점 | 무엇이 일어나나 |
|---|---|
| 생성 | `ObjectManager::create_game_object(이름)`: GameObject 생성 → `init()`(Transform 추가) → 전체 목록과 "새 오브젝트" 큐에 넣음. 이어서 생성한 쪽이 컴포넌트를 붙인다 |
| `awake` → `start` | 다음 `update_game_logic` 시작에서 `process_new_game_objects`: 새 오브젝트 **전부 awake를 먼저**, 그다음 전부 start. 활성 Behavior만 |
| `update` → `late_update` | 매 프레임 `update_game_logic`: 활성 GameObject의 활성 Behavior를 목록 순서대로. 모든 오브젝트의 update가 끝난 뒤 모든 오브젝트의 late_update |
| `fixed_update` | `update_physics`에서 0.02초 누적마다: 모든 오브젝트 fixed_update(Transform → 물리 바디 동기화) → `PhysicsManager::update`(Jolt 스텝 + 접촉 이벤트 전달) |
| 파괴 | `destroy()` → 파괴 큐 → 프레임 끝(Present 뒤) `process_destructions`: 모든 Behavior의 `on_destroy`(비활성이어도 호출) → 부모-자식 연결 해제 → 목록에서 제거. 한 프레임 처리 예산 500ms를 넘으면 다음 프레임으로 미룸. 컴포넌트만 파괴하면 소유 GameObject에서 제거 |
| 씬 전환 | `clear_non_persistent_objects`: persistent가 아닌 오브젝트 전부 파괴 요청 |

- **GameObject의 활성 플래그**(`GameObject::set_enabled`)는 갱신·렌더 대상에서 빼기만 하고 `on_enable/on_disable`을 부르지 않는다. 그 콜백은 `Behavior::set_enabled`에서만 불린다.
- NPC는 씬 시작 때 서버가 알려준 수만큼 **비활성 풀 오브젝트**로 미리 만들고(`HANDLE_S2C_NPC_COUNT`), 스폰 패킷이 오면 활성화하면서 컴포넌트를 붙인다(이미 붙어 있으면 재초기화만).

### 3.4 Transform과 계층

- `TransformComponent`: 로컬 위치·회전(쿼터니언)·스케일, 부모(`weak_ptr`)·자식 목록. 로컬이 바뀌면 자신과 자식 전체를 dirty로 표시하고, `world_matrix()`를 읽을 때 다시 계산한다(지연 계산). `set_world_matrix`로 월드 행렬을 직접 넣을 수도 있다(잡기 부착 등).
- 뼈에 다른 오브젝트를 붙일 때는 Transform 계층 대신 `SocketComponent`가 매 late_update에서 뼈 행렬로 월드 행렬을 계산해 넣는다.

### 3.5 레이어·검색

- `LayerManager`: 레이어 이름 ↔ 비트(`Player`, `OtherPlayer`, `Enemy` 등). `GameObject::set_layer(이름)`, `is_in_layer(이름)`.
- `ObjectManager`: `find_by_name`, `find_by_layer(마스크)`, `find_object(이름/ID)`, NPC id 캐시(`register_npc/find_npc`). 전체 목록은 `vector`라 이름·레이어 검색은 선형 탐색이다.

### 3.6 컴포넌트 상속 관계

```
Object
├─ GameObject
└─ Component                       (갱신 없음)
   ├─ TransformComponent
   ├─ CameraComponent
   └─ Behavior                     (awake/update/late_update/fixed_update)
      ├─ AnimationComponent, SocketComponent, TargetingComponent, MonsterHPComponent
      ├─ PhysicsColliderComponent, PhysicsCharacterControllerComponent
      ├─ ParticleSystemComponent (자동으로 ParticleRenderComponent 부착) → GatherParticleComponent
      ├─ RenderComponent
      │  ├─ InstancedRenderComponent → FoliageRenderComponent
      │  ├─ TerrainRenderComponent, SkyboxRenderComponent, ParticleRenderComponent
      │  ├─ UIRenderComponent → UIFrameRenderComponent
      │  └─ BillboardUIRenderComponent, MonsterHPUIRenderComponent
      └─ ScriptComponent
         ├─ MainPlayerScript, OtherPlayerScript, LeverScript
         ├─ NPCScript (+ INetSync) → TainerScript, QuestNPCScript
         ├─ WeaponScript → LongswordScript
         └─ FreeCameraScript, ToolCameraScript, BoardCubeScript, GltfTestScript
```

### 3.7 렌더러·네트워크와의 연결

- **렌더링**: 컴포넌트가 렌더러에 직접 등록하지 않는다. `Renderer::build_render_list`가 매 프레임 모든 오브젝트의 `RenderComponent`를 찾아 **`pso_name()` 별로 묶고**(`_renderMap`), 거리·프러스텀·오클루전으로 거른 뒤 PSO별로 그린다. 그릴 셰이더는 `RenderComponent::set_pso_name("skinned")`처럼 PSO 이름으로 고른다.
- **네트워크**: 서버가 움직이는 엔티티(NPC)는 `INetSync`를 구현하고 `ReplicationSystem`에 id로 등록한다. 패킷 핸들러 → `on_receive_snapshot`, 매 프레임 → `apply_snapshot`.
- **충돌**: Jolt 접촉 → `PhysicsManager` 큐 → 오브젝트의 첫 `PhysicsColliderComponent::OnContact` → `GameObject::on_collision_enter` → 모든 스크립트. 공격 판정은 `PhysicsColliderComponent::run_hit_query` → `GameObject::on_trigger_enter` → 모든 스크립트.

---

## 4. 씬

| 파일 | 역할 |
|---|---|
| `Scene.h/.cpp` | 씬 기반. `build_objects`, `scene_process`, 씬 파일(JSON) 로드, 폴리지·조명 로드, `render_post_process`(빈 가상 함수) |
| `SceneManager.h/.cpp` | 씬 등록·전환(`SCENE_NUM`), 서버가 지정한 씬 이름, 스카이박스·지형·메인 랜드스케이프·미니맵·풀 생성 |
| `Title_Scene` | 타이틀·리소스 로딩·오프닝 연출, 방 입장 |
| `Main_Scene` | 메인 게임 씬 (UI, 몬스터 HP UI, 레버, 컷씬 시퀀스, 더미 파티클) |
| `Boss_Scene` | 보스전 씬 |
| `Chess_Scene` | 테스트 씬 (더미 NPC, 각종 메쉬 스폰, `Body` 모드 콜라이더 사용 예) |
| `Tool_Scene` | 소켓(무기 부착) 편집 툴 (ImGui, 뼈 선택, 기즈모) |

---

## 5. 플레이어·NPC·게임플레이 스크립트

| 파일 | 역할 |
|---|---|
| `MainPlayerScript.h/.cpp` | 내 플레이어. 입력, 클라 예측 이동(`_logicalPosition` + `_visualOffset` 보정, `sync_with_server`), 이동 패킷 0.02초마다(`send_network_sync`), 평타·대검 스킬 상태, 공격 패킷(평타 30%에 1회), **칼날 캡슐**(`ik_hand_r`, 중심 (0,0,-0.495), 반지름 0.1, `Role::Hitbox`), **평타 예측 판정**(5~80% 구간 `begin/end_hit_query`, `on_trigger_enter`에서 0.5초 쿨다운 흉내·로그·나와 맞은 NPC에 `GameObject::hit_stop` 0.05초, 칼 타격음 `SwordHit`, 맞은 NPC `on_predicted_hit` 피격음, 카메라 킥 `add_kick`), HP/MP/퀘스트 UI, 디버그 키 F7(판정 구간 매 프레임 물리 기록)/F8(물리 기록 + 서버 스냅샷 요청) |
| `OtherPlayerScript.h/.cpp` | 다른 플레이어. `on_server_move`로 이동 패킷을 `SnapshotBuffer`에 쌓아 **렌더 시각(도착 기준 서버 시각 − 30ms)으로 위치·회전 보간**, 상태·액션(공격음)·잡기·HP·MP는 패킷 서버 시각에 적용(`apply_state`). 생성·부활은 `reset_transform`(버퍼 비우고 그 위치에서 시작). 잡기 시 보스 손 뼈에 부착, 파티 슬롯 UI, 대검 스킬 연출 |
| `NPCScript.h/.cpp` | NPC 공통(필수 컴포넌트에 `HitReactionComponent`, 피격 시 피격 모션 대신 리액션). `INetSync` 구현. 스냅샷을 `SnapshotBuffer`에 쌓아 **렌더 시각(도착 기준 서버 시각 − 70ms)으로 위치·회전 보간**, 상태·액션·잡기·HP와 사운드는 스냅샷 서버 시각에 적용(`apply_state`). **넉백 모션**(`on_motion_start/end`, `apply_motion`): 렌더 시각이 모션 구간 안이면 수평 위치를 서버와 같은 곡선으로 계산(높이·회전은 스냅샷). `init_visual`에서 종류별 메쉬·애니메이션 로드(매직 컨스트럭트 1.5배, 드래곤 브루트 Hit 모션), 상태별 애니메이션 분기 |
| `TainerScript.h/.cpp` | 보스(본 골렘, 5배 스케일). 액션 번호별 애니메이션·사운드, HP 바, 사망 엔딩 연출, BT 디버그 정보 |
| `QuestNPCScript.h/.cpp` | 퀘스트 NPC (상호작용 F UI, 퀘스트 마커). 이동 보간 안 함 |
| `LeverScript.h/.cpp` | 레버 상호작용과 UI |
| `WeaponScript.h/.cpp` | 무기 정보·공격 활성 플래그·스킬 차지·쿨다운, `LongswordScript`(같은 파일). `LongswordScript::awake`가 소켓 오브젝트 `MainWeapon`(10배 대검, 화면엔 파티클)의 콜라이더를 **대검 스킬 판정 캡슐**(QueryOnly Hitbox, 반지름 0.5m, 길이 약 10m, 로컬 중심 (0, 0.35, 0))로 설정. 판정은 `MainPlayerScript`가 켜고 적중도 받음 |
| `TargetingComponent.h/.cpp` | 락온 대상 선정·토글 |
| `FreeCameraScript.h/.cpp` | 플레이어 추적 카메라·자유 카메라, 화면 흔들기(`add_trauma` 무작위 떨림, `add_kick` 정해진 방향으로 한 번 튐), 동적 줌 오프셋, 시네마틱 모드 |
| `ToolCameraScript.h/.cpp` | 툴 씬 카메라 |
| `MonsterHPComponent.h/.cpp` | 몬스터 HP 값·비율 |
| `BoardCubeScript`, `GltfTestScript` | 테스트용 스크립트 |

---

## 6. 애니메이션·메쉬·리소스

| 파일 | 역할 |
|---|---|
| `AnimationComponent.h/.cpp` | 애니메이션 별칭 → (메쉬, 실제 이름) 매핑, `play/play_until_progress`, 진행도, **캐릭터별 뼈 자세 보관**(`try_get_bone_model/world_matrix`, 공용 메쉬 노드에 의존하지 않음), 스키닝 팔레트(`bone_palette`), 조인트 추가 회전(`set_joint_offsets`, 키프레임 자세 위에 더함) |
| `HitReactionComponent.h/.cpp` | **부위 피격 리액션.** `react(TriggerHit)`: NPC 메쉬를 현재 자세로 CPU 스키닝해 칼날 캡슐이 쓸고 간 구간에 닿은 정점을 찾고, 정점 뼈 가중치 비율대로 여러 뼈(다리 포함, 정점 없는 뼈·루트·골반 제외) + 부모 2개를 칼 방향으로 꺾고 감쇠 스프링으로 돌아옴. 세기·복귀 속도는 캐릭터별 `set_settings`(각 `init_visual`에서 종류별로 설정). 로그 `[HitReaction]`. 오브젝트 시간으로 진행해 히트스톱 중엔 꺾인 채 멈춤. `NPCScript`의 필수 컴포넌트 |
| `SocketComponenet.h/.cpp` (파일명 오타 그대로) | 뼈에 다른 오브젝트 부착(`add_connecting`), 애니메이션 따라가기 토글. late_update에서 같은 프레임 자세를 읽음 |
| `Mesh.h/.cpp` | 메쉬 기반(정점·인덱스 업로드, 렌더, 인스턴싱, CSM 그림자 렌더, OBB), 디버그 메쉬 |
| `ReadGLTFMesh.h/.cpp` | **주력 로더.** glTF 정적/스킨 메쉬, 스킨·애니메이션 채널, 노드 계층, `update_animation`(팔레트 + 조인트 모델 행렬 출력, 조인트 추가 회전 `JointRotationOffset` 적용 후 공용 노드 복구), 소켓 행렬, 조인트 인덱스·이름·부모, `joint_moves_skin`(정점 가중치 기준), 프리미티브 형상·CPU 스키닝 형상(디버그 기록·피격 리액션, 정점별 뼈 가중치 `SkinInfluence` 선택 출력), 파티클 목표점 추출. 엔진이 glTF의 Z를 뒤집어 읽음(좌표계 변환) |
| `ReadGLBMesh`, `ReadFBXMesh`(Assimp), `ReadOBJMesh` | 보조 로더 |
| `SkyboxMesh`, `TerrainLoader.h/.cpp` | 스카이박스, 지형(하이트맵, 레이어 텍스처 배열, 풀 배치 가중치) |
| `ResourceManager.h/.cpp` | 메쉬·텍스처·머티리얼 캐시, 업로드 버퍼 수명, IBL 맵, 스카이박스, R8 텍스처 배열, 기본 텍스처 |
| `DescriptorManager`, `LinearAllocator` | 디스크립터 힙 할당, 프레임별 256바이트 정렬 상수 버퍼 선형 할당 |

---

## 7. 렌더링

| 파일 | 역할 |
|---|---|
| `Renderer.h/.cpp` | 루트 시그니처·PSO 생성, 렌더 목록 구성(`build_render_list`: 정적/동적, 프러스텀·오클루전), PSO별 그리기, 스카이박스·파티클·UI 단계, 통계. 파티클 PSO 3개(`particle_draw`, `particle_alpha`, `particle_additive`, 이 순서로 그림)는 `render_particle_group`(그룹의 컴퓨트를 먼저 몰아서 → 그래픽 상태 한 번 복구 → 그리기, 두 렌더 경로 공용), 컴퓨트 PSO 공유 `get_or_create_compute_pso` |
| `Shader.h/.cpp` + 각 `*Shader` | PSO 설정 단위(입력 레이아웃, 셰이더 파일, 블렌드·깊이·래스터 상태, 객체별 상수). 셰이더 클래스 ↔ hlsl: `GltfShader`→`Gltf_Shader.hlsl`, `GltfSkinnedShader`→`Gltf_Skinned_Shader.hlsl`, `GlbShader`→`GLB_Shader.hlsl`, `TerrainShader`, `SkyboxShader`, `ShadowDepth(Skinned)Shader`, `UIShader`, `UIFrameShader`, `BillboardUIShader`, `MonsterHPUIShader`, `MinimapShader`, `OcclusionQueryShader`, `DebugShader`, `ParticleShader`→`Particle_Draw.hlsl`, `DefaultObjectShader`/`PlayerShader`→`Shaders.hlsl` |
| `RootSignature.h/.cpp` | 이름별 루트 시그니처 생성기 (gltf, skinned, terrain, ui, debug, csm, minimap, occlusion, `compute_particle`, `particle_draw`, 범용 파티클 `particle_compute`·`particle_billboard` 등) |
| `RenderComponent.h/.cpp` | 메쉬 + PSO 이름 + 객체 상수 버퍼, 컬링 상태. 파생: `InstancedRenderComponent`, `FoliageRenderComponent`, `TerrainRenderComponent`, `SkyboxRenderComponent`, `UIRenderComponent`, `UIFrameRenderComponent`, `BillboardUIRenderComponent`, `MonsterHPUIRenderComponent`, `ParticleRenderComponent` |
| `ShadowManager.h/.cpp` | 캐스케이드 그림자(CSM), 정적 그림자 갱신 조건 |
| `LightManager.h/.cpp` | 조명 상수 버퍼, 태양 방향, IBL 구면 조화 |
| `OcclusionManager.h/.cpp` | 오클루전 쿼리 힙·결과(N-1 프레임 결과로 조건부 렌더) |
| `MinimapManager.h/.cpp` | 미니맵 타일·플레이어 위치 |
| `CameraComponent.h/.cpp` | 투영·뷰 행렬, 카메라 상수, 흔들기 오프셋·회전(`set_shake_angle`), 메인 카메라 |
| `ParticleSystemComponent.h/.cpp` + `ParticleSystemSettings.h` | **범용 GPU 파티클**(유니티 ParticleSystem에 해당). 붙이면 `ParticleRenderComponent` 자동 부착. 설정 `ParticleSystemSettings`(유니티 모듈 이름: 수명·속도·크기·회전 범위, 두 색 무작위, 중력·저항, `max_particles`, 초당 방출·버스트, 모양 Point/Sphere/Hemisphere/Cone/Edge, 수명별 색·크기, 블렌딩 가산/알파, 일반/속도 늘림). `play/stop/clear/emit`. CPU는 방출 요청만 만들고(링 버퍼 쓰기 위치·살아 있는 수는 묶음별 최대 수명으로 추정, 꽉 차면 방출 안 함), `dispatch_compute`가 갱신 → 방출 컴퓨트, `draw`가 빌보드. 방출 위치·방향은 오브젝트 위치·정면(+Z), 시간은 오브젝트 시간 |
| `Particle_Common.hlsli`, `Particle_Emit_CS.hlsl`, `Particle_Update_CS.hlsl`, `Particle_Billboard.hlsl` | 범용 파티클 셰이더: 공용 구조체(파티클·방출 요청 48바이트, 방출기 상수)와 PCG 난수, 방출(요청 찾기·모양별 초기값), 갱신(중력·저항·나이, `clear`), 빌보드(죽은 것은 화면 밖, 속도 늘림, 부드러운 원). **BOM 없이 저장** |
| `GatherParticleComponent.h/.cpp` + `Particle_CS.hlsl` | 목표점으로 모이는 파티클(대검 스킬, 컷씬 플레이어, 분수). 위치를 진행도(`set_compute_data`)로부터 컴퓨트가 직접 계산. 루트 상수 24개(파티클 수 포함), 컴퓨트 PSO는 렌더러가 공유 |
| `ParticleRenderComponent.h/.cpp` | 파티클을 렌더 목록에 올리는 렌더 컴포넌트. 같은 오브젝트의 파티클 컴포넌트 `draw`를 부름 |
| `ParticleBillboardShader.h/.cpp` | 범용 파티클 PSO `particle_additive`(가산)·`particle_alpha`(알파). 깊이 테스트 O, 쓰기 X, 컬링 없음 |
| `UIManager`, `DamageTextManager`, `ImGuiManager` | UI 레이어·파티 슬롯, 데미지 숫자(ImGui), ImGui 수명 |
| `DebugDrawManager.h/.cpp` | 디버그 도형(박스·구·캡슐·선), 서버가 보낸 디버그 도형. `_DEBUG_PHYSICS_VISUALIZATION`일 때 렌더 |

렌더 타깃: 씬을 스왑체인 백버퍼에 바로 그린다(오프스크린 씬 타깃 없음). 화면 후처리를 하려면 이 구조부터 바꿔야 한다.

---

## 8. 물리 (Jolt)

| 파일 | 역할 |
|---|---|
| `PhysicsManager.h/.cpp` | Jolt 초기화, 0.02초 고정 스텝, 접촉 이벤트 큐 → 메인 스레드에서 `PhysicsColliderComponent::OnContact`(첫 번째 콜라이더만), 지형 하이트필드 생성 |
| `JoltSetup.h` | 브로드페이즈·오브젝트 레이어 필터 |
| `PhysicsColliderComponent.h/.cpp` | 박스·구·캡슐 콜라이더. `BodyMode::Body`(Jolt 바디) / `QueryOnly`(바디 없이 모양·월드 변환만). **뼈 부착**(`attach_to_bone`), 스케일 무시(`set_ignore_owner_scale`), **역할**(`Role::Hitbox` 공격 / `Hurtbox` 피격, Hurtbox는 정적 목록 등록), **공격 판정**(`begin/end_hit_query`: late_update에서 직전→현재 자세를 15도·0.2m 단위로 나눠 `CollisionDispatch::sCollideShapeVsShape`, 대상마다 1회 `on_trigger_enter`, `set_hit_receiver`로 받을 오브젝트 지정 가능. 캡슐이면 `TriggerHit`에 쓸고 간 구간 기록) |
| `PhysicsCharacterControllerComponent.h/.cpp` | 캐릭터 컨트롤러 (fixed_update) |
| `PhysicsDebugCapture.h/.cpp` | JoltViewer용 기록(`client_physics_dump.bin`): Jolt 바디, QueryOnly 콜라이더(공격 초록, 피격 하늘색, 적중 빨강), 뼈 축, 기준 프리미티브(칼날), 적중 지점·방향, NPC 실제 메쉬(CPU 스키닝, 15m 이내). 첫 기록 시 viewer `-focus` 명령을 로그로 출력 |

NPC 피격 히트박스는 `NetworkManager.cpp`의 `attach_npc_hurtbox`가 NPC 생성 시 서버 값과 같게 붙인다(일반 NPC 캡슐 r0.5·키1.8·발 위 0.9m, 보스 r3·키8·발 위 4m, DynamicBox 1m 박스).

---

## 9. 네트워크·동기화

| 파일 | 역할 |
|---|---|
| `NetworkManager.h/.cpp` | 블로킹 소켓 + 수신 전용 스레드(`network_worker` → 패킷 조립 → `concurrent_queue`, 수신 시각 기록). 메인 스레드 `process_queued_packets`에서 핸들러 실행. **인공 수신 지연·흔들림**(순서 유지). 송신 함수 `Send*Packet`, 수신 핸들러 `HANDLE_S2C_*`(로그인, 방, 플레이어 스폰·이동·피격, NPC 스폰·이동·배치·피격, 퀘스트, 인벤토리, 컷씬, 카운트다운, 시간 동기화, 모션 시작·종료 등) |
| `ServerClock.h/.cpp` | 서버 시각 추정. TIME_SYNC 왕복(로그인 직후 0.1초×5, 이후 2초), RTT 최소 표본 기준 offset, 초당 5ms 이내로 따라감. `server_now_ms`, `arrival_now_ms`(− RTT/2), `unwrap_server_time`(32비트 서버 시각 복원), 보간 지연 상수(NPC 70, 플레이어 30). **F6 창**: RTT, offset, 보간 상태 집계(NPC·플레이어 따로), 서버 위치 유령(NPC·다른 플레이어), 인공 지연 슬라이더 |
| `SnapshotBuffer.h/.cpp` | 서버 시각 표본 버퍼. 에르미트 위치 보간 + slerp, 긴 간격은 마지막 100ms만, 최대 100ms 외삽, 5m 이상 순간이동 |
| `ReplicationSystem.h/.cpp`, `INetSync.h` | 엔티티 id → `INetSync` 등록, 스냅샷 전달(`on_receive_snapshot`), 매 프레임 `apply_snapshot` |

---

## 10. 기타 매니저·유틸

| 파일 | 역할 |
|---|---|
| `InputManager` | 키 상태(Down/Up/Press), 마우스 델타·고정 |
| `TimerManager` | 프레임 시간. `SetHitStop`·`_gameTimeScale`은 사용처 없음(정리 대상) |
| `SoundManager` | FMOD, 2D/3D 재생, 구간 재생(`play_3d_section`), 그룹 볼륨 |
| `main.cpp` | `wWinMain`, 디버그 콘솔 할당(UTF-8 코드페이지), 창 생성, `GameFramework::OnCreate`, 서버 주소 설정 |
| `d3dx12.h`, `DDSTextureLoader12`, `WICTextureLoader12` | 외부 헬퍼 |
| `BehaviorTree.h` | 클라이언트 쪽 BT 사본. 사용처 없음 |
| `generate_*.py` | UI 이미지 생성 스크립트 |
| `LuaUtil.h/.cpp` | Lua 데이터 파일 읽기(`LuaState::do_file`, `LuaTable`: 숫자·색·범위·배열, 없으면 기본값, 타입 오류·모르는 키 로그, `lua_for_each_global_table`). 이펙트 프리셋용 |

**사용하지 않는 파일(내용이 전부 주석이거나 비어 있음)**: `Camera.h/.cpp`(Renderer.cpp가 include만), `FreeCamera.h/.cpp`, `ColiderComponent.h/.cpp`, `LongswordScript.h/.cpp`(실제 클래스는 `WeaponScript.h`), `BehaviorTree.cpp`.

---

## 11. 디버그 키

| 키 | 동작 | 위치 |
|---|---|---|
| F6 | 네트워크 창 토글 | `GameFramework::ProcessInput` |
| F7 | 평타 판정 구간 매 프레임 물리 기록 토글 | `MainPlayerScript::handle_input` |
| F8 | 클라 물리 한 프레임 기록 + 서버 물리 스냅샷 요청 | `MainPlayerScript::handle_input` |
| F11 | 전체 화면 | `GameFramework::ProcessInput` |
| K | 300m 안 몬스터 즉사 (서버 디버그 명령) | `MainPlayerScript::handle_input` |
| U | 대검 스킬 즉시 해금 (서버 디버그 명령 `UNLOCK_SKILL`) | `MainPlayerScript::handle_input` |

기록 파일은 `Jolt/JoltViewer/RunViewer.py`로 연다.

---

## 12. 진행 중 작업 (관련 문서)

- 타격감: `기획 & 계획/HitFeel_Plan_KR.md`, `BoneCollider_Spec_KR.md`(1단계), `MeleeHitPrediction_Spec_KR.md`(2단계)
- 이동·넉백 동기화: `기획 & 계획/NetMotionSync_Design_KR.md` (S1~S3 완료, 다음 S4 넉백 모션 이벤트)
- 파티클: `기획 & 계획/ParticleSystem_Plan_KR.md`
