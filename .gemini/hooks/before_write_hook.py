#!/usr/bin/env python3
import sys
import json
import os


def read_manual(filename):
    """프로젝트 폴더 내의 매뉴얼 파일을 안전하게 읽어오는 함수"""
    # 샌드박스 환경에서도 프로젝트 루트를 찾을 수 있도록 환경 변수 활용 [2]
    project_dir = os.environ.get("GEMINI_PROJECT_DIR", os.getcwd())
    filepath = os.path.join(project_dir, ".gemini", "manuals", filename)

    try:
        with open(filepath, "r", encoding="utf-8") as f:
            return f.read()
    except Exception:
        return f"({filename} 매뉴얼 내용을 찾을 수 없습니다.)"


def main():
    try:
        input_data = json.load(sys.stdin)
    except Exception:
        sys.stdout.write(json.dumps({"decision": "allow"}))
        sys.exit(0)

    tool_name = input_data.get("tool_name", "")
    tool_input = input_data.get("tool_input", {})

    # 파일을 쓰거나 수정하는 툴이 아니면 즉시 통과
    if tool_name not in ["write_file", "replace"]:
        sys.stdout.write(json.dumps({"decision": "allow"}))
        sys.exit(0)

    # 쓰려고 하는 파일 경로와 작성하려는 코드 내용 추출
    file_path = tool_input.get("file_path", "").lower()
    content = tool_input.get("content", "") or tool_input.get("new_string", "")

    # 1. 파일 경로를 기반으로 모듈 판별
    is_network = any(keyword in file_path for keyword in ["network", "server", "client", "packet"])
    is_graphics = any(keyword in file_path for keyword in ["graphics", "render", "shader", "dx12"])

    violation_reason = ""

    # 2. 매뉴얼 검증 (무한 루프 방지를 위해 인증 주석 확인)
    if is_network and "/* NETWORK_RULE_APPLIED */" not in content:
        server_rule = read_manual("server_rule.md")
        net_rule = read_manual("network_rule.md")
        client_rule = read_manual("client_rule.md")

        violation_reason = (
            f"네트워크/서버 코드를 작성할 때는 다음 매뉴얼을 반드시 준수해야 합니다.\n"
            f"--- [서버 규칙] ---\n{server_rule}\n\n"
            f"--- [네트워크 규칙] ---\n{net_rule}\n\n"
            f"--- [클라이언트 규칙] ---\n{client_rule}\n\n"
            f"위 매뉴얼을 철저히 읽고 코드를 수정한 뒤, 파일 최상단에 '/* NETWORK_RULE_APPLIED */' 주석을 추가하여 다시 저장하세요."
        )

    elif is_graphics and "/* GRAPHICS_RULE_APPLIED */" not in content:
        gfx_rule = read_manual("graphics_rule.md")

        violation_reason = (
            f"그래픽스 코드를 작성할 때는 다음 매뉴얼을 반드시 준수해야 합니다.\n"
            f"--- [그래픽스 규칙] ---\n{gfx_rule}\n\n"
            f"위 매뉴얼을 철저히 읽고 코드를 수정한 뒤, 파일 최상단에 '/* GRAPHICS_RULE_APPLIED */' 주석을 추가하여 다시 저장하세요."
        )

    # 3. 위반 사항이 있으면 파일 쓰기를 차단(deny)하고 에이전트에게 매뉴얼을 피드백으로 전송
    if violation_reason:
        # reason 필드에 담긴 텍스트가 AI에게 '도구 실행 실패 사유'로 전달되어 자가 교정을 유도합니다.
        response = {
            "decision": "deny",
            "reason": violation_reason,
            "systemMessage": "🚨 파일 쓰기 차단됨: 모듈별 규약 누락. AI에게 매뉴얼을 전송하여 재작성을 지시합니다."
        }
        sys.stdout.write(json.dumps(response))
        sys.exit(0)

    # 문제 없으면 정상적으로 파일 시스템에 쓰기 허용
    sys.stdout.write(json.dumps({"decision": "allow"}))
    sys.exit(0)


if __name__ == "__main__":
    main()