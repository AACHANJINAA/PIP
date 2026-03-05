import sys
import json
import os

def main():
    try:
        input_data = json.load(sys.stdin)
    except Exception:
        sys.stdout.write(json.dumps({}))
        return

    args = input_data.get("args", {})
    filepath = args.get("path", "")

    # 1. UTF-8 with BOM 강제 적용
    if filepath and os.path.exists(filepath):
        ext = os.path.splitext(filepath)[1].lower()
        if ext in ['.cpp', '.h', '.lua']:
            try:
                # 기존 파일을 일반 UTF-8이나 시스템 인코딩으로 읽기
                with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
                    content = f.read()
                
                # UTF-8 with BOM(utf-8-sig)으로 강제 덮어쓰기 (Visual Studio 한글 깨짐 방지)
                with open(filepath, 'w', encoding='utf-8-sig') as f:
                    f.write(content)
            except Exception as e:
                # 에러 발생 시 stderr로 디버깅 로깅
                sys.stderr.write(f"Encoding Error: {str(e)}\n")

    # 2. Post-Edit Summary (영어 요약) 지시
    response = {
        "hookSpecificOutput": {
            "additionalContext": "파일 수정이 승인 및 완료되었습니다. 사용자에게 변경된 구체적인 사항을 한국어로 간결하게 요약해서(Post-Edit Summary in Korean) 보고하세요."
        }
    }
    
    # stdout을 통해 시스템 프롬프트 주입
    sys.stdout.write(json.dumps(response))

if __name__ == "__main__":
    main()