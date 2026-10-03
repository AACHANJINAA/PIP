# 뼈 부착 콜라이더 구현 명세서

작성일: 2026-10-01
관련 문서: `HitFeel_Plan_KR.md` 4장 1단계 (1-1 ~ 1-3)

---

## 1. 목표

- 캐릭터별 뼈 자세를 `AnimationComponent`가 보관하고, 누구든 컴포넌트 순서와 무관하게 읽을 수 있게 한다.
- 기존 `PhysicsColliderComponent`에 **뼈 부착**과 **바디 없는 검사 전용 모드**를 추가한다.
- 플레이어 칼날에 캡슐 콜라이더를 붙인다.
- F8로 클라이언트 물리 상태 한 프레임을 기록해 JoltViewer에서 캡슐과 실제 칼날을 겹쳐 본다.

범위 밖: NPC 히트박스, 겹침 판정, 히트스톱 (계획 문서 2단계 이후).

---

## 2. 변경 파일 요약

| 파일 | 변경 내용 |
|---|---|
| `ReadGLTFMesh.h/.cpp` | 조인트 인덱스 조회, 조인트별 모델 행렬 출력, 프리미티브 CPU 형상 조회 |
| `AnimationComponent.h/.cpp` | 캐릭터별 뼈 자세 보관과 조회 함수 |
| `PhysicsColliderComponent.h/.cpp` | `attach_to_bone`, `BodyMode`, `late_update`, 월드 변환 보관, `get_shape()` 수정 |
| `PhysicsDebugCapture.h/.cpp` (신규) | 클라이언트 한 프레임 기록 |
| `GameFramework.cpp` | 프레임 끝에서 기록 처리 호출 |
| `MainPlayerScript.cpp` | 칼날 캡슐 부착, F8 연동, 평타 자동 연속 기록 |

---

## 3. 구현 명세

### 3.1 `ReadGLTFMesh`

```cpp
// 스킨 조인트 순서(_skeleton, GPU 팔레트 순서)에서의 인덱스. 없으면 -1
int get_joint_index_by_name(const std::string& name) const;

// 기존 벡터 버전에 출력 인자 추가 (기본값 nullptr이라 기존 호출 영향 없음)
void update_animation(float& delta_time, std::string animation_name,
                      std::vector<XMFLOAT4X4>& bone_transforms, bool _isLoop = true,
                      std::vector<XMFLOAT4X4>* out_joint_model_matrices = nullptr);

// 재질 이름으로 프리미티브를 찾아 CPU 정점 위치와 인덱스를 복사 (디버그 기록용)
bool get_primitive_geometry(const std::string& material_name,
                            std::vector<XMFLOAT3>& out_positions,
                            std::vector<UINT>& out_indices) const;
```

- `get_joint_index_by_name`: `_skeleton[i]._name`을 검색해 `i`를 반환한다. 기존 `get_bone_index_by_name`은 **노드** 인덱스를 반환하므로 구분한다.
- `out_joint_model_matrices`
  - 크기는 `_joints.size()`이고, `i`번째에 `_nodes[_joints[i]]._global_transform`을 복사한다.
  - **전치하지 않은** DirectX 행 벡터 규약 그대로 저장한다. `bone_transforms`(GPU용 전치 행렬)와 다르다.
  - T-Pose 분기에서도 노드 계층 갱신 후 같은 방식으로 채운다.
- `get_primitive_geometry`
  - `_material_names[primitive->_materialIndex] == material_name`인 첫 프리미티브를 대상으로 한다.
  - 스킨 메쉬는 `_skinned_vertices`, 아니면 `_vertices`에서 위치만 꺼낸다.

### 3.2 `AnimationComponent`

```cpp
// 이 캐릭터의 현재 뼈 행렬 (모델 공간). 자세가 없거나 뼈가 없으면 false
bool try_get_bone_model_matrix(const std::string& bone_name, XMFLOAT4X4& out) const;

// 위 행렬 × 오브젝트 월드 행렬
bool try_get_bone_world_matrix(const std::string& bone_name, XMFLOAT4X4& out) const;

// 이번 실행에서 한 번이라도 자세가 계산되었는지
bool has_pose() const { return !_jointModelMatrices.empty(); }
```

추가 멤버:

```cpp
std::vector<XMFLOAT4X4> _jointModelMatrices;                          // 조인트별 모델 공간 행렬
mutable std::unordered_map<std::string, int> _jointIndexCache;        // 뼈 이름 → 조인트 인덱스
mutable const Mesh* _jointIndexCacheMesh = nullptr;                   // 캐시를 만든 메쉬
```

