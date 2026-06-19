import os
from PIL import Image, ImageDraw, ImageFont

def generate_story_board():
    w, h = 950, 530  # 텍스트 줄수가 늘어남에 따라 세로 크기를 530px로 확장
    img = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # 1. 어두운 반투명 패널 배경 그리기 (상아색 테두리)
    draw.rounded_rectangle([10, 10, w-10, h-10], radius=15, fill=(15, 15, 15, 185), outline=(60, 55, 50, 220), width=2)
    
    # 2. 폰트 로드
    try:
        font = ImageFont.truetype("batang.ttc", 24)  # 가독성을 위해 폰트 크기를 24pt로 살짝 조절
    except IOError:
        font = ImageFont.load_default()
        
    # 3. 줄바꿈 처리된 엘든링 풍 대사 설정
    lines = [
        "이곳은 원래 우리들의 요람이자, 영광스러운 성채였소...",
        "하지만 찬탈자 테이너가 우리를 몰아내고 성스러운 왕좌를 더럽혔지.",
        "",
        "성안을 떠도는 흉물들을 잠재우고, 두 개의 장치를 작동시켜 봉인을 푸시오.",
        "그래야만 저 깊은 곳에 숨은 비틀린 군주에게 칼끝을 겨눌 수 있을 터.",
        "",
        "이방인이여, 부디 우리에게 잃어버린 안식을 돌려주시오...",
        "",
        "...아아, 전해지는 전설에 의하면, 봉인이 풀리는 날",
        "메마른 영혼의 샘이 다시 솟구쳐 그대에게 힘을 보태줄지도 모른다네."
    ]
    
    y_offset = 45
    shadow_color = (0, 0, 0, 255)
    text_color = (235, 225, 205, 255) # 부드러운 상아색
    
    # 4. 각 줄을 중앙 정렬하여 그림자 효과와 함께 그리기
    for i, line in enumerate(lines):
        if not line:  # 빈 줄은 건너뛰어 간격 유지
            continue
        bbox = draw.textbbox((0, 0), line, font=font)
        tw = bbox[2] - bbox[0]
        th = bbox[3] - bbox[1]
        
        x = (w - tw) / 2
        y = y_offset + i * 38  # 줄간격을 38px로 오밀조밀하게 조절하여 패널에 쏙 넣음
        
        # 그림자 그리기
        draw.text((x + 2, y + 2), line, font=font, fill=shadow_color)
        # 본문 텍스트 그리기
        draw.text((x, y), line, font=font, fill=text_color)
        
    img.save('./Quest_Story.png')
    print("Quest_Story.png Generated successfully with Dark Souls/Elden Ring style!")

if __name__ == "__main__":
    generate_story_board()
