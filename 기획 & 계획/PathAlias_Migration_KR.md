# 경로 별칭·매니페스트 전환 조사와 구현 계획 (PathManager 2차 작업)

작성: 2026-10-06. 기준 브랜치 `temp/path-manager` (`bf195ba23`, PathManager 1차 작업 포함).
목적: 경로 문자열을 별칭(`UI:HP_Bar.dds`)으로 바꾸고, 배포 프로그램이 매니페스트만 읽고 필요한 폴더를 복사하게 만든다. 이 문서는 **바꿔야 할 곳 전체 조사**와 **전환 방식**을 정리한다. 아직 코드는 바꾸지 않았다.

---

## 1. 현재 상태 (1차 작업에서 된 것)

- `Common/PathManager.h`
  - exe 위치를 보고 배포/개발 모드를 판별한다.
  - 루트 6개(`App`, `Shader`, `Lua`, `ClientResource`, `CommonData`, `Saved`)를 계산한다.
- 파일을 실제로 여는 곳은 이미 전부 `PathManager::ResolveApp` / `Resolve`를 거친다(4.2 표).
  - 그래서 **별칭 해석을 PathManager 한 곳에 넣으면, 호출하는 쪽 문자열만 바꿔서 하나씩 옮길 수 있다.**
- 문제점: 배포 모드 판단이 "exe 옆에 `Resource/`, `Shaders/`(서버는 `Lua/`)가 있으면 배포"라는 추측이다. 서버 `x64/Release`처럼 손으로 복사해 둔 폴더가 있으면 잘못 판정할 수 있다.

## 2. 결정 사항

| 항목 | 결정 |
|---|---|
| 기본 모드 | **개발 모드.** 저장소 원본을 읽는다 |
| 배포 모드 판단 | 배포 프로그램이 exe 옆에 `Deploy.json`을 쓴다. 그 파일이 있을 때만 배포 모드. 루트 위치도 이 파일에서 읽는다(배포 폴더 구조를 C++에 하드코딩하지 않음) |
| 경로 목록 | 저장소 루트의 `PathManifest.json`. 게임(PathManager)과 배포 프로그램이 같은 파일을 읽는다 |
| 별칭 단위 | **폴더 단위.** 별칭 하나 = 폴더 하나. 배포는 별칭 폴더를 통째로 복사(데이터 파일이 참조하는 `.bin`, 텍스처, `Meshes/`도 같이 따라옴) |
| 코드 표기 | `"별칭:폴더 안 경로"`. 예: `"UI:HP_Bar.dds"`, `"DarkKnight:DKF_animations/Anim_DKF_Death.gltf"` |
| 전환 방식 | **점진적.** 기존 `"Resource/..."` 표기와 별칭 표기를 둘 다 받는다. 새 코드는 별칭으로 쓰고, 기존 파일은 손댈 때 바꾼다 |
| 사용 파일 기록 | 개발 모드에서 PathManager로 연 파일을 전부 기록해 `Saved/used_files.txt`로 남긴다. 배포 전에 "매니페스트에 없는 폴더에서 읽은 파일"을 검사하는 데 쓴다 |

### 2.1 `PathManifest.json` 모양 (안)

```json
{
  "aliases": {
    "UI":         { "dev": "Client/Client/Resource/UI",                   "apps": ["client"] },
    "Sound":      { "dev": "Client/Client/Resource/Sound",                "apps": ["client"] },
    "BossMap":    { "dev": "Client/Client/Resource/1-BossScene",          "apps": ["client", "server"] },
    "Lua":        { "dev": "Server/Server/Lua",                           "apps": ["server"] }
  },
  "exclude": ["*.psd", "*.blend"]
}
```

- `dev`: 저장소 루트 기준 경로.
- 배포 위치는 개발 폴더의 상대 위치를 그대로 따른다(6.0의 1번). App 밖 별칭만 `<exe 폴더>/External/<별칭>/`이다. 배포 프로그램이 `Deploy.json`에 별칭 → 상대 경로를 기록한다.
- `apps`: 그 별칭을 어느 배포본에 넣을지. 서버는 클라 리소스 중 `apps`에 `server`가 있는 것만 받는다(1차 작업의 `Data/ClientResource` 사본을 대신함).

### 2.2 기존 표기와 공존시키는 규칙 (중요)

별칭 표기와 기존 표기가 섞이면 다음 두 가지가 깨진다. 그래서 PathManager에 `Expand`(논리 경로로 펼치기)를 두고, 경로를 받는 함수 입구에서 먼저 펼친다.

1. **캐시 키가 둘로 갈라진다.**
   `_textures`, `_meshes`는 경로 문자열이 키다. `"UI:HP_Bar.dds"`와 `"Resource/UI/HP_Bar.dds"`가 다른 키가 되면 같은 파일을 두 번 올린다.
   → 키는 항상 `Expand` 결과(기존 `"Resource/..."` 형태)로 통일한다.
2. **부모 폴더 기준 경로 계산이 깨진다.**
   `parent_path()`로 옆 파일을 찾는 코드(4.3 표)는 `"BossMap:x.json"`의 부모를 구할 수 없다.
   → 이런 함수도 입구에서 `Expand`한 경로를 쓴다.

```cpp
// PathManager 추가 함수 (안)
static std::string Expand(std::string_view path);
// "UI:HP_Bar.dds"         -> "Resource/UI/HP_Bar.dds"  (개발/배포 공통의 논리 경로)
// "Resource/UI/HP_Bar.dds" -> 그대로
// ResolveApp은 내부에서 Expand 후 루트를 붙인다
```

서버에서 클라 리소스를 가리키는 별칭(`BossMap` 등)은 App 기준 논리 경로가 없다. 그래서 `Expand`가 절대 경로를 돌려준다(서버는 캐시 키 문제가 없다).

---

## 3. 별칭 제안표

"코드 사용" 열은 주석이 아닌 코드에서 해당 폴더 경로가 나오는 횟수다. 접두 변수로 조합하는 경우는 변수 1개를 1회로 셌다.

### 3.1 클라이언트

| 별칭(안) | 개발 경로 (`Client/Client/` 기준) | 코드 사용 | 크기 | 비고 |
|---|---|---|---|---|
| `UI` | `Resource/UI` | 91 + `ID/` 11 | 63M | `Player_N.dds`는 문자열 조합(4.5). `UI/default_png`(21M)는 코드 참조 없음 |
| `Sound` | `Resource/Sound` | 27 | 98M | `SoundManager::load_sound` 경유 |
| `SkyBox` | `Resource/SkyBox` | 8 + `build_skybox` 5곳 | 241M | 4.5 참고. `star/`는 코드 참조 없음 |
| `DarkKnight` | `Resource/Character/DarkKnight` | 17 + 접두 변수 6 | | 플레이어. 애니메이션은 `DKF_animations/` |
| `DarkKnightNoSword` | `Resource/Character/DarkKnightNoneSword` | 3 | | |
| `BoneGolem` | `Resource/Character/BoneGolem` | 7 + 접두 변수 1 | | Tainer 보스 |
| `MagicConstruct` | `Resource/Character/SK_MagicConstruct` | 14 | | |
| `DragonBrute` | `Resource/Character/DragonBrute` | 7 | | |
| `BruteHi` | `Resource/Character/BruteHi` | 3 | | |
| `BruteAnim` | `Resource/Character/Brute_Attack_animation`, `Brute_Walk`, `Brute_idle` | 2 + 2 + 2 | | 별칭 하나에 폴더 여러 개를 묶을지, 각각 별칭을 줄지 결정 필요 |
| `Bandit` | `Resource/Character/Bandit_Rd_NPC` | 3 | | |
| `Gramma` | `Resource/Character/Gramma_Walk` | 1 | | |
| `Weapons` | `Resource/Weapons` | 6 | 7.1M | |
| `Lever` | `Resource/Lever` | 4 + `Animation/` 2 | 11M | |
| `LeverAndPosition` | `Resource/LeverAndPosition` | 3 | 11M | |
| `Elevator` | `Resource/Elevator` | 3 | 24M | |
| `Foliage` | `Resource/Foliage` | 2 + 1 | 13M | 실제 로드는 `Foliage_tree_-1_0_MapData`뿐 |
| `BossMap` | `Resource/1-BossScene` | 1 | 165M | **서버 공용** |
| `LandscapeMeshes` | `Resource/MainLandscape_Meshes` | 6 | 2.9G | **서버 공용**(`-1_-1`, `-1_0`만). 서버만 일부를 쓰므로 하위 폴더별 별칭도 고려 |
| `MainLandscape` | `Resource/MainLandscape` | 1 + `SharedTextures/` 접두 변수 1 | 142M | **서버 공용**. `Landscape01~05`는 `directory_iterator`로 전부 읽음 |
| `HeightMap` | `Resource/HeightMap` | 2 | 155M | 체스 씬 지형 텍스처 |
| `ChessMap` | `Resource/MD` | 1 | 207M | 체스 씬 JSON |
| `Shaders` | `Shaders` | 34 (셰이더 클래스 18개 파일 + 파티클) | | 이미 `Resolve(PathRoot::Shader, ...)`. 별칭 전환은 선택 |
| `CommonMapData` | `../../Common/MapData` (저장소 `Common/MapData`) | 1 | 977K | 체스 씬 하이트맵 |
| `WorldBatch` | 저장소 `Common/World_Batch_glTF` | 1 (클라 디버그 도형) | 107M | **서버 공용**. 실제 사용은 `Tile_X-1_Y-1/`뿐 |

### 3.2 서버

| 별칭(안) | 개발 경로 | 코드 사용 | 비고 |
|---|---|---|---|
| `Lua` | `Server/Server/Lua` | 4 (+ `AIComponent::SetLuaScript`, 호출처 없음) | |
| `NavMesh` | `Server/Server/Resource` | 1 | `NavMesh2.obj`만 사용(`NavMesh.obj`는 코드 참조 없음) |
| `BossMap`, `LandscapeMeshes`, `MainLandscape`, `WorldBatch` | 클라 별칭과 같음 | 각 1~2 | `server.cpp` 시작부 |

### 3.3 저장 위치 (별칭 아님, 루트 유지)

| 루트 | 쓰는 곳 |
|---|---|
| `Saved` | 클라 `imgui.ini`(`ImGuiManager`), `client_physics_dump.bin`(`PhysicsDebugCapture`), 서버 `physics_dump.bin`(`Room`), 앞으로 `used_files.txt` |

### 3.4 코드에서 참조하지 않는 폴더 (배포 제외 후보)

주석이 아닌 코드에 경로가 없는 폴더다. 데이터 파일(씬 JSON 등)에서 참조하는지는 3.5 범위까지만 확인했다.

