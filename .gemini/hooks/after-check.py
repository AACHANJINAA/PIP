import sys
import json
import re


class STLSoftReminderHook:
    def __init__(self):
        # S.T.L 프로젝트 핵심 검토 키워드 및 정규식 패턴
        self.check_patterns = {
            "DX12": {
                "keywords": ["ID3D12", "Device", "CommandList", "SwapChain"],
                "message": "💡 DX12 객체를 다루셨네요. 자원 해제 누락이나 ComPtr 적용 등 위험한 부분은 없는지 다시 한 번 보셨나요?"
            },
            "SERVER": {
                "keywords": ["IOCP", "Socket", "Session", "Thread", "Accept"],
                "message": "💡 서버 네트워크 로직이 포함되어 있네요. 데드락 위험은 없는지, 스레드 안전성(Lock-free, 원자적 연산)을 확보했는지 확인해 보셨나요?"
            },
            "MEMORY": {
                # new/delete를 직접 사용한 흔적이 있는지 정규식으로 검사
                "regex": re.compile(r'\b(new|delete)\b(?!\s+(void|char))'),
                "message": "💡 날것의 동적 할당(new/delete)이 감지되었습니다. 스마트 포인터나 오브젝트 풀링을 사용하는 것이 우리 컨벤션에 맞지 않을까요?"
            },
            "PHYSICS_COMBAT": {
                "keywords": ["Attack", "Damage", "Hitbox", "Collision"],
                "message": "💡 전투/타격 판정 로직이군요. 이 판정이 클라이언트가 아닌 서버(Jolt Physics 등)에서 안전하고 권위적으로 처리되도록 설계되었나요?"
            }
        }

    def analyze_and_remind(self, ai_response):
        """답변 내용을 분석하여 맞춤형 '옆자리 선배' 질문 생성"""
        questions = [
            "💡 혹시 전체 로직 중에 예외 상황(Error Handling)이 발생할 만한 곳은 없었나요?",
            "💡 우리가 정한 S.T.L 코드 컨벤션(Modern C++, OOP)에 잘 맞게 작성되었을까요?"
        ]

        trigger_count = 0

        # 키워드 기반 검사
        for category, rules in self.check_patterns.items():
            triggered = False

            if "regex" in rules and rules["regex"].search(ai_response):
                triggered = True
            elif "keywords" in rules and any(keyword.lower() in ai_response.lower() for keyword in rules["keywords"]):
                triggered = True

            if triggered:
                questions.append(rules["message"])
                trigger_count += 1

        # 감지된 내용이 없으면 기본 질문만 있으므로 굳이 귀찮게 띄우지 않음
        if trigger_count == 0:
            return "", False

        # 프롬프트 조립
        feedback_prompt = (
            "\n========================================\n"
            "[👨‍💻 옆자리 선배의 부드러운 체크]\n"
            "고생하셨습니다! 결과물을 확정하기 전에 혹시 아래 사항들도 확인해 보셨나요?\n\n"
        )
        for q in questions:
            feedback_prompt += f"{q}\n"

        feedback_prompt += "\n빠뜨린 것이 있다면 스스로 코드를 수정해 주시고, 완벽하다면 그대로 진행해 주세요.\n"
        feedback_prompt += "========================================\n"

        return feedback_prompt, True


def main():
    try:
        input_data = json.load(sys.stdin)
    except Exception:
        sys.stdout.write(json.dumps({"decision": "allow"}))
        sys.exit(0)

    tool_name = input_data.get("tool_name", "")

    # 1. 파일 수정 툴(write_file, replace)이 실행된 직후에만 작동하도록 타겟팅
    if tool_name not in ["write_file", "replace"]:
        sys.stdout.write(json.dumps({"decision": "allow"}))
        sys.exit(0)

    # 2. AfterTool 규격에 맞게 AI가 툴에 전달한 파일 내용(content) 추출
    tool_input = input_data.get("tool_input", {})
    ai_generated_content = tool_input.get("content", "") or tool_input.get("new_string", "")

    if not ai_generated_content.strip():
        sys.stdout.write(json.dumps({"decision": "allow"}))
        sys.exit(0)

    # 3. 내용 분석
    reminder = STLSoftReminderHook()
    soft_prompt, is_triggered = reminder.analyze_and_remind(ai_generated_content)

    if not is_triggered:
        sys.stdout.write(json.dumps({"decision": "allow"}))
        sys.exit(0)

    # 4. 터미널(stderr)에 출력하여 화면에서 개발자가 직관적으로 볼 수 있게 함 [2]
    sys.stderr.write(f"\033[33m[경고] 감지된 내용이 있습니다.\033[0m\n")
    response = {
        "decision": "allow",
        "hookSpecificOutput": {
            "additionalContext": f"\n[시스템 로그: 선배의 조언]\n{soft_prompt}"
        }
    }

    # 6. 반드시 stdout으로 단 하나의 JSON 객체만 출력 [2]
    sys.stdout.write(json.dumps(response))
    sys.exit(0)


if __name__ == "__main__":
    main()