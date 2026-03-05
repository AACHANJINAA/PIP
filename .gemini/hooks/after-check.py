#!/usr/bin/env python3
import sys
import json
import re
import datetime

class STLQualityGuard:
    def __init__(self):
        # S.T.L 프로젝트 핵심 검사 규칙 (영상 가이드: 프로젝트 고유 규칙 강제)
        self.rules = [
            {
                "id": "DX12_RESOURCE",
                "pattern": r"ID3D12(Device|GraphicsCommandList|Resource)",
                "message": "⚠️ DX12 직접 제어: ComPtr을 사용했나요? Resource Barrier나 Descriptor Heap 관리에 누락은 없나요?",
                "triggers": ["ID3D12", "Device", "CommandList"]
            },
            {
                "id": "SERVER_CONCURRENCY",
                "pattern": r"(std::mutex|lock_guard|unique_lock)",
                "message": "⚠️ 락(Lock) 사용 감지: 우리 서버는 Room 단위 Job Queue 기반입니다. 가급적 Lock-free하게 작성되었나요?",
                "triggers": ["mutex", "lock", "atomic"]
            },
            {
                "id": "PHYSICS_CONVERSION",
                "pattern": r"JPH::Vec3",
                "message": "⚠️ 물리 변환 확인: common::Vec3와 JPH::Vec3 간의 변환 시 반드시 'PIP::Utils'를 사용했나요?",
                "triggers": ["JPH::Vec3", "conversion"]
            },
            {
                "id": "RAW_MEMORY",
                "pattern": r"\b(new|delete)\b(?!\s+(void|char))",
                "message": "⚠️ 수동 메모리 관리: 스마트 포인터(unique_ptr, shared_ptr) 사용을 강력히 권장합니다.",
                "triggers": ["new", "delete"]
            }
        ]

    def check_content(self, content):
        findings = []
        for rule in self.rules:
            if re.search(rule["pattern"], content, re.IGNORECASE):
                findings.append(rule["message"])
        return findings

def main():
    # 터미널 디버그 로그
    sys.stderr.write(f"\n\033[35m[Gemini Hook] {datetime.datetime.now()} - '코드 자가 진단' 실행 중...\033[0m\n")

    try:
        input_data = json.load(sys.stdin)
    except Exception:
        sys.stdout.write(json.dumps({"decision": "allow"}))
        sys.exit(0)

    tool_name = input_data.get("tool_name", "")
    if tool_name not in ["write_file", "replace"]:
        sys.stdout.write(json.dumps({"decision": "allow"}))
        sys.exit(0)

    # 수정된 내용 추출
    tool_input = input_data.get("tool_input", {})
    new_content = tool_input.get("content", "") or tool_input.get("new_string", "")

    if not new_content.strip():
        sys.stdout.write(json.dumps({"decision": "allow"}))
        sys.exit(0)

    # 품질 검사 수행
    guard = STLQualityGuard()
    findings = guard.check_content(new_content)

    # ❗ 영상 가이드: 수정 후 검증(Validate) 질문 추가
    validation_questions = [
        "✅ 이 수정 사항에 대한 유닛 테스트를 추가하거나 실행했을 때 문제 없나요?",
        "✅ 빌드 에러나 런타임 Crash 가능성을 검토하셨나요?",
        "✅ 이 코드가 기존의 디자인 패턴(BT, Component)과 일치하나요?"
        "✅ 이 코드가 기존의 코드 컨벤션과 일치 하나요?"
        "✅ 수정된 코드가 서명된 UTF-8 인코딩으로 인코딩되어있나요?(한글주석깨지면안됨)"
    ]

    # 결과 조립
    feedback = "\n" + "!"*40 + "\n"
    feedback += "[👨‍💻 S.T.L 코드 자가 진단 결과]\n\n"
    
    if findings:
        feedback += "🔍 위험 요소가 감지되었습니다:\n"
        for f in findings:
            feedback += f" - {f}\n"
        feedback += "\n"

    feedback += "🛠️ 최종 검토 체크리스트:\n"
    for q in validation_questions:
        feedback += f" - {q}\n"

    feedback += "\n문제가 없다면 진행하시고, 수정이 필요하면 지금 바로 수정해 주세요.\n"
    feedback += "!"*40 + "\n"

    # AI에게 피드백 주입
    response = {
        "decision": "allow",
        "hookSpecificOutput": {
            "additionalContext": f"\n\n[자가 진단 로그]\n{feedback}"
        }
    }

    sys.stdout.write(json.dumps(response))
    sys.stderr.write(f"\033[92m  -> 진단 완료: AI에게 검토 지침이 전달되었습니다.\033[0m\n\n")

if __name__ == "__main__":
    main()