동작:

- `late_update`에서 `update_animation(..., &_jointModelMatrices)`로 함께 채운다. `_bonePaletteSize == 0`이면 채우지 않는다(스키닝 없는 오브젝트).
- 조인트 인덱스 캐시는 현재 애니메이션 메쉬가 바뀌면 비운다(`_jointIndexCacheMesh` 비교).
- 보관값은 그 캐릭터의 `late_update` 이후부터 다음 프레임 `late_update` 전까지 유효하다. 업데이트 단계에서 읽으면 직전 프레임 자세다.

### 3.3 `PhysicsColliderComponent`

추가·변경 인터페이스:

```cpp
enum class BodyMode { Body, QueryOnly };

void initialize(ShapeType type, const XMFLOAT3& size,
                const f3& center = {0,0,0},
                const f3& rotation_offset = {0,0,0},
                bool isSensor = true,
                BodyMode bodyMode = BodyMode::Body);   // 추가 (기본값으로 기존 호출 영향 없음)

void attach_to_bone(const std::string& bone_name);      // 같은 오브젝트의 AnimationComponent 뼈에 부착
void detach_from_bone();                                 // 오브젝트 트랜스폼 부착으로 복귀
const std::string& attached_bone() const;

void late_update(float deltaTime) override;              // 월드 변환 계산

const JPH::Shape* get_shape() const { return _shape.GetPtr(); }   // 반환문 누락 수정
bool has_world_transform() const;
const JPH::RMat44& world_transform() const;              // 현재 프레임 (스케일 제거됨)
const JPH::RMat44& prev_world_transform() const;         // 직전 프레임
BodyMode body_mode() const;
```

월드 변환 계산 (`late_update`):

```
local  = RotationRollPitchYaw(rotation_offset[도]) × Translation(center)
basis  = 뼈 부착이면  boneModel × ownerWorld
         아니면       ownerWorld
world  = local × basis            // DirectX 행 벡터 규약
```

- 계산 결과에서 **스케일을 제거**하고 회전과 위치만 `JPH::RMat44`로 보관한다. Jolt 모양은 행렬 스케일을 반영하지 않기 때문이다. 스케일이 1에서 벗어나면 디버그 로그를 한 번 남긴다.
- 직전 값을 `_prevWorldTransform`으로 옮긴 뒤 새 값을 저장한다. 첫 프레임은 현재값과 같게 둔다.
- 뼈를 찾지 못하면 `has_world_transform()`은 false이고, 한 번만 에러 로그를 남긴다.
- 기존 오브젝트 부착 계산과의 차이: 기존 `fixed_update`는 회전 오프셋을 `localRot × 오브젝트 회전`으로, 중심을 월드 행렬로 변환한다. 새 식은 같은 결과를 행렬 하나로 계산한다.

`BodyMode`별 동작:

| 항목 | `Body` (기존) | `QueryOnly` (신규) |
|---|---|---|
| Jolt 바디 생성 | 함 | 안 함 |
| `fixed_update` | 바디를 `MoveKinematic` | 아무것도 안 함 |
| 충돌 콜백 | `PhysicsManager`가 전달 | 없음 |
| `set_active` | 바디 활성/비활성 | 플래그만 변경 |
| 소멸자 | 바디 제거 | 제거할 바디 없음 |

- `Body` 모드에서 뼈에 부착하면 `fixed_update`가 보관된 월드 변환으로 바디를 옮긴다. 물리 스텝이 `late_update`보다 먼저 돌아서 한 프레임 늦는다. 이번 작업에서는 `QueryOnly`만 쓰므로 영향이 없다.
- 기존 사용처(무기 소켓 오브젝트, `Chess_Scene` NPC)는 기본값으로 동작이 바뀌지 않는다.

### 3.4 `PhysicsDebugCapture` (신규, 클라이언트 싱글톤)

```cpp
class PhysicsDebugCapture : public Singleton<PhysicsDebugCapture>
{
public:
    void request_capture();                       // 다음 프레임 끝에 한 프레임 기록
    void process_end_of_frame();                  // GameFramework에서 매 프레임 호출

    // 기록 시 함께 그릴 기준 형상: owner의 bone에 강체로 붙은 프리미티브
    void add_reference_primitive(GameObject* owner, const std::string& material_name,
                                 const std::string& bone_name);
    void remove_reference_primitives(GameObject* owner);

    void close_session();                         // 파일 닫기 (종료 시 호출)
};
```

