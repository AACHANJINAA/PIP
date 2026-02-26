import sys
import json
import re
import os
import difflib

def main():
    # 1. 표준 입력(stdin)에서 JSON 데이터 읽기
    try:
        input_data = json.load(sys.stdin)
    except Exception:
        # JSON 파싱 실패 시 기본적으로 통과시켜 시스템 멈춤 방지
        sys.stdout.write(json.dumps({"decision": "allow"}))
        return

    # Gemini CLI 도구 호출 인자 추출 (경로 및 내용)
    args = input_data.get("args", {})
    filepath = args.get("path", "")
    content = args.get("content", "")

    # 2. 코딩 컨벤션 스캐너 로직 (정규식 기반)
    errors = []
    
    # 2-1. 원시 포인터(new/delete) 사용 감지
    if re.search(r'\bnew\s+[a-zA-Z_]', content) or re.search(r'\bdelete\b', content):
        errors.append("C++ 메모리 원칙 위반: std::unique_ptr 대신 raw pointer가 감지되었습니다.")
        
    # 2-2. 클래스명 PascalCase 검사 (소문자로 시작하는 class 감지)
    if re.search(r'\bclass\s+[a-z][a-zA-Z0-9_]*\b', content):
        errors.append("명명 규칙 위반: 클래스명은 PascalCase여야 합니다.")
        
    # 2-3. 함수 snake_case 위반이나 멤버 변수 '_' 접두사 등 추가 가능

    # 반려(Deny) 응답
    if errors:
        deny_response = {
            "decision": "deny",
            "reason": " ".join(errors) + " 코드를 수정하세요."
        }
        # Strict JSON rule: stdout으로만 JSON 출력
        sys.stdout.write(json.dumps(deny_response))
        return

    # 3. Diff 리뷰 텍스트 생성 및 출력 (통과 시)
    if filepath and os.path.exists(filepath):
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            old_lines = f.readlines()
            
        new_lines = [line + '\n' for line in content.splitlines()]
        diff = difflib.unified_diff(
            old_lines, new_lines, 
            fromfile='Original File', tofile='AI Generated Content'
        )
        diff_text = "".join(diff)
        
        if diff_text:
            # 루프 붕괴를 막기 위해 Diff는 반드시 표준 에러(stderr)로 출력
            sys.stderr.write("\n\033[93m=== [Diff Review - 검토 후 Y를 눌러 승인하세요] ===\033[0m\n")
            sys.stderr.write(diff_text)
            sys.stderr.write("\n\033[93m=========================================================\033[0m\n\n")

    # 4. 최종 승인(Allow) 응답
    sys.stdout.write(json.dumps({"decision": "allow"}))

if __name__ == "__main__":
    main()