| 폴더 | 크기 | 비고 |
|---|---|---|
| `Resource/Test`, `Test_glTF`, `TESTMapData` | 65M, 31M, 52K | 테스트용 |
| `Resource/Monster`, `Resource/Default` | 256K, 4K | |
| `Resource/Character/DDSMapData` | 327M | `ExportedClientData.json`이 있지만 로드하는 코드 없음 |
| `Resource/Character/` 의 춤·사망 애니 폴더 8개 (`Animation_BruteHi`, `BruteDance`, `Brute_*_Dance` 5개, `Brute_die`) | 약 250M | `Animation_BruteHi`, `BruteDance`는 주석 코드에만 있음 |
| `Resource/Character/Character.obj/.mtl/.dds`, `test_mesh.obj` | 약 20M | |
| `Resource/UI/default_png` | 21M | |
| `Resource/SkyBox/star` | 65M | |
| `Resource/Foliage/` 중 `Foliage_tree_-1_0_MapData` 외 4개 | 약 10M | |
| `Server/Server/Resource/NavMesh.obj` | | |

### 3.5 데이터 파일이 다른 파일을 부르는 구조 (폴더째 복사해야 하는 이유)

| 데이터 파일 | 참조 방식 | 읽는 곳 |
|---|---|---|
| 씬 JSON (`*_ExportedClientData.json`) | 같은 폴더의 `Meshes/...` (확인한 7개 모두 자기 폴더 안) | `Scene::load_scene_from_file`, 서버 `MapDataManager::LoadStaticMeshShapes` |
| 폴리지 JSON | 같은 폴더 기준 메쉬 | `Scene::load_foliage_from_file` |
| glTF | 옆의 `.bin`, 이미지 URI | `ReadGLTFMesh::load_gltf_file`, `ResourceManager::load_materials_from_gltf`, 서버 `glTFMeshLoader` |
| OBJ | 옆의 `.mtl` | `ReadOBJMesh::LoadMtlFile` |
| 지형 `metadata.json` / `Heightmap.json` | 같은 폴더의 raw·weightmap | `common::TerrainData::LoadFromJSON`, `TerrainLoader` |
| 서버 `.jbin` 캐시 | 원본 메쉬 옆에 **쓰기** | `MapDataManager` (배포 서버 폴더에 쓰기 권한 필요) |

---

## 4. 바꿔야 하는 곳 전체

### 4.1 PathManager 자체 (`Common/PathManager.h`)

| 할 일 | 내용 |
|---|---|
| 매니페스트 읽기 | `Init`에서 개발 모드면 저장소 `PathManifest.json`, 배포 모드면 exe 옆 `Deploy.json`을 읽어 별칭 → 실제 폴더 표를 만든다 |
| 모드 판단 교체 | `Resource/`·`Shaders/`·`Lua/` 존재 검사 → `Deploy.json` 존재 검사 |
| `Expand(path)` 추가 | `별칭:나머지`를 논리 경로로 펼친다. 별칭이 없으면 그대로 둔다. 모르는 별칭이면 에러 로그 |
| `ResolveApp` 수정 | 내부에서 `Expand`를 먼저 하고 루트를 붙인다 |
| 사용 파일 기록 | 개발 모드에서 `Resolve`할 때마다 실제 경로를 집합에 넣고, 종료 시 `Saved/used_files.txt`에 쓴다. 멀티스레드 로드가 있으면 뮤텍스 필요 |
| 고정 루트 정리 | `ClientResource`, `CommonData`는 별칭으로 대체되면 제거. `Shader`, `Lua`, `Saved`는 별칭 또는 루트로 유지 |

### 4.2 경로를 받는 함수 (입구에서 `Expand` 적용)

1차 작업에서 이미 여는 경로를 `Resolve`한다. 이번에는 입구에서 `Expand`를 해서 캐시 키와 파생 경로를 통일해야 한다.

| 함수 | 1차 작업 상태 | 이번에 할 일 |
|---|---|---|
| `ResourceManager::load_mesh` | 리더 안에서 Resolve | 입구에서 `Expand`. 키 `_meshes[expanded]` |
| `ResourceManager::load_texture` | Resolve | 입구에서 `Expand`. 키·`TextureInfo::name` 통일 |
| `ResourceManager::load_materials_from_gltf` | Resolve | 입구에서 `Expand`(이미지 URI 기준 폴더 계산에 씀) |
| `ResourceManager::load_cubemap_from_dds`, `load_skybox`, `load_ibl_maps` | Resolve | 입구에서 `Expand`. IBL 경로 멤버(`_ibl_*_path`)도 펼친 값 저장 |
| `ResourceManager::load_heightmap_from_raw`, R8 텍스처, 텍스처 배열 | Resolve | 입구에서 `Expand` |
| `ResourceManager::get_texture` | 키 조회만 | 인자를 `Expand` 후 조회 |
| `ReadGLTFMesh` 생성자, `load_animation_only`, `load_gltf_file` | Resolve | `load_animation_only` 입구에서 `Expand`(로그·애니 이름용) |
| `ReadGlbMesh`, `ReadOBJMesh`, `ReadFBXMesh` 생성자 | Resolve | `set_name`에 펼친 경로를 쓰도록 `load_mesh`에서 넘겨줌 |
| `Scene::load_scene_from_file`, `load_foliage_from_file`, `load_from_file_with_light` | 여는 것만 Resolve | 입구에서 `Expand`. `basePath = parent_path()`가 펼친 경로를 쓰게 |
| `SoundManager::load_sound` | Resolve | 변경 없음(키가 이름이라 문제없음) |
| `UIRenderComponent::set_texture`, `BillboardUIRenderComponent::set_texture` | `load_texture` 경유 | 저장하는 텍스처 경로 문자열이 있으면 `Expand` |
| `SceneManager::build_skybox` | 인자를 이어 붙여 넘김 | 4.5 참고 |
| `SceneManager::build_terrain`, `build_main_landscapes` | Resolve | 별칭으로 교체 |
| `TerrainLoader` 생성자, `load_textures_to_resource_manager`, weightmap 로드 | Resolve | 입구에서 `Expand` |
| `DebugDrawManager::LoadLocalDebugShape` | Resolve | 변경 없음 |
| `Shader::compile_shader_from_file`, `ParticleSystemComponent` | `Resolve(Shader)` | 별칭을 쓸 경우에만 변경 |
| 서버 `MapDataManager::LoadStaticMeshShapes`, `LoadServerExportData`, `LoadMainLandscapeData`, `LoadNavMesh` | 호출하는 쪽에서 Resolve | 함수 안에서 `Resolve`로 옮기고, 호출하는 쪽은 별칭 문자열만 넘김 |
| 서버 `LuaManager` 4곳, `AIComponent::SetLuaScript` | `Resolve(Lua)` | 별칭 `Lua:`로 통일(선택) |

### 4.3 부모 폴더 기준으로 경로를 만드는 곳 (`Expand` 필수)

| 위치 | 코드 |
|---|---|
| `Scene::load_scene_from_file`, `load_foliage_from_file`, `load_from_file_with_light` | `basePath = path(filename).parent_path()` → `basePath / meshFile` |
| `ResourceManager::load_materials_from_gltf` | `base_path = path(file_path).parent_path()` → 이미지 URI |
| `ReadOBJMesh::LoadMtlFile` | `objFilePath.substr(0, find_last_of("/\\"))` → `.mtl` |
| `ResourceManager::load_texture` | `replace_extension(".dds")` (같은 폴더 DDS 우선) |
| `SceneManager::build_main_landscapes`, 서버 `MapDataManager::LoadMainLandscapeData` | `directory_iterator` + `metadata.json` |
| 서버 `MapDataManager` (`.jbin` 캐시, `tileName + ".gltf"`) | 원본 경로 옆 |

### 4.4 경로 문자열을 키로 찾는 곳

| 위치 | 내용 | 할 일 |
|---|---|---|
| `ResourceManager::get_ibl_irradiance_srv`, `get_ibl_prefiltered_srv`, `get_ibl_brdf_lut_srv` | `"Resource\\SkyBox\\IBL_diffuse.dds"` 등을 하드코딩한 키로 찾고, 실패하면 이름 일부로 검색 | 별칭 전환 시 키 형태가 바뀌면 깨짐. 멤버(`_ibl_*_path`)만 쓰도록 정리하거나 `Expand` 결과로 비교 |
| `TerrainLoader`, `TerrainRenderComponent` | `get_texture(_heightmapTextureKey)`, 배열 키 | 키를 만드는 쪽과 찾는 쪽이 같은 `Expand` 결과를 쓰는지 확인 |
| `load_texture`의 `.dds` 대체 | 키는 원본(png) 경로, 실제 파일은 dds | 변경 없음 |

### 4.5 문자열을 조합하는 곳

| 위치 | 현재 | 별칭 표기 예 |
|---|---|---|
| `MainPlayerScript` (1), `Main_Scene` (4), `OtherPlayerScript` (1) | `animationpath = "Resource/Character/DarkKnight/DKF_animations/"` + 파일명 | `"DarkKnight:DKF_animations/"` + 파일명 |
| `TainerScript` | `basePath = "Resource/Character/BoneGolem/"` | `"BoneGolem:"` |
| `TerrainLoader::load_landscape_weightmaps` | `sharedTexpath = "Resource/MainLandscape/SharedTextures/"` + `_Albedo/_Normal/_Roughness.dds` | `"MainLandscape:SharedTextures/"` |
| `Boss_Scene`, `Main_Scene`, `NetworkManager` | `"Resource/UI/ID/Player_" + n + ".dds"` | `"UI:ID/Player_" + n + ".dds"` |
| `SceneManager::build_skybox` 호출 5곳 (`Boss_Scene`, `Chess_Scene`, `Main_Scene`, `Title_Scene`, `Tool_Scene`) | 공용 폴더 `"Resource/SkyBox/"`(체스는 `"Resource\\SkyBox\\"`) + 파일명 4개 | 공용 폴더 인자를 `"SkyBox:"`로 바꾸거나, 인자를 없애고 별칭으로 고정 |
| `Scene::load_*` | `basePath / meshFile` | 4.3 |

### 4.6 서버