- 전체 구현을 `#ifdef JPH_DEBUG_RENDERER`로 감싼다. 정의되지 않은 빌드(Release)에서는 모든 함수가 빈 함수다.
- 파일: 실행 폴더의 `client_physics_dump.bin`. 서버와 같이 첫 기록 때 세션을 열고, 기록할 때마다 프레임을 이어 붙인다. 뷰어에서 프레임 단위로 넘겨 볼 수 있다.
- 호출 위치: `GameFramework`의 `update_game_logic`, `update_physics` 다음, 렌더링 전. 모든 `late_update`가 끝난 시점이라 화면 자세와 일치한다.

한 프레임에 기록하는 내용:

| 내용 | 색 | 방법 |
|---|---|---|
| Jolt 바디 전체 | 모양 종류별 | `PhysicsSystem::DrawBodies` |
| `QueryOnly` 콜라이더 | 초록 | 각 콜라이더의 `world_transform()`으로 모양 `Draw` |
| `QueryOnly` 콜라이더 직전 프레임 | 어두운 초록 | `prev_world_transform()` (2단계 보간 확인용) |
| 부착 뼈 축 | RGB | `DrawCoordinateSystem` |
| 기준 프리미티브 | 흰색 와이어 | 정점 × 뼈 월드 행렬, 삼각형마다 `DrawWireTriangle` |

- 기준 프리미티브의 형상은 등록 시 `get_primitive_geometry`로 한 번 복사해 둔다. 칼은 정점 3087개라 기록 비용이 작다.
- `QueryOnly` 콜라이더 목록은 `ObjectManager`의 전체 오브젝트를 돌며 `PhysicsColliderComponent`를 찾는다. 기록은 디버그 키를 누를 때만 일어나므로 비용은 신경 쓰지 않는다.

### 3.5 `MainPlayerScript`

- `awake`의 무기 설정 다음에 칼날 캡슐 콜라이더를 붙이고, 칼 프리미티브를 기준 형상으로 등록한다(4.1).
- F8 처리에 `PhysicsDebugCapture::request_capture()`를 추가한다. 서버 스냅샷 요청은 그대로 둔다.
- **평타 자동 연속 기록**: 디버그 플래그(`_debugCaptureAttack`, F7로 토글)가 켜져 있으면 평타 진행도가 30%, 45%, 60%를 넘는 프레임마다 `request_capture()`를 호출한다. 진행도는 기존 `anim_progress / duration`을 쓴다.
- 소켓 오브젝트와 그 충돌체(`LongswordScript`)는 건드리지 않는다.

---

## 4. 사용법

### 4.1 뼈에 콜라이더 붙이기

```cpp
// 플레이어 칼날 캡슐 (ik_hand_r 기준, 칼 메쉬 분석값)
constexpr float kBladeRadius = 0.1f;
_bladeCollider = owner->add_component<PhysicsColliderComponent>();
_bladeCollider->initialize(
    PhysicsColliderComponent::ShapeType::Capsule,
    { kBladeRadius, 0.647f - kBladeRadius, 0.0f },   // x = 반지름, y = 반높이
    { 0.0f, 0.0f, 0.495f },                           // 뼈 좌표계 중심
    { 90.0f, 0.0f, 0.0f },                            // Y축 캡슐을 +Z(칼끝) 방향으로
    true,
    PhysicsColliderComponent::BodyMode::QueryOnly);
_bladeCollider->attach_to_bone("ik_hand_r");

// 검증용 기준 형상 등록 (디버그 빌드에서만 효과 있음)
PhysicsDebugCapture::instance()->add_reference_primitive(owner.get(), "M_DKF_Sword", "ik_hand_r");
```

- `AnimationComponent`가 있는 오브젝트에만 뼈 부착을 쓸 수 있다.
- `add_component` 순서상 `AnimationComponent` 뒤에 붙으므로 같은 프레임 자세를 읽는다.
- 오브젝트 트랜스폼에 붙이려면 `attach_to_bone`을 호출하지 않으면 된다.

### 4.2 콜라이더 위치 읽기 (2단계 판정에서 쓸 형태)

```cpp
if (_bladeCollider->has_world_transform())
{
    const JPH::Shape* shape = _bladeCollider->get_shape();
    JPH::RMat44 now  = _bladeCollider->world_transform();
    JPH::RMat44 prev = _bladeCollider->prev_world_transform();
    // prev → now 사이를 보간하며 CollideShape로 겹침 검사 (2단계에서 구현)
}
```

