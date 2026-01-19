# PIP 프로젝트: 애니메이션 시스템 리팩토링 계획서 (메쉬와 애니메이션 상태 분리)

## 1. 개요 및 문제 분석

### 현재 상황
현재 PIP 프로젝트의 `ReadGLTFMesh` 클래스는 두 가지 역할을 동시에 수행하고 있습니다:
1.  **불변 데이터 (Immutable Data):** 정점(Vertex), 인덱스(Index), UV, 본(Bone) 구조, 애니메이션 키프레임 데이터 등. 모든 캐릭터가 공유할 수 있는 데이터입니다.
2.  **가변 상태 (Mutable State):** 현재 재생 중인 애니메이션 시간, 계산된 본 행렬(Bone Matrices), 그리고 이를 GPU에 전달하는 상수 버퍼(`_bone_palette_buffer`).

### 문제점
*   **동기화 버그:** 여러 플레이어(`OtherPlayer`)가 동일한 메쉬 파일(예: `Brute_Idle.gltf`)을 `ResourceManager`를 통해 공유하여 사용하면, `ReadGLTFMesh` 내부의 `_bone_palette_buffer`도 하나만 존재하게 됩니다. A 플레이어가 움직이면 버퍼가 갱신되고, B 플레이어도 같은 버퍼를 사용하여 렌더링되므로 **모든 플레이어가 똑같은 동작을 취하는 문제**가 발생합니다.
*   **메모리 낭비:** 이를 피하기 위해 각 플레이어마다 메쉬를 새로 로드하면, 정점 데이터 등 공유 가능한 데이터까지 중복으로 메모리에 올라가 낭비가 심해집니다.

### 목표
상용 엔진(Unreal, Unity)과 같이 **데이터(Resource)**와 **상태(Component)**를 분리합니다.
*   **`ReadGLTFMesh`:** 순수한 데이터 저장소로 변경 (공유 가능).
*   **`AnimationComponent`:** 각 인스턴스별 애니메이션 상태와 GPU 본 버퍼 관리 (인스턴스별 독립).

---

## 2. 변경된 아키텍처 구조

| 클래스 | 역할 변경 | 주요 멤버 변수/함수 변화 |
| :--- | :--- | :--- |
| **`ReadGLTFMesh`** | **[데이터 컨테이너]**<br>정점, 인덱스, 애니메이션 클립 원본 보관.<br>렌더링 기능은 유지하되, 외부에서 본 버퍼를 받아야 함. | **삭제:** `_bone_palette_buffer`, `_final_bone_transforms`, `_current_animation_time`<br>**수정:** `render_skinned(..., ID3D12Resource* boneBuffer)` |
| **`AnimationComponent`** | **[상태 관리자]**<br>실제 본 행렬 계산 및 GPU 버퍼 관리.<br>매 프레임 자신의 자세를 계산하여 버퍼에 업로드. | **추가:** `_bone_palette_buffer`, `_final_bone_transforms`<br>**이동:** `update_animation()` 로직을 여기로 가져옴. |

---

## 3. 단계별 상세 작업 가이드

### [Step 1] `ReadGLTFMesh` 다이어트 (상태 제거)

`ReadGLTFMesh`에서 "변하는 값"들을 모두 제거하고, 외부에서 주입받도록 수정합니다.

#### 1-1. 헤더 파일 (`ReadGLTFMesh.h`) 수정
`_final_bone_transforms`, `_bone_palette_buffer`, `_current_animation_time` 멤버 변수를 제거하거나 `AnimationComponent`로 옮길 준비를 합니다. `render_skinned` 함수가 버퍼를 인자로 받도록 수정합니다.