| 위치 | 현재 (1차 작업) | 별칭 표기 예 |
|---|---|---|
| `server.cpp` `Server::Initialize` | `Resolve(PathRoot::ClientResource, "MainLandscape_Meshes/Landscape_-1_-1_MapData/...json")` | `"LandscapeMeshes:Landscape_-1_-1_MapData/...json"` |
| 〃 | `Landscape_-1_0_MapData/...json` | `"LandscapeMeshes:Landscape_-1_0_MapData/...json"` |
| 〃 | `Resolve(PathRoot::CommonData, "World_Batch_glTF/Tile_X-1_Y-1/Tile_X-1_Y-1.json")` | `"WorldBatch:Tile_X-1_Y-1/Tile_X-1_Y-1.json"` |
| 〃 | `1-BossScene/Boss_Landscape_ExportedClientData.json` | `"BossMap:Boss_Landscape_ExportedClientData.json"` |
| 〃 | `LoadMainLandscapeData(Resolve(ClientResource, "MainLandscape"))` | `"MainLandscape:"` |
| 〃 | `LoadNavMesh(..., ResolveApp("Resource/NavMesh2.obj"))` | `"NavMesh:NavMesh2.obj"` |
| `LuaManager` 4곳 | `Resolve(PathRoot::Lua, "NPC_Data.lua")` 등 | `"Lua:NPC_Data.lua"` |
| `Room.cpp` 물리 기록 | `Resolve(PathRoot::Saved, "physics_dump.bin")` | 유지 |

### 4.7 함께 고칠 기존 버그 (조사 중 발견)

| 위치 | 문제 |
|---|---|
| `Chess_Scene` `build_skybox` | `night_field\night_field_*.dds`, `IBL_BRDF_LUT.dds`를 부르는데 실제 폴더는 `SkyBox/night/`(파일명 `night_*`)이고, `IBL_BRDF_LUT.dds`는 없음 |
| `BoardCubeScript` | `Resource/MapData/SM_Crate_01.glb`를 부르는데 `Resource/MapData` 폴더가 없음 |
| `Shaders/Minimap_shader.hlsl` | 코드에서는 `Minimap_Shader.hlsl`로 부름(윈도우라 동작은 함) |

---

## 5. 진행 순서

1단계 PathManager 기능 → 2단계 입구 정리 → 3단계 배포 프로그램 → 4단계 점진적 이동. 상세 내용과 체크리스트는 6장에 있다.

---

## 6. 구현 계획과 체크리스트

### 6.0 핵심 규칙 (구현 전에 정한 것)

1. **배포 폴더는 개발 폴더의 상대 위치를 그대로 유지한다.**
   - App 루트(`Client/Client`, `Server/Server`) 안에 있는 별칭 폴더는 배포 때 같은 상대 위치로 복사한다. 예: `Client/Client/Resource/UI` → `<exe>/Resource/UI`.
   - 그래서 기존 `"Resource/..."` 표기가 배포본에서도 그대로 동작한다. 점진적 전환의 전제 조건이다.
   - App 밖에 있는 별칭(`Common/...`, 서버가 쓰는 클라 리소스)은 `<exe>/External/<별칭>/`으로 복사하고, 그 위치를 `Deploy.json`에 적는다.
2. **`Expand` 결과(논리 경로)는 개발·배포에서 같다.**
   - App 안 별칭은 `"Resource/UI/x.dds"`처럼 App 기준 상대 경로로 펼친다.
   - App 밖 별칭은 절대 경로로 펼친다.
   - 캐시 키는 한 번 실행하는 동안에만 일관되면 된다.
3. **별칭 문법**: `이름:나머지`. 이름은 매니페스트에 등록된 2글자 이상이어야 한다. `C:\...` 같은 드라이브 문자와 구분하기 위해서다.
4. **모르는 별칭은 에러 로그를 남기고 원문을 그대로 쓴다**(크래시 대신 파일 없음 에러로 드러나게).
5. **PathManager는 헤더 전용을 유지한다.** JSON은 `Common/json.hpp`를 쓴다(`TerrainData.h`와 같음).

### 6.1 파일 형식

`PathManifest.json` (저장소 루트, git 관리):
```json
{
  "binaries": {
    "client": ["Client/x64/{Config}/STL_Client.exe", "Client/x64/{Config}/fmod.dll", "Client/x64/{Config}/assimp-vc142-mt.dll"],
    "server": ["Server/x64/{Config}/STL_Server.exe", "Server/x64/{Config}/lua54.dll"]
  },
  "appRoots": { "client": "Client/Client", "server": "Server/Server" },
  "aliases": {
    "UI":       { "dev": "Client/Client/Resource/UI", "apps": ["client"] },
    "BossMap":  { "dev": "Client/Client/Resource/1-BossScene", "apps": ["client", "server"] },
    "Shaders":  { "dev": "Client/Client/Shaders", "apps": ["client"] },
    "Lua":      { "dev": "Server/Server/Lua", "apps": ["server"] }
  },
  "exclude": ["*.psd", "*.blend", "*.jbin"]
}
```

`Deploy.json` (배포 프로그램이 exe 옆에 생성):
```json
{
  "app": "client",
  "aliases": { "UI": "Resource/UI", "WorldBatch": "External/WorldBatch" }
}
```
값은 exe 폴더 기준 상대 경로다.

### 6.2 1단계: PathManager 기능 (`Common/PathManager.h`)

- [ ] `PathManifest.json`을 작성한다. 3.1·3.2 표의 별칭을 전부 넣고, 3.4의 배포 제외 후보는 넣지 않는다.
  - 결정 필요: `BruteAnim`을 묶을지 나눌지, `LandscapeMeshes` 하위 폴더별 별칭.
- [ ] `Init` 흐름을 바꾼다.
  1. exe 폴더를 구한다.
  2. `Deploy.json`이 있으면 배포 모드다. 별칭 표는 exe 기준으로 만든다.
  3. 없으면 저장소 루트를 찾고 `PathManifest.json`을 읽는다. 별칭 표는 저장소 기준으로 만든다.
  4. 둘 다 실패하면 에러 창을 띄운다.
- [ ] 기존 폴더 존재 검사(`Resource/`+`Shaders/`, `Lua/`)를 삭제한다.
- [ ] `Expand(std::string_view) -> std::string`를 추가한다. 6.0의 2·3·4번 규칙을 따른다.
- [ ] `ResolveApp`이 `Expand`를 먼저 하게 바꾼다. 결과가 절대 경로면 그대로 쓴다.
- [ ] `GetAlias(name)`를 추가한다. 별칭 폴더의 실제 경로를 돌려주며, `directory_iterator`를 쓰는 곳에서 사용한다.
- [ ] 사용 파일 기록(개발 모드만)을 넣는다.
  - `Resolve`할 때 `std::mutex` + `std::set<std::wstring>`에 넣는다.
  - `Init`에서 `atexit`으로 등록한 함수가 `Saved/used_files.txt`(UTF-8, 저장소 기준 상대 경로, 정렬)를 쓴다.
  - `.bin`, `metadata.json`처럼 다른 파일에서 파생되는 파일은 기록하지 않는다. 같은 폴더라 폴더 단위 검사에는 영향이 없다.
- [ ] 기존 루트(`Shader`, `Lua`, `Saved`, `ClientResource`, `CommonData`)는 이번 단계에서 유지한다(호환). `ClientResource`, `CommonData`는 서버 이동(6.5)이 끝나면 삭제한다.
- [ ] 로그: 모드, 매니페스트 경로, 별칭 개수, 없는 별칭 폴더 목록.
- [ ] 확인: 기존 표기만으로 클라·서버가 예전과 똑같이 동작하는지 본다(1차 작업 검증과 같음). `used_files.txt`가 생기는지도 본다.

### 6.3 2단계: 입구 정리 (`Expand` 적용)

4.2~4.4 표의 함수 입구에서 `const std::string path = PathManager::Expand(in);`로 바꾼 뒤 기존 로직을 그대로 쓴다.

- [ ] `ResourceManager`: `load_mesh`, `load_texture`, `get_texture`, `load_materials_from_gltf`, `load_cubemap_from_dds`, `load_skybox`, `load_ibl_maps`, `load_heightmap_from_raw`, R8 텍스처, 텍스처 배열
- [ ] `ResourceManager` IBL 조회 3개: 하드코딩한 키(`"Resource\SkyBox\IBL_*.dds"`)와 이름 검색 대체 로직을 지우고, `_ibl_*_path` 멤버(펼친 값)로만 찾는다
- [ ] `ReadGLTFMesh::load_animation_only`
- [ ] `Scene::load_scene_from_file`, `load_foliage_from_file`, `load_from_file_with_light` (`basePath` 계산 전에 펼치기)
- [ ] `SceneManager::build_skybox`: 공용 폴더 인자가 별칭(`"SkyBox:"`)이어도 동작하게, 이어 붙인 뒤 `Expand`
- [ ] `SceneManager::build_main_landscapes`: `GetAlias("MainLandscape")` 사용
- [ ] `TerrainLoader` 생성자 2개, `load_textures_to_resource_manager`, weightmap 로드
- [ ] `UIRenderComponent::set_texture`, `BillboardUIRenderComponent::set_texture`: 저장하는 경로 문자열 확인
- [ ] 서버 `MapDataManager::LoadStaticMeshShapes`, `LoadServerExportData`, `LoadMainLandscapeData`, `LoadNavMesh`: 함수 안에서 `PathManager::ResolveApp(Expand(path))`
- [ ] 확인: 아무 파일 하나를 별칭으로 바꿔 놓고, 같은 파일을 기존 표기로도 불러서 캐시가 한 번만 올라가는지 본다(로그). 끝나면 되돌린다.

### 6.4 3단계: 배포 프로그램 (`Tools/Deploy.ps1`)

- [ ] 인자: `-App client|server|all`, `-Config Release`, `-Out <폴더>`(기본 `Deploy/`), `-Build`(지정 시 MSBuild 먼저 실행), `-Check`
- [ ] `PathManifest.json`을 읽고 `binaries`의 `{Config}`를 치환해 exe와 DLL을 복사한다
- [ ] `apps`에 해당 앱이 있는 별칭만 복사한다(`robocopy /MIR`, `exclude` 적용). 위치는 6.0의 1번 규칙을 따른다
- [ ] `Deploy.json`을 생성한다. `Saved/`는 만들지 않는다(실행 중 생성)
- [ ] 끝나면 별칭별 크기와 합계를 출력한다
- [ ] `-Check`: `Saved/used_files.txt`(클라·서버)를 읽어, 어느 별칭 폴더에도 속하지 않는 파일을 출력한다. 하나라도 있으면 종료 코드 1
- [ ] `.gitignore`에 `/Deploy/` 추가
- [ ] 확인: `Deploy/Client`, `Deploy/Server`를 저장소 밖으로 옮겨 실행했을 때 배포 모드 로그가 나오고 정상 동작해야 한다

### 6.5 4단계: 점진적 이동

한 번에 하지 않는다. 아래 순서는 권장 순서이고, 각 파일은 그 파일을 다른 일로 손댈 때 해도 된다. 파일 하나를 바꿀 때마다 부록 A의 해당 줄을 전부 별칭으로 바꾸고, 해당 씬을 실행해 확인한다.