- 값은 `late_update`에서 갱신된다. 업데이트 단계에서 읽으면 직전 프레임 값이다.

### 4.3 뼈 행렬 직접 읽기

```cpp
XMFLOAT4X4 handWorld;
if (anim->try_get_bone_world_matrix("ik_hand_r", handWorld))
{
    // 이펙트 위치 등에 사용
}
```

- 공용 메쉬의 `get_socket_transform`과 달리 다른 캐릭터 자세에 덮어써지지 않는다.

### 4.4 기록하고 확인하기

1. 클라이언트를 Debug로 실행한다.
2. **F8**: 클라이언트와 서버가 각각 한 프레임을 기록한다. 클라이언트 파일은 실행 폴더의 `client_physics_dump.bin`이다.
3. **F7**: 평타 자동 연속 기록을 켠다. 켠 상태로 평타를 치면 30%, 45%, 60% 세 프레임이 기록된다. 다시 누르면 끈다.
4. `Jolt/JoltViewer/JoltViewer.exe <파일 경로>`로 연다. 인자는 공백으로 나뉘므로 경로에 공백이 없어야 한다. 파일은 클라이언트 작업 폴더에 생긴다(VS에서 실행하면 보통 `Client/Client`).
5. 기록할 때마다 파일을 flush하므로 게임을 켜 둔 상태에서도 바로 열 수 있다. 세션(파일)은 클라이언트 종료 시 닫히고, 다음 실행에서 첫 기록 때 새로 만든다(덮어씀).
6. 기록은 Debug와 Release 모두 동작한다. 클라이언트 Release는 디버그 렌더러를 켠 별도 Jolt 라이브러리를 쓴다(6장 참고).

---

## 5. 검증 체크리스트

- [ ] 기존 동작 유지: 무기 소켓 오브젝트, 대검 스킬 연출, `Chess_Scene` NPC 충돌이 그대로다.
- [ ] 대기 자세에서 초록 캡슐이 흰 칼날을 감싼다.
- [ ] 평타 30%, 45%, 60% 자세에서 캡슐이 칼날을 따라간다.
- [ ] 칼끝과 손잡이 끝이 캡슐 밖으로 나오지 않는다.
- [ ] `ik_hand_r` 축의 +Z가 칼끝 방향이다.
- [ ] 다른 플레이어나 더미가 있을 때도 내 캡슐이 내 칼에 붙어 있다(공유 문제 해결 확인).
- [x] Debug/Release 빌드 성공 (Release도 `ReleaseDebugRenderer` Jolt로 기록 가능).
- [ ] 캡슐 반지름 확정 (칼날만 0.1, 코등이 포함 0.13).

---

## 6. 구현 전 확인할 것과 위험 요소

| 항목 | 내용 | 대응 |
|---|---|---|
| Jolt 라이브러리 (해결) | 기존 `Jolt/lib/Debug`(/MTd)에는 디버그 렌더러가 있고, `Jolt/lib/Release`(/MT)는 Jolt CMake의 `Distribution` 구성과 같아 디버그 렌더러가 없다. Debug Jolt는 CRT(/MTd vs /MT)가 달라 Release에 링크할 수 없다 | PIP에 포함된 Jolt 소스와 완전히 일치하는 커밋(`JoltPhysics` 리포 `e486f5b5`, 5.5.1-dev)을 찾아 CMake `Release` 구성(최적화 + `JPH_DEBUG_RENDERER` + `JPH_PROFILE_ENABLED` + `JPH_FLOATING_POINT_EXCEPTIONS_ENABLED`, 정적 CRT, DX12 컴퓨트 포함, Vulkan 제외)으로 빌드해 `Jolt/lib/ReleaseDebugRenderer`에 두었다. 클라이언트 Release만 이 라이브러리와 정의를 쓰고, 서버는 기존 `lib/Release`를 그대로 쓴다 |
| JoltViewer 파일 열기 (해결) | Jolt 소스 확인 결과 실행 인자로 파일 경로를 받는다(`JoltViewer <파일>`). 인자를 공백으로 나누므로 경로에 공백이 있으면 안 된다 | 4.4에 반영 |
| 콜라이더 여러 개 | `PhysicsManager`는 충돌 알림 때 첫 번째 콜라이더만 찾는다 | 칼날 캡슐은 `QueryOnly`라 무관. NPC 부위별 히트박스(2단계)에서 해결 |
| 스케일 | Jolt 모양은 행렬 스케일을 반영하지 않는다 | 스케일 제거 후 경고 로그. 스케일 있는 NPC는 2단계에서 모양 크기에 반영 |
| `Body` 모드 뼈 부착 | 물리 스텝이 `late_update`보다 먼저라 한 프레임 늦다 | 이번 범위는 `QueryOnly`만 사용 |

