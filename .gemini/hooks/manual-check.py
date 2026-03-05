#!/usr/bin/env python3
import datetime
import sys
import json
import re
import os
import glob

def read_directory_code(base_path, extensions=[".h", ".cpp", ".lua", ".md"]):
    """폴더 내의 주요 소스 코드와 문서를 한 번에 읽음"""
    combined_content = ""
    if not os.path.exists(base_path):
        return ""
    
    for ext in extensions:
        pattern = os.path.join(base_path, f"**/*{ext}")
        for filepath in glob.glob(pattern, recursive=True):
            if os.path.isfile(filepath):
                if ".vs" in filepath or "x64" in filepath or "Debug" in filepath or "Release" in filepath:
                    continue
                try:
                    with open(filepath, "r", encoding="utf-8", errors='ignore') as f:
                        file_rel_path = os.path.relpath(filepath, os.getcwd())
                        combined_content += f"\n\n// --- FILE: {file_rel_path} ---\n"
                        combined_content += f.read()
                except Exception:
                    continue
    return combined_content

def main():
    sys.stderr.write(f"\n\033[94m[Gemini Hook] {datetime.datetime.now()} - '승인 기반 고속 컨텍스트' 가동\033[0m\n")

    try:
        input_data = json.load(sys.stdin)
    except Exception:
        sys.stdout.write(json.dumps({"decision": "allow"}))
        sys.exit(0)

    user_message = input_data.get("prompt", "")
    if not user_message:
        sys.stdout.write(json.dumps({"decision": "allow"}))
        sys.exit(0)

    project_root = os.getenv('GEMINI_PROJECT_DIR', os.getcwd())
    
    categories = {
        "SERVER_LOGIC": {
            "dirs": ["Server/Server", ".gemini/manuals"],
            "triggers": ["서버", "server", "iocp", "패킷", "packet", "보스", "boss", "npc", "ai", "테이너", "전투"]
        },
        "CLIENT_LOGIC": {
            "dirs": ["Client/Client", ".gemini/manuals"],
            "triggers": ["클라", "client", "dx12", "렌더", "render", "shader", "pbr", "그림자"]
        }
    }

    injected_code = ""
    match_found = False
    
    for cat_name, info in categories.items():
        if any(t.lower() in user_message.lower() for t in info["triggers"]):
            sys.stderr.write(f"  -> \033[93m고속 로드: {cat_name}\033[0m\n")
            match_found = True
            for target_dir in info["dirs"]:
                full_dir_path = os.path.join(project_root, target_dir)
                injected_code += read_directory_code(full_dir_path)

    if not match_found or not injected_code:
        sys.stdout.write(json.dumps({"decision": "allow"}))
        sys.exit(0)

    # [핵심] 영상 철학 반영: 승인 기반 워크플로우(Gated Workflow) 강제 가이드라인
    final_context = "\n\n" + "█"*60 + "\n"
    final_context += "⚠️ [MANDATORY PROTOCOL: GATED MODIFICATION WORKFLOW]\n"
    final_context += "당신은 프로젝트의 안전을 위해 다음 단계를 반드시 지켜야 합니다.\n\n"
    final_context += "1. [CODE PROPOSAL]: 수정이 필요한 모든 파일의 코드를 '파일별 코드 블록'으로 먼저 제시하십시오.\n"
    final_context += "2. [WAIT FOR APPROVAL]: 코드를 보여준 직후 답변을 멈추고, 사용자의 승인을 기다리십시오.\n"
    final_context += "   - 절대로 코드 제시와 동시에 write_file이나 replace 도구를 사용하지 마십시오.\n"
    final_context += "3. [EXECUTION]: 사용자가 코드를 확인하고 '적용' 혹은 '승인'을 내린 다음 턴에만 도구를 사용하여 파일을 수정하십시오.\n"
    final_context += "4. [CONTEXT DRIVEN]: 별도의 리서치 과정 없이 주입된 아래 소스 데이터를 즉시 분석하여 답변하십시오.\n\n"
    final_context += "█ [PROJECT SOURCE DATA] █\n"
    final_context += injected_code
    final_context += "\n" + "█"*60 + "\n"

    response = {
        "hookSpecificOutput": {
            "additionalContext": final_context
        }
    }

    sys.stdout.write(json.dumps(response))
    sys.stderr.write(f"\033[92m  -> 완료: 고속 데이터 주입 및 승인 절차 규칙이 적용되었습니다.\033[0m\n\n")

if __name__ == "__main__":
    main()