**우선 (구조 정리 효과가 큼)**
- [ ] `Server/Server/server.cpp` (6) + `LuaManager.cpp` (4): 끝나면 `PathRoot::ClientResource`, `CommonData` 삭제
- [ ] 접두 변수(4.5): `MainPlayerScript`, `Main_Scene` 4곳, `OtherPlayerScript`의 `animationpath`, `TainerScript`의 `basePath`, `TerrainLoader`의 `sharedTexpath`
- [ ] `Player_N.dds` 조합 3곳: `Boss_Scene`, `Main_Scene`, `NetworkManager`
- [ ] `build_skybox` 호출 5곳

**클라 씬·스크립트 (사용처 수)**
- [ ] `Main_Scene.cpp` (107)
- [ ] `Boss_Scene.cpp` (62)
- [ ] `Chess_Scene.cpp` (33). 4.7 스카이박스 버그를 같이 수정
- [ ] `MainPlayerScript.cpp` (25)
- [ ] `NPCScript.cpp` (21)
- [ ] `OtherPlayerScript.cpp` (18)
- [ ] `Title_Scene.cpp` (16)
- [ ] `TainerScript.cpp` (12)
- [ ] `SceneManager.cpp`, `TerrainLoader.cpp/.h`, `QuestNPCScript.cpp`, `Tool_Scene.cpp`, `LeverScript.cpp`, `NetworkManager.cpp`, `BoardCubeScript.cpp`(4.7 버그)

**셰이더 (선택)**
- [ ] 셰이더 클래스 18개 파일 + `ParticleSystemComponent`: 지금 `Resolve(PathRoot::Shader)`로 동작하므로 바꾸지 않아도 된다. 바꾸면 `"Shaders:Gltf_Shader.hlsl"`

### 6.6 검증 체크리스트 (단계마다)

- [ ] 클라·서버 Debug, Release 빌드
- [ ] 개발 모드: VS 실행 + `x64/Release` exe 더블클릭 둘 다
- [ ] 씬 5개 진입: 타이틀, 메인, 보스, 체스, 툴. 콘솔에 텍스처·메쉬·사운드·셰이더 로드 에러가 없어야 한다
- [ ] 서버 로그: 지형 5개, 정적 메쉬 3개, NavMesh, Lua 4개 로드 성공
- [ ] `Deploy.ps1 -Check`로 `used_files.txt` 검사 통과
- [ ] 배포 모드: `Deploy.ps1 -App all -Config Release`로 만든 결과를 저장소 밖에 두고 서버 실행 → 클라 접속 → 메인·보스 씬
- [ ] F8 물리 덤프와 `imgui.ini`가 각 배포본의 `Saved/`에 생기는지

### 6.7 문서

- [ ] `Common/CODEMAP.md`: PathManager 설명(매니페스트, `Expand`, 모드 판단)
- [ ] `Client/Client/CODEMAP.md`, `Server/Server/CODEMAP.md`: 1장 경로 기준, 배포 폴더
- [ ] 루트 `CODEMAP.md`: 진행 중 설계 문서 표에 이 문서 추가(파티클 브랜치 병합 후), `Tools/Deploy.ps1` 추가
- [ ] 이 문서의 3·4장과 부록 A는 이동이 끝난 항목을 표시하거나 지운다

---

## 부록 A. 파일별 경로 사용처 전체 목록

주석을 제외한 코드에서 경로 문자열이 나오는 줄이다. 줄 번호는 `temp/path-manager` `bf195ba23` 기준이라 코드가 바뀌면 틀어진다. "별칭(안)"은 3장 표를 기계적으로 적용한 결과다.


총 363줄, 38개 파일.

### `Client/Client/BillboardUIShader.cpp` (3)

| 줄 | 현재 문자열 | 별칭(안) |
|---|---|---|
| 34 | `BillboardUI_Shader.hlsl` | (셰이더: Resolve(Shader) 유지 또는 Shaders:) |
| 39 | `BillboardUI_Shader.hlsl` | (셰이더: Resolve(Shader) 유지 또는 Shaders:) |
| 44 | `BillboardUI_Shader.hlsl` | (셰이더: Resolve(Shader) 유지 또는 Shaders:) |

### `Client/Client/BoardCubeScript.cpp` (1)

| 줄 | 현재 문자열 | 별칭(안) |
|---|---|---|
| 27 | `Resource/MapData/SM_Crate_01.glb` | (폴더 없음, 4.7) |

### `Client/Client/Boss_Scene.cpp` (61)

| 줄 | 현재 문자열 | 별칭(안) |
|---|---|---|
| 24 | `Resource/SkyBox/` | `SkyBox:` |
| 25 | `farmland/farmland_skybox.dds` | (build_skybox 인자 4.5) |
| 26 | `farmland/farmland_specular.dds` | (build_skybox 인자 4.5) |
| 27 | `farmland/farmland_diffuse.txt` | (build_skybox 인자 4.5) |
| 28 | `BRDF.dds` | (build_skybox 인자 4.5) |
| 33 | `Resource/Character/BoneGolem/BoneGolem.gltf` | `BoneGolem:BoneGolem.gltf` |
| 34 | `Resource/Character/BoneGolem/BoneGolemRd.gltf` | `BoneGolem:BoneGolemRd.gltf` |
| 35 | `Resource/Character/DarkKnight/SKM_DKF_Full_With_Sword.gltf` | `DarkKnight:SKM_DKF_Full_With_Sword.gltf` |
| 36 | `Resource/Elevator/Elevator.gltf` | `Elevator:Elevator.gltf` |
| 39 | `Resource/1-BossScene/Boss_Landscape_ExportedClientData.json` | `BossMap:Boss_Landscape_ExportedClientData.json` |
| 57 | `Resource/Sound/BossBGM.mp3` | `Sound:BossBGM.mp3` |
| 61 | `Resource/Sound/BossCharge.wav` | `Sound:BossCharge.wav` |
| 62 | `Resource/Sound/BossGrab.wav` | `Sound:BossGrab.wav` |
| 63 | `Resource/Sound/BossLanding.mp3` | `Sound:BossLanding.mp3` |
| 64 | `Resource/Sound/BossRoar.wav` | `Sound:BossRoar.wav` |
| 89 | `Resource/UI/ID/Player_` | `UI:ID/Player_` |
| 105 | `Resource/UI/HP_Bar_Frame.dds` | `UI:HP_Bar_Frame.dds` |
| 110 | `Resource/UI/HP_Bar.dds` | `UI:HP_Bar.dds` |
| 121 | `Resource/UI/Q_interaction_UI_OFF.png` | `UI:Q_interaction_UI_OFF.png` |
| 122 | `Resource/UI/Q_interaction_UI_ON.png` | `UI:Q_interaction_UI_ON.png` |
| 132 | `Resource/UI/E_interaction_UI_OFF.png` | `UI:E_interaction_UI_OFF.png` |
| 133 | `Resource/UI/E_interaction_UI_ON.png` | `UI:E_interaction_UI_ON.png` |
| 143 | `Resource/UI/Controls_UI_New.png` | `UI:Controls_UI_New.png` |
| 159 | `Resource/UI/HP_Bar_Frame.dds` | `UI:HP_Bar_Frame.dds` |
| 164 | `Resource/UI/MP_Bar.dds` | `UI:MP_Bar.dds` |
| 176 | `Resource/UI/TX_BG.dds` | `UI:TX_BG.dds` |
| 191 | `Resource/UI/die_ui (1).dds` | `UI:die_ui (1).dds` |
| 202 | `Resource/UI/game_title_alpha.dds` | `UI:game_title_alpha.dds` |
| 212 | `Resource/UI/F_interaction_UI.dds` | `UI:F_interaction_UI.dds` |
| 222 | `Resource/UI/F_interaction_UI.dds` | `UI:F_interaction_UI.dds` |
| 232 | `Resource/UI/F_interaction_UI.dds` | `UI:F_interaction_UI.dds` |
| 244 | `Resource/UI/Quest_Question_UI.png` | `UI:Quest_Question_UI.png` |
| 247 | `Resource/UI/Quest_Exclamation_UI.png` | `UI:Quest_Exclamation_UI.png` |
| 257 | `Resource/UI/Quest_BG.png` | `UI:Quest_BG.png` |
| 269 | `Resource/UI/Quest_Title_1.png` | `UI:Quest_Title_1.png` |
| 272 | `Resource/UI/Quest_Reward.png` | `UI:Quest_Reward.png` |
| 273 | `Resource/UI/Quest_Title_2.png` | `UI:Quest_Title_2.png` |
| 274 | `Resource/UI/Quest_Title_3.png` | `UI:Quest_Title_3.png` |
| 284 | `Resource/UI/Quest_Reward.png` | `UI:Quest_Reward.png` |
| 298 | `Resource/UI/Help_Me.png` | `UI:Help_Me.png` |
| 310 | `Resource/UI/Quest_Story.png` | `UI:Quest_Story.png` |
| 327 | `Resource/UI/Quest_Numbers.png` | `UI:Quest_Numbers.png` |
| 334 | `Resource/UI/Quest_Numbers.png` | `UI:Quest_Numbers.png` |
| 345 | `Resource/UI/Quest_Numbers.png` | `UI:Quest_Numbers.png` |
| 361 | `Resource/UI/Ending_Scene.png` | `UI:Ending_Scene.png` |
| 391 | `Resource/UI/ID/Player_1.dds` | `UI:ID/Player_1.dds` |
| 392 | `Resource/UI/ID/Player_2.dds` | `UI:ID/Player_2.dds` |
| 393 | `Resource/UI/ID/Player_3.dds` | `UI:ID/Player_3.dds` |
| 394 | `Resource/UI/ID/Player_4.dds` | `UI:ID/Player_4.dds` |
| 405 | `Resource/UI/HP_Bar_Frame.dds` | `UI:HP_Bar_Frame.dds` |
| 413 | `Resource/UI/HP_Bar.dds` | `UI:HP_Bar.dds` |
| 424 | `Resource/UI/HP_Bar_Frame.dds` | `UI:HP_Bar_Frame.dds` |
| 431 | `Resource/UI/MP_Bar.dds` | `UI:MP_Bar.dds` |
| 460 | `Resource/UI/HP_Bar_Frame.dds` | `UI:HP_Bar_Frame.dds` |
| 469 | `Resource/UI/HP_Bar.dds` | `UI:HP_Bar.dds` |
| 480 | `Resource/UI/Boss_Name_Tainer.png` | `UI:Boss_Name_Tainer.png` |
| 493 | `Resource/UI/Percent_Sign.png` | `UI:Percent_Sign.png` |
| 508 | `Resource/UI/Quest_Numbers.png` | `UI:Quest_Numbers.png` |
| 520 | `Resource/UI/HP_Bar_Frame.dds` | `UI:HP_Bar_Frame.dds` |
| 521 | `Resource/UI/HP_Bar.dds` | `UI:HP_Bar.dds` |
| 533 | `Resource/Elevator/Elevator.gltf` | `Elevator:Elevator.gltf` |

