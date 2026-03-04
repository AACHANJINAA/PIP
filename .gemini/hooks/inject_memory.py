import sys
import os

def main():
    # CLI에서 전달받은 사용자의 원래 프롬프트
    original_prompt = sys.argv[1] if len(sys.argv) > 1 else ""

    # 훅 스크립트 위치 기준으로 프로젝트 루트 경로 계산 (.gemini/hooks/ -> ../../)
    current_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.abspath(os.path.join(current_dir, '..', '..'))

    memory_files = ['plan.md', 'context.md', 'checklist.md']
    injected_context = "[프로젝트 핵심 컨텍스트]\n"

    # 3대 문서 읽어오기
    for filename in memory_files:
        filepath = os.path.join(project_root, filename)
        if os.path.exists(filepath):
            with open(filepath, 'r', encoding='utf-8') as f:
                injected_context += f"\n--- [{filename}] ---\n"
                injected_context += f.read().strip() + "\n"
        else:
            injected_context += f"\n--- [{filename}] (파일 없음) ---\n"

    # 최종 프롬프트 조립
    final_prompt = f"""{injected_context}
        위 컨텍스트(계획서, 맥락, 체크리스트)를 반드시 숙지하고 아래의 지시사항을 수행해.
        절대 컨텍스트에 어긋나는 기술 스택이나 구조를 제안하지 마.
        [지시사항]: {original_prompt}
        """

    # 조립된 최종 프롬프트를 stdout으로 출력하여 CLI가 인식하도록 함
    print(final_prompt)

if __name__ == "__main__":
    main()