```cpp
// ReadGLTFMesh.h

class ReadGLTFMesh : public Mesh {
public:
    // ... 기존 생성자 등 ...

    // [수정] 렌더링 시, 외부(컴포넌트)에서 관리하는 본 버퍼를 인자로 받습니다.
    void render_skinned(ID3D12GraphicsCommandList* commandList, ID3D12Resource* boneBuffer);

    // [삭제] update_animation은 이제 AnimationComponent가 담당합니다.
    // void update_animation(...); 

    // [추가] AnimationComponent가 데이터에 접근할 수 있도록 Getter가 필요할 수 있습니다.
    const std::vector<BoneInfo>& get_skeleton() const { return _skeleton; }
    const std::map<std::string, AnimationClip>& get_animations() const { return _animations; }
    const std::vector<NodeInfo>& get_nodes() const { return _nodes; } // NodeInfo 구조체도 공유 데이터로 봅니다.

    // ...
private:
    // [삭제] 멤버 변수 제거
    // std::vector<DirectX::XMFLOAT4X4> _final_bone_transforms;
    // ComPtr<ID3D12Resource> _bone_palette_buffer;
};
```

#### 1-2. 소스 파일 (`ReadGLTFMesh.cpp`) 수정
`update_animation` 구현부를 제거하고, `render_skinned`에서 멤버 변수 대신 인자로 받은 버퍼를 사용하도록 고칩니다.

```cpp
// ReadGLTFMesh.cpp

void ReadGLTFMesh::render_skinned(ID3D12GraphicsCommandList* commandList, ID3D12Resource* boneBuffer)
{
    // 멤버 변수 _bone_palette_buffer 대신 인자로 들어온 boneBuffer를 사용
    if (boneBuffer)
    {
        // 8번 루트 파라미터가 본 행렬 상수 버퍼라고 가정 (쉐이더 설정에 따라 다름)
        commandList->SetGraphicsRootConstantBufferView(8, boneBuffer->GetGPUVirtualAddress());
    }
}
```

---

### [Step 2] `AnimationComponent` 기능 확장 (상태 관리)

애니메이션 로직의 핵심을 이곳으로 옮깁니다. 각 캐릭터마다 하나씩 존재하는 컴포넌트이므로, 여기서 버퍼를 관리하면 안전합니다.

#### 2-1. 헤더 파일 (`AnimationComponent.h`) 수정

```cpp
// AnimationComponent.h

class AnimationComponent : public Behaviour
{
public:
    // ...
    virtual void update(float deltaTime) override; // late_update 대신 update 사용 권장

private:
    // [이동] ReadGLTFMesh에 있던 로직을 수행하기 위한 변수들
    std::vector<DirectX::XMFLOAT4X4> _final_bone_transforms;
    Microsoft::WRL::ComPtr<ID3D12Resource> _bone_palette_buffer; // 나만의 본 버퍼!

    // 현재 재생 중인 메쉬 데이터에 대한 참조 (어떤 뼈대 구조를 따를 것인가)
    std::weak_ptr<ReadGLTFMesh> _targetMesh; 

    // 내부 함수: 본 버퍼 생성 및 업데이트
    void create_bone_buffer(ID3D12Device* device, int boneCount);
    void update_bone_buffer();
    void calculate_bone_transforms(float deltaTime); // 구 update_animation 로직
};
```

#### 2-2. 소스 파일 (`AnimationComponent.cpp`) 구현

가장 중요한 부분입니다. `ReadGLTFMesh`의 `update_animation` 로직을 가져와서, `_targetMesh`의 데이터를 읽고 -> 계산 후 -> 내 `_bone_palette_buffer`에 씁니다.