### `Client/Client/Chess_Scene.cpp` (33)

| 줄 | 현재 문자열 | 별칭(안) |
|---|---|---|
| 28 | `Resource\\SkyBox\\` | `SkyBox:` |
| 29 | `night_field\\night_field_skybox.dds` | (build_skybox 인자 4.5) |
| 30 | `night_field\\night_field_diffuse.dds` | (build_skybox 인자 4.5) |
| 31 | `night_field\\night_field_specular.dds` | (build_skybox 인자 4.5) |
| 32 | `IBL_BRDF_LUT.dds` | (build_skybox 인자 4.5) |
| 37 | `Resource/Character/BruteHi/bruteHi.gltf` | `BruteHi:bruteHi.gltf` |
| 38 | `Resource/Character/Brute_Walk/Brute_Walk.gltf` | `BruteAnim:Brute_Walk/Brute_Walk.gltf` |
| 39 | `Resource/Character/BoneGolem/BoneGolem.gltf` | `BoneGolem:BoneGolem.gltf` |
| 40 | `Resource/Character/BoneGolem/BoneGolemRd.gltf` | `BoneGolem:BoneGolemRd.gltf` |
| 41 | `Resource/Character/DarkKnight/SKM_DKF_Full_With_Sword.gltf` | `DarkKnight:SKM_DKF_Full_With_Sword.gltf` |
| 42 | `Resource/Character/Brute_idle/Brute_idle.gltf` | `BruteAnim:Brute_idle/Brute_idle.gltf` |
| 43 | `Resource/Character/Brute_Attack_animation/Brute_Attack_animation.gltf` | `BruteAnim:Brute_Attack_animation/Brute_Attack_animation.gltf` |
| 54 | `Resource/MD/ExportedClientData.json` | `ChessMap:ExportedClientData.json` |
| 126 | `Resource/Character/BruteHi/bruteHi.gltf` | `BruteHi:bruteHi.gltf` |
| 220 | `Resource/Character/Gramma_Walk/Gramma_Walk.gltf` | `Gramma:Gramma_Walk.gltf` |
| 272 | `Resource/Character/SK_MagicConstruct/SK_MagicConstruct.gltf` | `MagicConstruct:SK_MagicConstruct.gltf` |
| 275 | `Resource/Character/SK_MagicConstruct/A_MagicConstruct_Combat_Unarmed_Dodge.gltf` | `MagicConstruct:A_MagicConstruct_Combat_Unarmed_Dodge.gltf` |
| 276 | `Resource/Character/SK_MagicConstruct/A_MagicConstruct_Combat_Unarmed_Attack03.gltf` | `MagicConstruct:A_MagicConstruct_Combat_Unarmed_Attack03.gltf` |
| 277 | `Resource/Character/SK_MagicConstruct/A_MagicConstruct_Combat_Unarmed_Attack02.gltf` | `MagicConstruct:A_MagicConstruct_Combat_Unarmed_Attack02.gltf` |
| 278 | `Resource/Character/SK_MagicConstruct/A_MagicConstruct_Combat_Unarmed_Attack01.gltf` | `MagicConstruct:A_MagicConstruct_Combat_Unarmed_Attack01.gltf` |
| 279 | `Resource/Character/SK_MagicConstruct/A_MagicConstruct_Combat_Unarmed_Attack.gltf` | `MagicConstruct:A_MagicConstruct_Combat_Unarmed_Attack.gltf` |
| 280 | `Resource/Character/SK_MagicConstruct/A_MagicConstruct_Combat_Stun.gltf` | `MagicConstruct:A_MagicConstruct_Combat_Stun.gltf` |
| 281 | `Resource/Character/SK_MagicConstruct/A_MagicConstruct_Combat_Roar.gltf` | `MagicConstruct:A_MagicConstruct_Combat_Roar.gltf` |
| 302 | `Resource/Weapons/SM_Weapon_Sword__10/SM_Weapon_Sword__10.gltf` | `Weapons:SM_Weapon_Sword__10/SM_Weapon_Sword__10.gltf` |
| 373 | `Resource/Character/DarkKnight/SKM_DKF_Full_With_Sword.gltf` | `DarkKnight:SKM_DKF_Full_With_Sword.gltf` |
| 377 | `Resource/Character/DarkKnight/Anim_DKF_Attack_02.gltf` | `DarkKnight:Anim_DKF_Attack_02.gltf` |
| 399 | `Resource/Weapons/SM_Weapon_Sword__10/SM_Weapon_Sword__10.gltf` | `Weapons:SM_Weapon_Sword__10/SM_Weapon_Sword__10.gltf` |
| 469 | `Resource/UI/HP_Bar_Frame.dds` | `UI:HP_Bar_Frame.dds` |
| 479 | `Resource/UI/HP_Bar.dds` | `UI:HP_Bar.dds` |
| 490 | `Resource/UI/TX_BG.dds` | `UI:TX_BG.dds` |
| 505 | `Resource/UI/die_ui (1).dds` | `UI:die_ui (1).dds` |
| 516 | `Resource/UI/HP_Bar_Frame.dds` | `UI:HP_Bar_Frame.dds` |
| 517 | `Resource/UI/HP_Bar.dds` | `UI:HP_Bar.dds` |

### `Client/Client/DebugShader.cpp` (2)

| 줄 | 현재 문자열 | 별칭(안) |
|---|---|---|
| 21 | `Debug.hlsl` | (셰이더: Resolve(Shader) 유지 또는 Shaders:) |
| 26 | `Debug.hlsl` | (셰이더: Resolve(Shader) 유지 또는 Shaders:) |

### `Client/Client/DefaultObjectShader.cpp` (2)

| 줄 | 현재 문자열 | 별칭(안) |
|---|---|---|
| 27 | `Shaders.hlsl` | (셰이더: Resolve(Shader) 유지 또는 Shaders:) |
| 32 | `Shaders.hlsl` | (셰이더: Resolve(Shader) 유지 또는 Shaders:) |

### `Client/Client/GlbShader.cpp` (2)

| 줄 | 현재 문자열 | 별칭(안) |
|---|---|---|
| 32 | `GLB_Shader.hlsl` | (셰이더: Resolve(Shader) 유지 또는 Shaders:) |
| 38 | `GLB_Shader.hlsl` | (셰이더: Resolve(Shader) 유지 또는 Shaders:) |

### `Client/Client/GltfShader.cpp` (2)

| 줄 | 현재 문자열 | 별칭(안) |
|---|---|---|
| 58 | `Gltf_Shader.hlsl` | (셰이더: Resolve(Shader) 유지 또는 Shaders:) |
| 63 | `Gltf_Shader.hlsl` | (셰이더: Resolve(Shader) 유지 또는 Shaders:) |

### `Client/Client/GltfSkinnedShader.cpp` (1)

| 줄 | 현재 문자열 | 별칭(안) |
|---|---|---|
| 35 | `Gltf_Skinned_Shader.hlsl` | (셰이더: Resolve(Shader) 유지 또는 Shaders:) |

### `Client/Client/LeverScript.cpp` (2)

| 줄 | 현재 문자열 | 별칭(안) |
|---|---|---|
| 28 | `Resource/Lever/Lever.gltf` | `Lever:Lever.gltf` |
| 32 | `Resource/Lever/Animation/Lever_UP.gltf` | `Lever:Animation/Lever_UP.gltf` |

### `Client/Client/MainPlayerScript.cpp` (25)

| 줄 | 현재 문자열 | 별칭(안) |
|---|---|---|
| 169 | `Resource/Character/DarkKnight/SKM_DKF_Full_With_Sword.gltf` | `DarkKnight:SKM_DKF_Full_With_Sword.gltf` |
| 171 | `Resource/Character/DarkKnight/DKF_animations/` | `DarkKnight:DKF_animations/` |
| 172 | `Anim_DKF_Idle_Alert.gltf` | (접두 변수 4.5) |
| 173 | `Anim_DKF_Walk_Alert_Fwd.gltf` | (접두 변수 4.5) |
| 174 | `Anim_DKF_Run_Alert_Fwd.gltf` | (접두 변수 4.5) |
| 175 | `Anim_DKF_Attack_01.gltf` | (접두 변수 4.5) |
| 176 | `Anim_DKF_Skill_01.gltf` | (접두 변수 4.5) |
| 177 | `Anim_DKF_Skill_01_end.gltf` | (접두 변수 4.5) |
| 178 | `Anim_DKF_Death.gltf` | (접두 변수 4.5) |
| 181 | `Anim_DKF_Crouch_Alert_Fwd.gltf` | (접두 변수 4.5) |
| 182 | `Anim_DKF_Crouch_Alert_Bwd.gltf` | (접두 변수 4.5) |
| 183 | `Anim_DKF_Crouch_Alert_Left.gltf` | (접두 변수 4.5) |
| 184 | `Anim_DKF_Crouch_Alert_Right.gltf` | (접두 변수 4.5) |
| 207 | `Resource/Sound/SwordSwing.mp3` | `Sound:SwordSwing.mp3` |
| 208 | `Resource/Sound/Dust.wav` | `Sound:Dust.wav` |
| 209 | `Resource/Sound/PlayerDamage.wav` | `Sound:PlayerDamage.wav` |
| 210 | `Resource/Sound/PlayerDash.ogg` | `Sound:PlayerDash.ogg` |
| 211 | `Resource/Sound/SwordSwingStrong.mp3` | `Sound:SwordSwingStrong.mp3` |
| 242 | `Resource/Weapons/SM_Weapon_Sword__10/SM_Weapon_Sword__10.gltf` | `Weapons:SM_Weapon_Sword__10/SM_Weapon_Sword__10.gltf` |
| 475 | `Resource/UI/Quest_Title_3.png` | `UI:Quest_Title_3.png` |
| 479 | `Resource/UI/Quest_Title_1.png` | `UI:Quest_Title_1.png` |
| 600 | `Resource/UI/Q_interaction_UI_ON.png` | `UI:Q_interaction_UI_ON.png` |
| 600 | `Resource/UI/Q_interaction_UI_OFF.png` | `UI:Q_interaction_UI_OFF.png` |
| 605 | `Resource/UI/E_interaction_UI_ON.png` | `UI:E_interaction_UI_ON.png` |
| 605 | `Resource/UI/E_interaction_UI_OFF.png` | `UI:E_interaction_UI_OFF.png` |

