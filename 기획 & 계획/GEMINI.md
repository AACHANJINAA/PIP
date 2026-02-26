# PIP (Slay The Lord: 구원자의 뒤틀린 웃음) - Project Overview

이 파일은 **PIP (Slay The Lord - STL)** 프로젝트의 기획, 아키텍처, 개발 컨벤션 및 핵심 기술 정보를 담고 있는 종합 지침서입니다.

## 📌 1. 게임 개요 (Game Overview)
- **제목**: Slay The Lord (S.T.L): 구원자의 뒤틀린 웃음
- **장르**: 3인칭 협력형 서바이벌 액션 RPG (소울라이크 스타일)
- **플랫폼**: PC (Windows)
- **핵심 컨셉**: "타락한 마나의 세계에서 잊힌 용사의 기억(무기)을 깨워 거짓된 신을 처단하라."
- **주요 특징**: 
  - 자체 제작 DX12 엔진 기반의 고정밀 액션.
  - IOCP 기반 Dedicated Server를 통한 멀티플레이어 협동(Co-op).
  - 무기 장착 시 해당 무기의 기억이 공명하여 스킬셋이 변경되는 시스템.

## 🏰 2. 세계관 및 스테이지 (World & Stages)
- **배경**: 대교황 티엘의 '푸른 마나' 세뇌에 저항하는 '황금빛 영혼'들의 투쟁.
- **주요 스테이지**:
  - **Stage 0. 아웃랜더 부락**: 생존자 거점 및 정비(로비).
  - **Stage 1. 테이너의 성**: 강철의 망령 '테이너' (Bone Golem).
  - **Stage 2. 레이터의 성**: 타락한 드루이드 '레이터' (Wolf/Bear/Boar 변신).
  - **Stage 3. 알리듬의 성**: 탐식자 '알리듬' (마력 폭격 및 환각).
  - **Stage 4. 티엘의 성채**: 최종 보스 '대교황 티엘' (사신 형태의 타락 천사).

## ⚔️ 3. 핵심 시스템 (Core Systems)
- **전투**: 논타겟팅(Non-Targeting) 기반. 히트박스와 타이밍이 중요한 피지컬 전투.
- **기억의 공명**: 롱소드, 대검, 단검, 활, 지팡이 등 무기 교체 시 즉시 스킬셋 변경.
- **성장 및 정비**: '방랑자의 모루'에서 소울을 사용하여 장비 제작, 강화 및 스텟 업그레이드.
- **협동 부활**: 한 명이라도 생존 시 체크포인트 도달 또는 보스 처치 시 파티원 부활.

## 🏗️ 4. 기술 아키텍처 (Technical Architecture)

### A. Client (DirectX 12 자체 엔진)
- **Rendering**: PBR(물리 기반 렌더링), 다이내믹 라이팅, GLTF/GLB 파서 직접 구현.
- **Animation**: 메쉬 데이터와 애니메이션 상태(Component)를 분리하여 인스턴스별 독립 시뮬레이션.
- **DX (Developer Experience)**: Unity-like한 컴포넌트 기반 아키텍처 지향.

### B. Server (IOCP Dedicated Server)
- **Physics**: **Jolt Physics** 기반 서버 권위적 판정.
- **Synchronization**: Dead Reckoning(위치), 애니메이션 트리거 및 상태값 동기화.
- **Optimization**: 30Hz 고정 타임스텝, NPC Dynamic Promotion, Spatial AOI 적용.
- **Rewind**: 서버 지연 보상을 위한 과거 시점 복원 및 재검증 시스템.

## 📁 5. 디렉토리 구조 및 주요 문서
- `기획 & 계획/`: 게임 기획서, 상세 설계서, 일정표 포함.
  - `Slay The Lord - 구원자의 뒤틀린 웃음.txt`: 메인 기획서.
  - `소울류_액션_서버_아키텍처.md`: 서버 및 전투 로직 설계.
  - `물리_서버_인프라_상세설계.md`: Jolt Physics 및 네트워크 최적화 설계.
  - `Architectural_Improvements_Plan_KR.md`: 애니메이션 시스템 리팩토링 계획.

## 🛠️ 6. 개발 지침 (Operational Protocols)
- **언어**: 모든 소통 및 설명은 **한국어**로 진행.
- **인코딩**: 소스 및 문서 파일은 반드시 **UTF-8 with BOM**으로 저장 (한글 깨짐 방지).
- **물리 변환**: `common::Vec3`와 `JPH::Vec3` 간 변환은 반드시 `PIP::Utils`를 사용.
- **수정 절차**: 파일 수정 전 반드시 **Diff Review**를 제공하고 사용자 승인 후 적용.

---
*이 문서는 프로젝트의 근간이 되는 기획과 기술적 설계를 연결하며, 모든 개발 단계에서 최우선 참조됩니다.*
