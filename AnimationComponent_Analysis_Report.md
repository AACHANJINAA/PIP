# AnimationComponent 및 Skinned Mesh 성능 분석 보고서

본 문서는 현재 클라이언트에서 발생하고 있는 **프레임 드랍(Spike)** 및 **"디바이스 오류(TDR)"**의 근본 원인을 분석하고, 이를 해결하기 위한 기술적인 가이드를 제공하기 위해 작성되었습니다.

---

## 1. 문제 현상 요약
*   **프레임 스파이크:** 네트워크 패킷 처리 시 메인 스레드가 최대 **70ms~80ms** 동안 멈추는 현상 발생. (정상 범위: 1~3ms)
*   **디바이스 오류:** 특정 상황(대량의 NPC 상태 변화 등)에서 화면이 멈추고 그래픽 드라이버가 리셋됨.
*   **원인:** 패킷 처리 루프 내에서 **GPU 동기화(Blocking)**와 **런타임 리소스 생성**이 빈번하게 발생.

---

## 2. 핵심 문제점 분석 (코드 레벨)

### 문제점 A: `WaitForGpuComplete()`의 오용 (가장 치명적)
`AnimationComponent.cpp`의 `create_bone_palette_buffer` 함수 시작 부분에 있는 아래 코드가 모든 성능 문제의 시발점입니다.

```cpp
void AnimationComponent::create_bone_palette_buffer(const std::shared_ptr<Mesh>& want_mesh) {
    GameFramework::instance()->WaitForGpuComplete(); // <--- !! 절대 금기 !!
    // ...
}
```

*   **이유:** `WaitForGpuComplete()`는 GPU가 현재 큐에 쌓인 모든 명령을 처리할 때까지 **CPU를 완전히 멈추게(Sleep)** 합니다.
*   **결과:** 서버에서 100마리의 NPC 이동 패킷을 '배치(Batch)'로 보낼 때, 그중 30마리만 상태가 변해도 CPU는 GPU를 **30번이나 처음부터 끝까지 기다려야 합니다.** 이 과정에서 수십 ms의 시간이 허비되며 메인 스레드가 마비됩니다.

### 문제점 B: 본 팔레트 버퍼(Bone Palette Buffer)의 불필요한 재생성
상태(IDLE <-> WALK)가 바뀔 때마다 기존 버퍼를 버리고 새로 만들고 있습니다.

```cpp
if (_bone_palette_buffer) {
    _bone_palette_buffer.Reset(); // 기존 버퍼 삭제
}
// ...
HRESULT hr = GameFramework::instance()->device()->CreateCommittedResource( // 새로운 메모리 할당
    &heapProps, D3D12_HEAP_FLAG_NONE, &bufferDesc, ...
);
```

*   **이유:** DX12에서 `CreateCommittedResource`는 운영체제 커널까지 다녀오는 **매우 무거운 작업**입니다.
*   **결과:** NPC 100마리가 돌아다니며 상태를 바꿀 때마다 메모리 할당/해제가 반복됩니다. 이는 메모리 단편화를 유발하고 프레임 타임을 예측 불가능하게 만듭니다. 사실상 본(Bone)의 개수가 같다면 버퍼의 크기도 같으므로, **한 번 만든 버퍼는 계속 재사용(Reuse)**해야 합니다.

### 문제점 C: "디바이스 오류"의 정체 (TDR 현상)
*   **현상:** 윈도우 운영체제는 그래픽 드라이버가 **2초 이상** 응답하지 않으면 시스템을 보호하기 위해 드라이버를 강제로 리셋합니다. 이를 TDR(Timeout Detection and Recovery)이라고 합니다.
*   **원인:** `process_queued_packets` 루프 내에서 수많은 `WaitForGpuComplete`가 누적되어 메인 스레드가 2초 가까이 혹은 그 이상 점유될 경우, 윈도우는 "애플리케이션이 뻗었다"고 판단하고 디바이스를 강제 종료시킵니다. 이것이 사용자에게는 **"디바이스 오류"**로 표시되는 것입니다.

---

## 3. 수정 가이드 (Action Plan)

담당자분께서는 아래의 순서대로 수정을 진행해 주시기를 강력히 권장합니다.

### 1단계: `WaitForGpuComplete()` 제거
업데이트 루프(Update, LateUpdate) 및 핸들러 로직 중간에서 이 함수를 호출하는 것을 즉시 중단해야 합니다. DX12의 리소스 교체는 Fence를 이용한 비동기 해제 방식을 써야 하지만, 우선은 이 호출을 빼는 것만으로도 70ms 스파이크가 1ms 수준으로 떨어질 것입니다.

### 2단계: 버퍼 재사용 로직 도입
`create_bone_palette_buffer` 함수를 다음과 같이 수정하여, 필요한 크기가 현재 버퍼보다 작거나 같으면 기존 것을 그대로 쓰도록 변경해 주세요.

```cpp
void AnimationComponent::create_bone_palette_buffer(const std::shared_ptr<Mesh>& want_mesh) {
    // 1. 필요한 버퍼 크기 계산 (256 정렬 포함)
    // ... 기존 joint_size 기반 계산 로직 ...

    // 2. [핵심] 기존 버퍼가 있고 크기가 충분하다면 생성 Skip!
    if (_bone_palette_buffer != nullptr) {
        if (_bone_palette_buffer->GetDesc().Width >= buffer_size) {
            return; // 이미 있는 걸 쓰면 됩니다.
        }
        _bone_palette_buffer.Reset(); // 더 큰 공간이 필요할 때만 재할당
    }

    // 3. CreateCommittedResource 호출 (최초 1회 또는 크기 변경 시에만 실행됨)
    // ...
}
```

### 3단계: 상태 변경 시 불필요한 메쉬 교체 방지
현재 `IDLE`과 `WALK` 애니메이션을 위해 아예 `Mesh` 객체 자체를 갈아끼우고 있는데, 만약 두 애니메이션이 동일한 스켈레톤(뼈 구조)을 공유한다면 메쉬를 바꿀 필요 없이 애니메이션 클립만 교체하는 것이 훨씬 효율적입니다.

---

## 4. 기대 효과
위 수정을 적용할 경우:
1.  패킷 처리 시간이 **70ms -> 1ms 미만**으로 단축됩니다.
2.  화면 끊김 현상(Stuttering)이 사라지고 부드러운 움직임이 가능해집니다.
3.  **"디바이스 오류"** 현상이 원천적으로 해결됩니다.

성능 최적화는 게임의 품질과 직결되는 중요한 요소입니다. 위 내용을 검토하시어 수정을 부탁드립니다. 추가 질문이 있으시면 언제든 말씀해 주세요.