### `Client/Client/Main_Scene.cpp` (106)

| 줄 | 현재 문자열 | 별칭(안) |
|---|---|---|
| 47 | `Resource/SkyBox/` | `SkyBox:` |
| 48 | `cloudy/cloudy_skybox.dds` | (build_skybox 인자 4.5) |
| 49 | `cloudy/cloudy_specular.dds` | (build_skybox 인자 4.5) |
| 50 | `diffuse.txt` | (build_skybox 인자 4.5) |
| 51 | `BRDF.dds` | (build_skybox 인자 4.5) |
| 55 | `Resource/Foliage/SM_Grass_01.gltf` | `Foliage:SM_Grass_01.gltf` |
| 56 | `Resource/Foliage/SM_Dead_grass_01.gltf` | `Foliage:SM_Dead_grass_01.gltf` |
| 62 | `Resource/Character/BruteHi/bruteHi.gltf` | `BruteHi:bruteHi.gltf` |
| 63 | `Resource/Weapons/SM_Weapon_Sword__10/SM_Weapon_Sword__10.gltf` | `Weapons:SM_Weapon_Sword__10/SM_Weapon_Sword__10.gltf` |
| 64 | `Resource/Character/DragonBrute/SK_DragonBrute.gltf` | `DragonBrute:SK_DragonBrute.gltf` |
| 65 | `Resource/Character/Brute_Walk/Brute_Walk.gltf` | `BruteAnim:Brute_Walk/Brute_Walk.gltf` |
| 66 | `Resource/Character/BoneGolem/BoneGolem.gltf` | `BoneGolem:BoneGolem.gltf` |
| 67 | `Resource/Character/BoneGolem/BoneGolemRd.gltf` | `BoneGolem:BoneGolemRd.gltf` |
| 68 | `Resource/Character/DarkKnight/SKM_DKF_Full_With_Sword.gltf` | `DarkKnight:SKM_DKF_Full_With_Sword.gltf` |
| 69 | `Resource/Character/SK_MagicConstruct/SK_MagicConstruct.gltf` | `MagicConstruct:SK_MagicConstruct.gltf` |
| 70 | `Resource/Character/Bandit_Rd_NPC/Bandit_Rd_NPC.gltf` | `Bandit:Bandit_Rd_NPC.gltf` |
| 71 | `Resource/Lever/Lever.gltf` | `Lever:Lever.gltf` |
| 72 | `Resource/Character/Brute_idle/Brute_idle.gltf` | `BruteAnim:Brute_idle/Brute_idle.gltf` |
| 73 | `Resource/Character/Brute_Attack_animation/Brute_Attack_animation.gltf` | `BruteAnim:Brute_Attack_animation/Brute_Attack_animation.gltf` |
| 77 | `Resource/MainLandscape_Meshes/Landscape_0_0_MapData/Landscape_0_0_ExportedClientData.json` | `LandscapeMeshes:Landscape_0_0_MapData/Landscape_0_0_ExportedClientData.json` |
| 78 | `Resource/MainLandscape_Meshes/Landscape_0_-1_MapData/Landscape_0_-1_ExportedClientData.json` | `LandscapeMeshes:Landscape_0_-1_MapData/Landscape_0_-1_ExportedClientData.json` |
| 82 | `Resource/MainLandscape_Meshes/Landscape_-1_-1_MapData/Landscape_-1_-1_ExportedClientData.json` | `LandscapeMeshes:Landscape_-1_-1_MapData/Landscape_-1_-1_ExportedClientData.json` |
| 87 | `Resource/MainLandscape_Meshes/Landscape_-1_0_MapData/Landscape_-1_0_ExportedClientData.json` | `LandscapeMeshes:Landscape_-1_0_MapData/Landscape_-1_0_ExportedClientData.json` |
| 91 | `Resource/MainLandscape_Meshes/Landscape_-2_-1_MapData/Landscape_-2_-1_ExportedClientData.json` | `LandscapeMeshes:Landscape_-2_-1_MapData/Landscape_-2_-1_ExportedClientData.json` |
| 117 | `Resource/LeverAndPosition/Meshes/Cube_5E5A4B61.gltf` | `LeverAndPosition:Meshes/Cube_5E5A4B61.gltf` |
| 119 | `World_Batch_glTF/Tile_X-1_Y-1/Tile_X-1_Y-1 Server Export Data.json` | `WorldBatch:Tile_X-1_Y-1/Tile_X-1_Y-1 Server Export Data.json` |
| 123 | `Resource/Sound/MainBGM.mp3` | `Sound:MainBGM.mp3` |
| 174 | `Resource/UI/ID/Player_` | `UI:ID/Player_` |
| 190 | `Resource/UI/HP_Bar_Frame.dds` | `UI:HP_Bar_Frame.dds` |
| 195 | `Resource/UI/HP_Bar.dds` | `UI:HP_Bar.dds` |
| 206 | `Resource/UI/Q_interaction_UI_OFF.png` | `UI:Q_interaction_UI_OFF.png` |
| 207 | `Resource/UI/Q_interaction_UI_ON.png` | `UI:Q_interaction_UI_ON.png` |
| 217 | `Resource/UI/E_interaction_UI_OFF.png` | `UI:E_interaction_UI_OFF.png` |
| 218 | `Resource/UI/E_interaction_UI_ON.png` | `UI:E_interaction_UI_ON.png` |
| 228 | `Resource/UI/Controls_UI_New.png` | `UI:Controls_UI_New.png` |
| 244 | `Resource/UI/HP_Bar_Frame.dds` | `UI:HP_Bar_Frame.dds` |
| 249 | `Resource/UI/MP_Bar.dds` | `UI:MP_Bar.dds` |
| 261 | `Resource/UI/TX_BG.dds` | `UI:TX_BG.dds` |
| 276 | `Resource/UI/die_ui (1).dds` | `UI:die_ui (1).dds` |
| 287 | `Resource/UI/game_title_alpha.dds` | `UI:game_title_alpha.dds` |
| 297 | `Resource/UI/F_interaction_UI.dds` | `UI:F_interaction_UI.dds` |
| 307 | `Resource/UI/F_interaction_UI.dds` | `UI:F_interaction_UI.dds` |
| 317 | `Resource/UI/F_interaction_UI.dds` | `UI:F_interaction_UI.dds` |
| 329 | `Resource/UI/Quest_Question_UI.png` | `UI:Quest_Question_UI.png` |
| 332 | `Resource/UI/Quest_Exclamation_UI.png` | `UI:Quest_Exclamation_UI.png` |
| 342 | `Resource/UI/Quest_BG.png` | `UI:Quest_BG.png` |
| 354 | `Resource/UI/Quest_Title_1.png` | `UI:Quest_Title_1.png` |
| 357 | `Resource/UI/Quest_Title_3.png` | `UI:Quest_Title_3.png` |
| 358 | `Resource/UI/Quest_Reward.png` | `UI:Quest_Reward.png` |
| 368 | `Resource/UI/Quest_Reward.png` | `UI:Quest_Reward.png` |
| 382 | `Resource/UI/Help_Me.png` | `UI:Help_Me.png` |
| 394 | `Resource/UI/Quest_Story.png` | `UI:Quest_Story.png` |
| 411 | `Resource/UI/Quest_Numbers.png` | `UI:Quest_Numbers.png` |
| 442 | `Resource/UI/ID/Player_1.dds` | `UI:ID/Player_1.dds` |
| 443 | `Resource/UI/ID/Player_2.dds` | `UI:ID/Player_2.dds` |
| 444 | `Resource/UI/ID/Player_3.dds` | `UI:ID/Player_3.dds` |
| 445 | `Resource/UI/ID/Player_4.dds` | `UI:ID/Player_4.dds` |
| 456 | `Resource/UI/HP_Bar_Frame.dds` | `UI:HP_Bar_Frame.dds` |
| 464 | `Resource/UI/HP_Bar.dds` | `UI:HP_Bar.dds` |
| 475 | `Resource/UI/HP_Bar_Frame.dds` | `UI:HP_Bar_Frame.dds` |
| 482 | `Resource/UI/MP_Bar.dds` | `UI:MP_Bar.dds` |
| 501 | `Resource/UI/HP_Bar_Frame.dds` | `UI:HP_Bar_Frame.dds` |
| 502 | `Resource/UI/HP_Bar.dds` | `UI:HP_Bar.dds` |
| 570 | `Resource/Lever/Lever.gltf` | `Lever:Lever.gltf` |
| 573 | `Resource/Lever/Animation/Lever_UP.gltf` | `Lever:Animation/Lever_UP.gltf` |
| 602 | `Resource/Lever/Lever.gltf` | `Lever:Lever.gltf` |
| 645 | `Resource/UI/just_black_background.dds` | `UI:just_black_background.dds` |
| 659 | `Resource/Character/DarkKnight/DKF_animations/` | `DarkKnight:DKF_animations/` |
| 661 | `Resource/Character/DarkKnight/SKM_DKF_Full_With_Sword.gltf` | `DarkKnight:SKM_DKF_Full_With_Sword.gltf` |
| 662 | `Anim_DKF_Idle_Alert.gltf` | (접두 변수 4.5) |
| 663 | `Anim_DKF_Walk_Alert_Fwd.gltf` | (접두 변수 4.5) |
| 664 | `Anim_DKF_Run_Alert_Fwd.gltf` | (접두 변수 4.5) |
| 665 | `Anim_DKF_Attack_01.gltf` | (접두 변수 4.5) |
| 666 | `Anim_DKF_Skill_01.gltf` | (접두 변수 4.5) |
| 667 | `Anim_DKF_Skill_01_end.gltf` | (접두 변수 4.5) |
| 668 | `Anim_DKF_Death.gltf` | (접두 변수 4.5) |
| 715 | `Resource/Character/DarkKnight/DKF_animations/` | `DarkKnight:DKF_animations/` |
| 717 | `Resource/Character/DarkKnight/SKM_DKF_Full_With_Sword.gltf` | `DarkKnight:SKM_DKF_Full_With_Sword.gltf` |
| 718 | `Anim_DKF_Idle_Alert.gltf` | (접두 변수 4.5) |
| 719 | `Anim_DKF_Walk_Alert_Fwd.gltf` | (접두 변수 4.5) |
| 720 | `Anim_DKF_Run_Alert_Fwd.gltf` | (접두 변수 4.5) |
| 721 | `Anim_DKF_Attack_01.gltf` | (접두 변수 4.5) |
| 722 | `Anim_DKF_Skill_01.gltf` | (접두 변수 4.5) |
| 723 | `Anim_DKF_Skill_01_end.gltf` | (접두 변수 4.5) |
| 724 | `Anim_DKF_Death.gltf` | (접두 변수 4.5) |
| 771 | `Resource/Character/DarkKnight/DKF_animations/` | `DarkKnight:DKF_animations/` |
| 773 | `Resource/Character/DarkKnight/SKM_DKF_Full_With_Sword.gltf` | `DarkKnight:SKM_DKF_Full_With_Sword.gltf` |
| 774 | `Anim_DKF_Idle_Alert.gltf` | (접두 변수 4.5) |
| 775 | `Anim_DKF_Walk_Alert_Fwd.gltf` | (접두 변수 4.5) |
| 776 | `Anim_DKF_Run_Alert_Fwd.gltf` | (접두 변수 4.5) |
| 777 | `Anim_DKF_Attack_01.gltf` | (접두 변수 4.5) |
| 778 | `Anim_DKF_Skill_01.gltf` | (접두 변수 4.5) |
| 779 | `Anim_DKF_Skill_01_end.gltf` | (접두 변수 4.5) |
| 780 | `Anim_DKF_Death.gltf` | (접두 변수 4.5) |
| 827 | `Resource/Character/DarkKnight/DKF_animations/` | `DarkKnight:DKF_animations/` |
| 829 | `Resource/Character/DarkKnight/SKM_DKF_Full_With_Sword.gltf` | `DarkKnight:SKM_DKF_Full_With_Sword.gltf` |
| 830 | `Anim_DKF_Idle_Alert.gltf` | (접두 변수 4.5) |
| 831 | `Anim_DKF_Walk_Alert_Fwd.gltf` | (접두 변수 4.5) |
| 832 | `Anim_DKF_Run_Alert_Fwd.gltf` | (접두 변수 4.5) |
| 833 | `Anim_DKF_Attack_01.gltf` | (접두 변수 4.5) |
| 834 | `Anim_DKF_Skill_01.gltf` | (접두 변수 4.5) |
| 835 | `Anim_DKF_Skill_01_end.gltf` | (접두 변수 4.5) |
| 836 | `Anim_DKF_Death.gltf` | (접두 변수 4.5) |
| 893 | `Resource/Weapons/SM_Weapon_Sword__10/SM_Weapon_Sword__10.gltf` | `Weapons:SM_Weapon_Sword__10/SM_Weapon_Sword__10.gltf` |
| 930 | `Resource/LeverAndPosition/Meshes/SM_fountain_01_1B971041.gltf` | `LeverAndPosition:Meshes/SM_fountain_01_1B971041.gltf` |
| 986 | `Resource/Sound/Monster_Hunter_World_OST_Journey_to_the_Truth.mp3` | `Sound:Monster_Hunter_World_OST_Journey_to_the_Truth.mp3` |