```cpp
// AnimationComponent.cpp

void AnimationComponent::calculate_bone_transforms(float deltaTime)
{
    auto meshPtr = _targetMesh.lock();
    if (!meshPtr) return; // 메쉬가 없으면 패스

    // 1. 메쉬의 애니메이션 데이터(Clip, Keyframes) 가져오기
    const auto& animations = meshPtr->get_animations();
    const auto& skeleton = meshPtr->get_skeleton();
    
    // ... (기존 update_animation의 보간 로직 수행) ...
    // 주의: Node 계층 구조(_nodes)는 읽기 전용으로 써야 하므로, 
    // 각 노드의 현재 변환 행렬(Local/Global)은 AnimationComponent가 
    // 별도의 배열로 관리해야 할 수도 있습니다. 
    // (간단하게 하려면 _final_bone_transforms 계산에 집중)

    // 2. 최종 행렬 계산
    for (size_t i = 0; i < skeleton.size(); ++i) {
        // ... (InverseBindMatrix * GlobalTransform 계산) ...
        _final_bone_transforms[i] = ...; 
    }

    // 3. GPU 업로드
    if (_bone_palette_buffer) {
        void* mapped_data = nullptr;
        _bone_palette_buffer->Map(0, nullptr, &mapped_data);
        memcpy(mapped_data, _final_bone_transforms.data(), _final_bone_transforms.size() * sizeof(XMFLOAT4X4));
        _bone_palette_buffer->Unmap(0, nullptr);
    }
}
```

---

### [Step 3] 렌더링 파이프라인 연결 (`RenderComponent`)

마지막으로, 렌더링할 때 `AnimationComponent`가 들고 있는 버퍼를 `ReadGLTFMesh`에게 전달해줘야 합니다.

```cpp
// RenderComponent.cpp (또는 Renderer.cpp)

void RenderComponent::render(ID3D12GraphicsCommandList* commandList)
{
    if (!_mesh) return;

    // 내 게임 오브젝트에 애니메이션 컴포넌트가 있는지 확인
    ID3D12Resource* boneBuffer = nullptr;
    auto animComp = game_object()->get_component<AnimationComponent>();
    if (animComp) {
        boneBuffer = animComp->get_bone_buffer(); // Getter 필요
    }

    // 메쉬에게 그리라고 명령할 때 버퍼를 전달
    // (dynamic_cast로 스키닝 메쉬인지 확인 필요)
    auto gltfMesh = std::dynamic_pointer_cast<ReadGLTFMesh>(_mesh);
    if (gltfMesh) {
        gltfMesh->render_skinned(commandList, boneBuffer); 
        // 주의: render_skinned 내부에서 Draw 호출까지 한다면 구조 수정 필요.
        // 보통은 Pre-Render 단계에서 버퍼를 바인딩하고, Draw는 그 뒤에 함.
    }
    
    _mesh->render(commandList); // 기본 렌더링
}
```

---

## 4. 초보자를 위한 팁 & 주의사항

1.  **점진적 적용:** 한 번에 모든 것을 바꾸려다간 프로젝트가 멈출 수 있습니다. 먼저 `ReadGLTFMesh`를 복제해서 `ReadGLTFMesh_Shared` 같은 이름으로 만들고, 리팩토링을 진행하며 기존 클래스와 교체하는 방식이 안전합니다.
2.  **NodeInfo 관리:** 리팩토링 중 가장 까다로운 부분은 `NodeInfo`(각 관절의 현재 위치/회전) 관리입니다. `ReadGLTFMesh`에 있던 `_nodes` 배열은 이제 모든 캐릭터가 공유할 수 없습니다(각자 자세가 다르니까요).
    *   **해결책:** `AnimationComponent`가 자신만의 `std::vector<NodeInfo> _currentNodes`를 가지고 있어야 합니다. 초기화 시 `ReadGLTFMesh`로부터 복사해옵니다.
3.  **성능:** 매 프레임 `Map`/`Unmap`으로 버퍼를 업데이트하는 것은 괜찮습니다. 단, 버퍼 생성(`CreateCommittedResource`)은 초기화 단계에서 한 번만 해야 합니다.

## 5. 결론

이 리팩토링을 수행하면:
*   **버그 해결:** 플레이어들이 각자 다른 동작을 해도 서로 간섭하지 않습니다.
*   **성능 향상:** 메쉬 데이터(수 MB)는 한 번만 로드하고, 가벼운 본 행렬(수 KB)만 인스턴스별로 관리하여 메모리를 획기적으로 절약할 수 있습니다.
*   **확장성:** 추후 무기 장착, 파츠 교체 등의 기능을 구현할 때 훨씬 유연해집니다.