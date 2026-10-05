# PIP (Slay The Lord) 코드맵 색인

DirectX 12 자체 엔진 클라이언트 + IOCP 권위 서버(방마다 Jolt 물리)로 만드는 소울류 액션 RPG.
폴더별 코드맵은 실제 소스를 보고 작성했다(2026-10-04 기준). 기존 `GEMINI.md`들은 예전에 작성된 것으로 지금 코드와 다른 부분이 있다.

## 폴더별 코드맵

| 폴더 | 문서 | 내용 |
|---|---|---|
| `Client/Client` | [Client/Client/CODEMAP.md](Client/Client/CODEMAP.md) | 프레임 흐름, 엔티티·씬, 플레이어·NPC 스크립트, 애니메이션·메쉬, 렌더링, 물리, 네트워크·보간, 디버그 키 |
| `Server/Server` | [Server/Server/CODEMAP.md](Server/Server/CODEMAP.md) | 스레드 모델, 방, 스테이지, 엔티티·컴포넌트, AI, 맵·Lua 데이터, 패킷 처리 |
| `Common` | [Common/CODEMAP.md](Common/CODEMAP.md) | 패킷 프로토콜, 공용 시계·벡터·Jolt 변환, 지형 데이터 |

## 그 밖의 폴더

| 폴더 | 내용 |
|---|---|
| `Jolt/Jolt` | Jolt Physics 소스 (JoltPhysics `e486f5b5`, 5.5.1-dev) |
| `Jolt/lib/{Debug,Release,ReleaseDebugRenderer}` | Jolt 라이브러리. `ReleaseDebugRenderer`는 클라 Release용(디버그 렌더러 포함, MSVC 14.38, LTCG 없음). 재빌드 방법은 `기획 & 계획/BoneCollider_Spec_KR.md` 7.6 |
| `Jolt/JoltViewer` | 물리 기록 뷰어 소스(`-focus=x,y,z` 인자 추가), `JoltViewer.exe`, `RunViewer.py`(더블클릭 실행 도우미), `CMakeLists.txt`(빌드 방법은 `BoneCollider_Spec_KR.md` 7.5) |
| `Jolt/TestFramework` | 뷰어가 쓰는 창·DX12 렌더러·UI 프레임워크 (upstream 그대로) |
| `Jolt/Assets` | 뷰어 셰이더·폰트 |
| `Tools` | `EnsureUtf8Bom.ps1` (C++ 소스를 UTF-8 BOM으로 맞추는 수동 실행 스크립트), `deploy.py` (`PathManifest.json` 기준 배포 폴더 생성·사용 파일 검사) |
| `TextureTools` | 텍스처 변환 도구 (HDR/PNG/JPG → DDS, IBL 맵 생성, TextureCooker) |
| `StressTest` | 서버 부하 테스트 봇 클라이언트 (`BotSession`) |
| `기획 & 계획` | 설계·계획 문서 (아래) |

## 진행 중 설계 문서 (`기획 & 계획`)

| 문서 | 내용 | 상태 |
|---|---|---|
| `HitFeel_Plan_KR.md` | 타격감 개선 전체 계획 (진단, 1~4단계) | 1·2단계 완료, 3단계 진행 중 (히트스톱·타격음·카메라 킥·부위 리액션·대검 스킬 판정·타격 스파크 완료, 칼 잔상·충격파 남음) |
| `BoneCollider_Spec_KR.md` | 1단계: 뼈 부착 칼날 캡슐, 물리 기록, 뷰어 | 완료 |
| `MeleeHitPrediction_Spec_KR.md` | 2단계: 평타 예측 판정 (스윕, 피격 히트박스, `on_trigger_enter`) | 동작 확인 |
| `NetMotionSync_Design_KR.md` | 시계 동기화, 보간 버퍼, 넉백 모션 이벤트 | S1~S4 완료 |
| `ParticleSystem_Plan_KR.md` | 범용 GPU 파티클·VfxManager, 충격파 왜곡 | P0~P3 완료(클라 Lua, 기존 분리, 범용 GPU 파티클, Lua 프리셋·VfxManager·타격 스파크). 남은 것: PathManager 합칠 때 셰이더 경로, 이후 칼 잔상·충격파(4장 P4) |

## 공통 규칙

- 서버·클라 모두 `/utf-8` 컴파일. C++ 소스는 UTF-8 BOM, 셰이더(`*.hlsl`, `*.hlsli`)는 BOM 없는 UTF-8(셰이더 컴파일러가 BOM을 못 읽음). `.editorconfig`
- 경로는 `Common/PathManager.h`가 exe 위치로 계산(작업 폴더 무관). 별칭·배포 범위는 저장소 루트 `PathManifest.json`, 배포는 `python Tools/deploy.py`.
- `Common`을 바꾸면 서버와 클라를 둘 다 다시 빌드한다.
- 코드 구조를 바꾸면 해당 폴더 `CODEMAP.md`도 같이 고친다.
