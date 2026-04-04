# 📜 [기술 설계서] 대규모 NPC 소환 최적화: 선형 할당기(Linear Allocator) 도입

## 1. 현황 분석 및 문제 진단 (The Problem)

현재 NPC 101마리 소환 시 발생하는 프리징의 핵심 원인은 **`AnimationComponent.cpp`**의 리소스 생성 로직에 있습니다.

### 🔍 문제 코드 분석 (`Client/Client/AnimationComponent.cpp`)
```cpp
void AnimationComponent::create_bone_palette_buffer(const std::shared_ptr<Mesh>& want_mesh)
{
    // [문제점 1] 매 NPC마다 개별 리소스 생성 (ID3D12Device::CreateCommittedResource)
    // 101마리 소환 시 커널 모드 진입 및 드라이버 할당 로직이 101번 반복됨 (매우 무거운 작업)
    HRESULT hr = GameFramework::instance()->device()->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &bufferDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&new_buffer)
    );

    // [문제점 2] 매번 CPU-GPU 주소 공간 연결 (Map)
    new_buffer->Map(0, &readRange, reinterpret_cast<void**>(&_mapped_bone_data));
}
```

*   **진단**: `ObjectManager`가 한 프레임에 모든 NPC의 `awake()`를 호출할 때, `CreateCommittedResource`가 연달아 실행되면서 메인 스레드가 OS의 메모리 할당 처리를 기다리느라 멈추게 됩니다.

---

## 2. 해결 방안: 상수 버퍼 선형 할당기 (Linear Allocator)

리소스 생성 오버헤드를 **0**으로 만들기 위해 **"미리 크게 하나 만들어서 쪼개 쓰는"** 방식을 도입합니다.

### 🛠 핵심 기술: Dynamic Constant Buffer (링 버퍼)
1.  **사전 할당**: 게임 시작(또는 씬 로드) 시 약 16~32MB의 큰 `Upload Heap` 리소스를 딱 한 번만 생성합니다.
2.  **포인터 범핑**: NPC는 "나 뼈 60개분 주소 줘"라고 요청하면, 관리자는 현재 사용 중인 **오프셋(Offset)**만 더해서 즉시 주소를 넘겨줍니다. (포인터 덧셈 연산이라 0.00001ms 소요)
3.  **데이터 복사**: 모든 NPC가 하나의 거대한 버퍼 메모리를 공유하며, 각각 할당받은 오프셋 위치에 자신의 뼈 행렬을 `memcpy` 합니다.

---

## 3. 상세 구현 가이드 (Implementation Plan)

### Step 1: `LinearAllocator` 클래스 구현
이 클래스는 거대한 메모리 덩어리를 관리하며, `GameFramework`에 위치합니다.

```cpp
// LinearAllocator.h (예시)
class LinearAllocator {
public:
    struct Allocation {
        void* cpuPtr;
        D3D12_GPU_VIRTUAL_ADDRESS gpuAddr;
    };

    LinearAllocator(ID3D12Device* device, size_t size) {
        auto heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(size);
        device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&_resource));
        _resource->Map(0, nullptr, &_cpuBase);
        _gpuBase = _resource->GetGPUVirtualAddress();
        _totalSize = size;
    }

    Allocation allocate(size_t size) {
        size_t alignedSize = (size + 255) & ~255; // 256바이트 정렬 필수
        if (_offset + alignedSize > _totalSize) return { nullptr, 0 };

        Allocation alloc = { (uint8_t*)_cpuBase + _offset, _gpuBase + _offset };
        _offset += alignedSize;
        return alloc;
    }

    void reset() { _offset = 0; }

private:
    ComPtr<ID3D12Resource> _resource;
    void* _cpuBase = nullptr;
    D3D12_GPU_VIRTUAL_ADDRESS _gpuBase = 0;
    size_t _totalSize = 0;
    size_t _offset = 0;
};
```

### Step 2: `AnimationComponent` 리팩토링
개별 `ID3D12Resource` 소유권을 제거하고 할당기에서 주소를 빌려오도록 수정합니다.

#### [AnimationComponent.h]
```cpp
class AnimationComponent : public Behavior {
private:
    // [삭제] ComPtr<ID3D12Resource> _bone_palette_buffer;
    // [삭제] UINT8* _mapped_bone_data;
    
    // [추가] 이번 프레임에 할당받은 주소만 기억
    D3D12_GPU_VIRTUAL_ADDRESS _currentBoneGPUAddr = 0;
    size_t _bonePaletteSize = 0; 
};
```

#### [AnimationComponent.cpp]
```cpp
void AnimationComponent::late_update(float dt) {
    // 1. 애니메이션 뼈 계산 (기존과 동일)
    // 2. 할당기에서 메모리 빌리기 (리소스 생성 오버헤드 0!)
    auto alloc = GameFramework::instance()->linear_allocator()->allocate(_bonePaletteSize);
    
    // 3. 빌린 공간에 행렬 데이터 memcpy
    memcpy(alloc.cpuPtr, _boneTransforms.data(), _bonePaletteSize);
    
    // 4. GPU 주소 업데이트 (렌더러가 사용할 주소)
    _currentBoneGPUAddr = alloc.gpuAddr;
}
```

### Step 3: `GameFramework` 라이프사이클 관리
매 프레임마다 할당기를 초기화해줍니다.

#### [GameFramework.cpp]
```cpp
void GameFramework::FrameAdvance() {
    // [추가] 프레임 시작 시 할당기 리셋 (창고 비우기)
    _linearAllocator->reset();

    // 1. Logic Update (각 NPC들이 할당기에서 바구니를 빌려 뼈 행렬 복사)
    update_game_logic(deltaTime);

    // 2. Render (Renderer는 AnimationComponent의 _currentBoneGPUAddr를 바인딩)
    Renderer::instance()->render(...);
}
```

---

## 4. 기대 효과 (The Impact)

1.  **소환 프리징 완전 제거**: 101마리 소환 시 OS 커널 호출이 **101번에서 0번**으로 줄어들어 즉시 소환됩니다.
2.  **메모리 효율 극대화**: 수천 개의 작은 리소스 조각이 아닌 하나의 큰 연속된 메모리를 사용하여 GPU 메모리 파편화를 방지합니다.
3.  **성능 향상**: `CreateCommittedResource`와 `Map/Unmap` 비용이 사라지며, CPU 캐시 적중률이 상승합니다.

---

**Senior Engineer's Note**: 
이 방식은 DX12 엔진의 성능을 100% 끌어쓰기 위한 표준 아키텍처입니다. 특히 101마리 이상의 대규모 전투가 예정되어 있다면, 이 리팩토링은 선택이 아닌 필수입니다.