### `Client/Client/MinimapShader.cpp` (2)

| 줄 | 현재 문자열 | 별칭(안) |
|---|---|---|
| 36 | `Minimap_Shader.hlsl` | (셰이더: Resolve(Shader) 유지 또는 Shaders:) |
| 41 | `Minimap_Shader.hlsl` | (셰이더: Resolve(Shader) 유지 또는 Shaders:) |

### `Client/Client/MonsterHPUIShader.cpp` (3)

| 줄 | 현재 문자열 | 별칭(안) |
|---|---|---|
| 71 | `Monster_HP_Shader.hlsl` | (셰이더: Resolve(Shader) 유지 또는 Shaders:) |
| 76 | `Monster_HP_Shader.hlsl` | (셰이더: Resolve(Shader) 유지 또는 Shaders:) |
| 81 | `Monster_HP_Shader.hlsl` | (셰이더: Resolve(Shader) 유지 또는 Shaders:) |

### `Client/Client/NPCScript.cpp` (21)

| 줄 | 현재 문자열 | 별칭(안) |
|---|---|---|
| 118 | `Resource/Sound/MonsterAttack.mp3` | `Sound:MonsterAttack.mp3` |
| 119 | `Resource/Sound/BossDamage.mp3` | `Sound:BossDamage.mp3` |
| 120 | `Resource/Sound/MagicGuardDamage.wav` | `Sound:MagicGuardDamage.wav` |
| 121 | `Resource/Sound/MonsterDie.mp3` | `Sound:MonsterDie.mp3` |
| 124 | `Resource/Sound/BossCharge.wav` | `Sound:BossCharge.wav` |
| 125 | `Resource/Sound/BossGrab.wav` | `Sound:BossGrab.wav` |
| 126 | `Resource/Sound/BossLanding.mp3` | `Sound:BossLanding.mp3` |
| 127 | `Resource/Sound/BossRoar.wav` | `Sound:BossRoar.wav` |
| 131 | `Resource/Elevator/Elevator.gltf` | `Elevator:Elevator.gltf` |
| 144 | `Resource/LeverAndPosition/Meshes/Cube_5E5A4B61.gltf` | `LeverAndPosition:Meshes/Cube_5E5A4B61.gltf` |
| 160 | `Resource/Character/SK_MagicConstruct/SK_MagicConstruct.gltf` | `MagicConstruct:SK_MagicConstruct.gltf` |
| 162 | `Resource/Character/SK_MagicConstruct/A_MagicConstruct_Idle01.gltf` | `MagicConstruct:A_MagicConstruct_Idle01.gltf` |
| 163 | `Resource/Character/SK_MagicConstruct/A_MagicConstruct_Walk_Forward.gltf` | `MagicConstruct:A_MagicConstruct_Walk_Forward.gltf` |
| 164 | `Resource/Character/SK_MagicConstruct/A_MagicConstruct_Combat_Unarmed_Attack.gltf` | `MagicConstruct:A_MagicConstruct_Combat_Unarmed_Attack.gltf` |
| 165 | `Resource/Character/SK_MagicConstruct/A_MagicConstruct_Death.gltf` | `MagicConstruct:A_MagicConstruct_Death.gltf` |
| 192 | `Resource/Character/DragonBrute/SK_DragonBrute.gltf` | `DragonBrute:SK_DragonBrute.gltf` |
| 194 | `Resource/Character/DragonBrute/animation/A_DragonBrute_Idle.gltf` | `DragonBrute:animation/A_DragonBrute_Idle.gltf` |
| 195 | `Resource/Character/DragonBrute/animation/A_DragonBrute_Walk.gltf` | `DragonBrute:animation/A_DragonBrute_Walk.gltf` |
| 196 | `Resource/Character/DragonBrute/animation/A_DragonBrute_Attack.gltf` | `DragonBrute:animation/A_DragonBrute_Attack.gltf` |
| 197 | `Resource/Character/DragonBrute/animation/A_DragonBrute_Death.gltf` | `DragonBrute:animation/A_DragonBrute_Death.gltf` |
| 198 | `Resource/Character/DragonBrute/animation/A_DragonBrute_Hit.gltf` | `DragonBrute:animation/A_DragonBrute_Hit.gltf` |

### `Client/Client/NetworkManager.cpp` (2)

| 줄 | 현재 문자열 | 별칭(안) |
|---|---|---|
| 505 | `Resource/UI/ID/Player_` | `UI:ID/Player_` |
| 1143 | `Resource/Sound/QuestComplete.mp3` | `Sound:QuestComplete.mp3` |

### `Client/Client/OcclusionQueryShader.cpp` (2)

| 줄 | 현재 문자열 | 별칭(안) |
|---|---|---|
| 12 | `OcclusionQuery.hlsl` | (셰이더: Resolve(Shader) 유지 또는 Shaders:) |
| 16 | `OcclusionQuery.hlsl` | (셰이더: Resolve(Shader) 유지 또는 Shaders:) |

### `Client/Client/OtherPlayerScript.cpp` (18)

| 줄 | 현재 문자열 | 별칭(안) |
|---|---|---|
| 405 | `Resource/Character/DarkKnight/SKM_DKF_Full_With_Sword.gltf` | `DarkKnight:SKM_DKF_Full_With_Sword.gltf` |
| 407 | `Resource/Character/DarkKnight/DKF_animations/` | `DarkKnight:DKF_animations/` |
| 408 | `Anim_DKF_Idle_Alert.gltf` | (접두 변수 4.5) |
| 409 | `Anim_DKF_Walk_Alert_Fwd.gltf` | (접두 변수 4.5) |
| 410 | `Anim_DKF_Run_Alert_Fwd.gltf` | (접두 변수 4.5) |
| 411 | `Anim_DKF_Attack_01.gltf` | (접두 변수 4.5) |
| 412 | `Anim_DKF_Skill_01.gltf` | (접두 변수 4.5) |
| 413 | `Anim_DKF_Skill_01_end.gltf` | (접두 변수 4.5) |
| 414 | `Anim_DKF_Death.gltf` | (접두 변수 4.5) |
| 417 | `Anim_DKF_Crouch_Alert_Fwd.gltf` | (접두 변수 4.5) |
| 418 | `Anim_DKF_Crouch_Alert_Bwd.gltf` | (접두 변수 4.5) |
| 419 | `Anim_DKF_Crouch_Alert_Left.gltf` | (접두 변수 4.5) |
| 420 | `Anim_DKF_Crouch_Alert_Right.gltf` | (접두 변수 4.5) |
| 442 | `Resource/Sound/SwordSwing.mp3` | `Sound:SwordSwing.mp3` |
| 443 | `Resource/Sound/Dust.wav` | `Sound:Dust.wav` |
| 444 | `Resource/Sound/PlayerDamage.wav` | `Sound:PlayerDamage.wav` |
| 445 | `Resource/Sound/PlayerDash.ogg` | `Sound:PlayerDash.ogg` |
| 469 | `Resource/Weapons/SM_Weapon_Sword__10/SM_Weapon_Sword__10.gltf` | `Weapons:SM_Weapon_Sword__10/SM_Weapon_Sword__10.gltf` |

### `Client/Client/ParticleShader.cpp` (2)

| 줄 | 현재 문자열 | 별칭(안) |
|---|---|---|
| 54 | `Particle_Draw.hlsl` | (셰이더: Resolve(Shader) 유지 또는 Shaders:) |
| 58 | `Particle_Draw.hlsl` | (셰이더: Resolve(Shader) 유지 또는 Shaders:) |

### `Client/Client/ParticleSystemComponent.cpp` (1)

| 줄 | 현재 문자열 | 별칭(안) |
|---|---|---|
| 39 | `Particle_CS.hlsl` | (셰이더: Resolve(Shader) 유지 또는 Shaders:) |

### `Client/Client/PhysicsDebugCapture.cpp` (1)

| 줄 | 현재 문자열 | 별칭(안) |
|---|---|---|
| 24 | `client_physics_dump.bin` | (Saved 유지) |

### `Client/Client/PlayerShader.cpp` (2)