---

## 7. 현재 상태와 알려진 문제 (2026-10-03)

1-1 ~ 1-3 구현을 커밋했고, JoltViewer로 첫 기록을 확인했다.

### 7.1 칼날 캡슐 방향이 반대 (미해결)

- JoltViewer에서 초록 캡슐이 손잡이에서 칼날 **반대쪽**으로 뻗어 있었다.
- 원인: 엔진의 glTF 로더가 오른손 → 왼손 좌표계 변환으로 정점과 노드 행렬의 Z를 뒤집는다(`ReadGLTFMesh.cpp`의 `-positions[i].z`, `z_flip * m * z_flip`). 칼 메쉬 분석값(+Z, z = -0.152 ~ 1.142)은 glTF 원본 기준이라, 엔진 안에서는 칼이 뼈 기준 **-Z** 방향(z = -1.142 ~ 0.152)에 놓인다.
- 수정 예정: 캡슐 중심을 (0, 0, 0.495)에서 **(0, 0, -0.495)**로 바꾼다. 회전(X축 90도)은 캡슐 축만 Z에 맞추므로 그대로 둔다. 이 문서 4.1과 `HitFeel_Plan_KR.md`의 값도 함께 고친다.
- 참고: 같은 기록에서 노란 캡슐은 기존 소켓 오브젝트의 무기 충돌체(`LongswordScript`)다. 공격 판정이 켜졌을 때만 따라 움직여서 마지막 활성 위치에 멈춰 있다. 판정에 쓰이지 않는다.

### 7.2 커밋 용량

- `Jolt/lib/ReleaseDebugRenderer/Jolt.lib`가 79MB다. GitHub는 50MB 초과 파일에 경고를 띄우지만 100MB 미만이라 푸시는 된다.
- Jolt lib는 LFS가 아니라 일반 git 파일로 관리된다(기존 Debug 67MB, Release 24MB와 같음).
- `.gitignore`의 Jolt 예외를 `!Jolt/lib/*/`로 바꿔 구성 폴더 이름과 상관없이 추적되게 했다.

### 7.3 원점 기준 기록 없음 (미해결)

- 기록은 세계 좌표 그대로 저장된다. 플레이 위치가 원점에서 멀면(예: (-215, 8, -366)) 뷰어 카메라에서 빈 화면처럼 보인다.
- 임시 방법: 기록 파일의 선, 삼각형, 라벨, 형상 위치를 첫 프레임 칼날 중심만큼 빼서 사본을 만든다(`client_physics_dump*.bin`은 git 무시 대상). 형상 정의는 로컬 좌표라 그대로 둔다.
- 수정 예정: 클라이언트가 첫 기록 때의 플레이어 위치를 원점으로 잡고 모든 그림을 그만큼 옮겨 기록한다. 한 파일 안의 캡처는 같은 기준점을 써서 프레임 사이 움직임을 그대로 비교할 수 있게 한다.

### 7.4 서버 기록 수정 (해결)

- 증상: 서버 기록에서 첫 캡처 말고는 뷰어에서 제대로 재생되지 않았다.
- 원인 (테스트 프로그램으로 재현 확인):
  - 캡처마다 레코더를 새로 만들어 배치/형상 ID가 1부터 다시 매겨졌다. 형상 정의는 처음 쓸 때만 기록되고, 메쉬·컨벡스 헐·하이트필드는 디버그 형상을 모양 안에 캐시한다. 그래서 뷰어에서 새로 등장한 모양이 다른 형상으로 그려졌다.
  - 파일을 flush하지 않아, 서버 실행 중에 뷰어로 열면 마지막 캡처가 잘렸다.
- 수정: 세션 레코더 하나를 계속 쓰고 캡처마다 flush한다. 0번 방에서만 기록한다(디버그 렌더러는 프로세스당 하나를 전제).
- 클라이언트 `PhysicsDebugCapture`는 처음부터 같은 방식이라 해당 없다.

