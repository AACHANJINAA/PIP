#!/usr/bin/env python3
import datetime
import sys
import json
import re
import os


def extract_section(filepath, section_keyword):
    """마크다운 파일에서 특정 키워드가 포함된 섹션(#)만 추출"""
    if not os.path.exists(filepath):
        return ""

    with open(filepath, "r", encoding="utf-8", errors='ignore') as f:
        content = f.read()

    pattern = re.compile(r'(#+.*?\n.*?)(?=\n#+ |\Z)', re.DOTALL)
    sections = pattern.findall(content)

    for sec in sections:
        if section_keyword.lower() in sec.lower():
            return sec.strip()
    return ""


def main():
    sys.stderr.write(f"\n\033[94m[디버그] {datetime.datetime.now()} - BeforeAgent 훅 진입 성공!\033[0m\n\n")
    try:
        input_data = json.load(sys.stdin)
    except Exception:
        sys.stdout.write(json.dumps({"decision": "allow"}))
        sys.exit(0)

    # BeforeAgent 훅에서는 사용자의 메시지가 'prompt' 키로 들어옵니다.
    user_message = input_data.get("prompt", "")

    if not user_message:
        sys.stdout.write(json.dumps({"decision": "allow"}))
        sys.exit(0)

    skills = {
        "RENDER_DX12": {
            "file": "plan.md",
            "keyword": "렌더링",
            "triggers": ["pbr", "shader", "dx12", "렌더", "그래픽", "hlsl"]
        },
        "SERVER_IOCP": {
            "file": "plan.md",
            "keyword": "아키텍처",
            "triggers": ["iocp", "패킷", "세션", "thread", "서버", "동기화"]
        },
        "PHILOSOPHY": {
            "file": "context.md",
            "keyword": "전투",
            "triggers": ["액션", "타격", "히트박스", "무기", "스위칭"]
        }
    }

    # 렌더링 오류를 방지하기 위해 빈 대괄호 대신 list() 함수를 사용하여 초기화합니다.
    activated_manuals = list()

    for skill_id, info in skills.items():
        if any(t.lower() in user_message.lower() for t in info["triggers"]):
            section_text = extract_section(info["file"], info["keyword"])
            if section_text:
                # 찾은 매뉴얼 텍스트를 리스트에 추가합니다.
                activated_manuals.append(f"--- [{info['file']} : {info['keyword']} 규칙] ---\n{section_text}")

    if not activated_manuals:
        sys.stdout.write(json.dumps({"decision": "allow"}))
        sys.exit(0)

    injected_context = "\n\n".join(activated_manuals)

    # 터미널에 진행 상황 표시 (에이전트가 읽지 않고 사용자 화면에만 보이도록 stderr 활용)
    log_msg = f"\n\n"
    sys.stderr.write(log_msg)

    # 최종 응답: BeforeAgent 규격에 맞게 additionalContext를 통해 프롬프트 뒤에 배경지식 몰래 붙여넣기
    response = {
        "hookSpecificOutput": {
            "additionalContext": f"\n\n\n{injected_context}"
        }
    }

    # 완성된 JSON을 단 한 번만 stdout으로 출력합니다.
    sys.stdout.write(json.dumps(response))
    sys.exit(0)


if __name__ == "__main__":
    main()