| 줄 | 현재 문자열 | 별칭(안) |
|---|---|---|
| 23 | `Shaders.hlsl` | (셰이더: Resolve(Shader) 유지 또는 Shaders:) |
| 28 | `Shaders.hlsl` | (셰이더: Resolve(Shader) 유지 또는 Shaders:) |

### `Client/Client/QuestNPCScript.cpp` (5)

| 줄 | 현재 문자열 | 별칭(안) |
|---|---|---|
| 28 | `Resource/Character/Bandit_Rd_NPC/Bandit_Rd_NPC.gltf` | `Bandit:Bandit_Rd_NPC.gltf` |
| 32 | `Resource/Character/Bandit_Rd_NPC/Animations/A_Hu_F_Idle.gltf` | `Bandit:Animations/A_Hu_F_Idle.gltf` |
| 200 | `Resource/UI/Quest_Exclamation_UI.png` | `UI:Quest_Exclamation_UI.png` |
| 204 | `Resource/UI/Quest_Question_UI.png` | `UI:Quest_Question_UI.png` |
| 208 | `Resource/UI/Quest_Question_UI.png` | `UI:Quest_Question_UI.png` |

### `Client/Client/ResourceManager.cpp` (3)

| 줄 | 현재 문자열 | 별칭(안) |
|---|---|---|
| 917 | `Resource\\SkyBox\\IBL_diffuse.dds` | `SkyBox:IBL_diffuse.dds` |
| 945 | `Resource\\SkyBox\\IBL_specular.dds` | `SkyBox:IBL_specular.dds` |
| 965 | `Resource\\SkyBox\\IBL_BRDF_LUT.dds` | `SkyBox:IBL_BRDF_LUT.dds` |

### `Client/Client/SceneManager.cpp` (4)

| 줄 | 현재 문자열 | 별칭(안) |
|---|---|---|
| 253 | `MapData/Heightmap.json` | `CommonMapData:Heightmap.json` |
| 260 | `Resource\\HeightMap\\rocky_terrain\\rocky_terrain_02_4k.gltf` | `HeightMap:rocky_terrain/rocky_terrain_02_4k.gltf` |
| 261 | `Resource\\HeightMap\\aerial_rocks\\textures\\aerial_rocks_04_diff_4k.dds` | `HeightMap:aerial_rocks/textures/aerial_rocks_04_diff_4k.dds` |
| 292 | `Resource/MainLandscape` | `MainLandscape:` |

### `Client/Client/ShadowDepthShader.cpp` (1)

| 줄 | 현재 문자열 | 별칭(안) |
|---|---|---|
| 18 | `Shadow_Depth.hlsl` | (셰이더: Resolve(Shader) 유지 또는 Shaders:) |

### `Client/Client/ShadowDepthSkinnedShader.cpp` (1)

| 줄 | 현재 문자열 | 별칭(안) |
|---|---|---|
| 22 | `Shadow_Depth_Skinned.hlsl` | (셰이더: Resolve(Shader) 유지 또는 Shaders:) |

### `Client/Client/SkyboxShader.cpp` (2)

| 줄 | 현재 문자열 | 별칭(안) |
|---|---|---|
| 22 | `Skybox_Shader.hlsl` | (셰이더: Resolve(Shader) 유지 또는 Shaders:) |
| 27 | `Skybox_Shader.hlsl` | (셰이더: Resolve(Shader) 유지 또는 Shaders:) |

### `Client/Client/TainerScript.cpp` (12)

| 줄 | 현재 문자열 | 별칭(안) |
|---|---|---|
| 46 | `Resource/Character/BoneGolem/` | `BoneGolem:` |
| 49 | `BoneGolemRd.gltf` | (접두 변수 4.5) |
| 54 | `A_BoneGolem_Idle.gltf` | (접두 변수 4.5) |
| 55 | `A_BoneGolem_Walk.gltf` | (접두 변수 4.5) |
| 56 | `A_BoneGolem_Run.gltf` | (접두 변수 4.5) |
| 57 | `A_BoneGolem_Attack.gltf` | (접두 변수 4.5) |
| 58 | `A_BoneGolem_Attack01.gltf` | (접두 변수 4.5) |
| 59 | `A_BoneGolem_Attack02.gltf` | (접두 변수 4.5) |
| 60 | `A_BoneGolem_Hit.gltf` | (접두 변수 4.5) |
| 61 | `A_BoneGolem_Roar.gltf` | (접두 변수 4.5) |
| 62 | `A_BoneGolem_Swim.gltf` | (접두 변수 4.5) |
| 63 | `A_BoneGolem_Death.gltf` | (접두 변수 4.5) |

### `Client/Client/TerrainLoader.cpp` (4)

| 줄 | 현재 문자열 | 별칭(안) |
|---|---|---|
| 470 | `Resource/MainLandscape/SharedTextures/` | `MainLandscape:SharedTextures/` |
| 498 | `_Albedo.dds` | (접두 변수 4.5) |
| 499 | `_Normal.dds` | (접두 변수 4.5) |
| 500 | `_Roughness.dds` | (접두 변수 4.5) |

### `Client/Client/TerrainShader.cpp` (2)

| 줄 | 현재 문자열 | 별칭(안) |
|---|---|---|
| 40 | `Terrain_Shader.hlsl` | (셰이더: Resolve(Shader) 유지 또는 Shaders:) |
| 45 | `Terrain_Shader.hlsl` | (셰이더: Resolve(Shader) 유지 또는 Shaders:) |

### `Client/Client/Title_Scene.cpp` (16)

| 줄 | 현재 문자열 | 별칭(안) |
|---|---|---|
| 104 | `Resource/UI/just_black_background.dds` | `UI:just_black_background.dds` |
| 116 | `Resource/UI/PIP_GAMES_LOGO.dds` | `UI:PIP_GAMES_LOGO.dds` |
| 128 | `Resource/UI/game_title_alpha.dds` | `UI:game_title_alpha.dds` |
| 140 | `Resource/UI/Controls_UI_New.png` | `UI:Controls_UI_New.png` |
| 178 | `Resource/SkyBox/` | `SkyBox:` |
| 179 | `cloudy/cloudy_skybox.dds` | (build_skybox 인자 4.5) |
| 180 | `cloudy/cloudy_specular.dds` | (build_skybox 인자 4.5) |
| 181 | `diffuse.txt` | (build_skybox 인자 4.5) |
| 182 | `BRDF.dds` | (build_skybox 인자 4.5) |
| 193 | `Resource/Character/DarkKnightNoneSword/SKM_DKF_Full.gltf` | `DarkKnightNoSword:SKM_DKF_Full.gltf` |
| 199 | `Resource/MainLandscape_Meshes/Landscape_-1_0_MapData/Landscape_-1_0_ExportedClientData.json` | `LandscapeMeshes:Landscape_-1_0_MapData/Landscape_-1_0_ExportedClientData.json` |
| 200 | `Resource/Foliage/Foliage_tree_-1_0_MapData/Foliage_tree_-1_0_MapData.json` | `Foliage:Foliage_tree_-1_0_MapData/Foliage_tree_-1_0_MapData.json` |
| 214 | `Resource/Character/DarkKnightNoneSword/SKM_DKF_Full.gltf` | `DarkKnightNoSword:SKM_DKF_Full.gltf` |
| 215 | `Resource/Character/DarkKnightNoneSword/animations/Sit_idle.gltf` | `DarkKnightNoSword:animations/Sit_idle.gltf` |
| 308 | `Resource/Sound/Monster Hunter Wilds Main Theme.mp3` | `Sound:Monster Hunter Wilds Main Theme.mp3` |
| 312 | `Resource/Sound/monster_hunter_ost.mp3` | `Sound:monster_hunter_ost.mp3` |

### `Client/Client/Tool_Scene.cpp` (5)

| 줄 | 현재 문자열 | 별칭(안) |
|---|---|---|
| 17 | `Resource/SkyBox/` | `SkyBox:` |
| 18 | `farmland/farmland_skybox.dds` | (build_skybox 인자 4.5) |
| 19 | `farmland/farmland_specular.dds` | (build_skybox 인자 4.5) |
| 20 | `farmland/farmland_diffuse.txt` | (build_skybox 인자 4.5) |
| 21 | `BRDF.dds` | (build_skybox 인자 4.5) |

### `Client/Client/UIFrameShader.cpp` (2)

| 줄 | 현재 문자열 | 별칭(안) |
|---|---|---|
| 35 | `UI_Frame_Shader.hlsl` | (셰이더: Resolve(Shader) 유지 또는 Shaders:) |
| 40 | `UI_Frame_Shader.hlsl` | (셰이더: Resolve(Shader) 유지 또는 Shaders:) |

### `Client/Client/UIShader.cpp` (2)

| 줄 | 현재 문자열 | 별칭(안) |
|---|---|---|
| 35 | `UI_Shader.hlsl` | (셰이더: Resolve(Shader) 유지 또는 Shaders:) |
| 40 | `UI_Shader.hlsl` | (셰이더: Resolve(Shader) 유지 또는 Shaders:) |

### `Server/Server/LuaManager.cpp` (4)

| 줄 | 현재 문자열 | 별칭(안) |
|---|---|---|
| 92 | `NPC_Data.lua` | `Lua:NPC_Data.lua` |
| 127 | `QuestData.lua` | `Lua:QuestData.lua` |
| 160 | `LeverData.lua` | `Lua:LeverData.lua` |
| 382 | `PlayerData.lua` | `Lua:PlayerData.lua` |

### `Server/Server/Room.cpp` (1)

| 줄 | 현재 문자열 | 별칭(안) |
|---|---|---|
| 2787 | `physics_dump.bin` | (Saved 유지) |

### `Server/Server/server.cpp` (5)

| 줄 | 현재 문자열 | 별칭(안) |
|---|---|---|
| 242 | `MainLandscape_Meshes/Landscape_-1_-1_MapData/Landscape_-1_-1_ExportedClientData.json` | `LandscapeMeshes:Landscape_-1_-1_MapData/Landscape_-1_-1_ExportedClientData.json` |
| 247 | `MainLandscape_Meshes/Landscape_-1_0_MapData/Landscape_-1_0_ExportedClientData.json` | `LandscapeMeshes:Landscape_-1_0_MapData/Landscape_-1_0_ExportedClientData.json` |
| 251 | `World_Batch_glTF/Tile_X-1_Y-1/Tile_X-1_Y-1.json` | `WorldBatch:Tile_X-1_Y-1/Tile_X-1_Y-1.json` |
| 255 | `1-BossScene/Boss_Landscape_ExportedClientData.json` | `BossMap:Boss_Landscape_ExportedClientData.json` |
| 264 | `Resource/NavMesh2.obj` | `NavMesh:NavMesh2.obj` |
