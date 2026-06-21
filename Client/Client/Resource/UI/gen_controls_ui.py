# -*- coding: utf-8 -*-
import os
from PIL import Image, ImageDraw, ImageFont

def draw_gradient_line(draw, y, width, color, max_alpha):
    # 좌우로 갈수록 투명해지는 선 (다크소울풍의 고급스러운 UI 선)
    for x in range(width):
        dist = abs(x - width/2) / (width/2)
        alpha = int(max_alpha * (1 - dist))
        draw.point((x, y), fill=(color[0], color[1], color[2], alpha))
        draw.point((x, y+1), fill=(color[0], color[1], color[2], alpha))

def create_controls_image():
    width, height = 800, 600
    # 반투명 어두운 배경 (다크소울풍의 깊고 어두운 톤)
    img = Image.new("RGBA", (width, height), (10, 10, 12, 190))
    draw = ImageDraw.Draw(img)
    
    try:
        font_title = ImageFont.truetype("batang.ttc", 40)
        font_body = ImageFont.truetype("batang.ttc", 26)
        font_key = ImageFont.truetype("batang.ttc", 26) # 키 텍스트용
    except IOError:
        font_title = ImageFont.load_default()
        font_body = ImageFont.load_default()
        font_key = ImageFont.load_default()

    title = "조 작 법"
    # (설명, 키)
    lines = [
        ("이동", "W, A, S, D"),
        ("대쉬 / 회피", "Left Shift"),
        ("점프", "Space Bar"),
        ("상호작용", "F"),
        ("기본 공격", "마우스 좌클릭"),
        ("스킬 사용", "마우스 우클릭"),
        ("락온 토글", "마우스 휠 클릭 또는 Tab"),
        ("퀘스트 및 스토리 확인", "Q"),
        ("조작법 확인", "E")
    ]

    title_color = (200, 180, 130, 255) # 바랜 황금색 (Muted Gold)
    text_color = (190, 190, 190, 255) # 밝은 회색
    key_color = (210, 140, 100, 255) # 붉은빛이 도는 바랜 금속색 (다크 판타지풍)
    sub_text_color = (130, 130, 130, 255) # 어두운 회색 (하위 항목용)

    # Draw Title
    title_bbox = draw.textbbox((0, 0), title, font=font_title)
    title_w = title_bbox[2] - title_bbox[0]
    title_x = (width - title_w) / 2
    title_y = 30
    
    # Shadow for title
    draw.text((title_x + 2, title_y + 2), title, font=font_title, fill=(0, 0, 0, 255))
    draw.text((title_x, title_y), title, font=font_title, fill=title_color)

    # Draw Top Line (Gradient)
    draw_gradient_line(draw, 90, width, title_color, 200)

    # Draw body text
    y_offset = 120
    line_spacing = 42
    
    for desc, key in lines:
        if key == "":
            # 하위 항목
            x_offset = 140
            draw.text((x_offset + 2, y_offset + 2), desc, font=font_body, fill=(0, 0, 0, 255))
            draw.text((x_offset, y_offset), desc, font=font_body, fill=sub_text_color)
        else:
            # 설명 파트
            x_offset_desc = 100
            draw.text((x_offset_desc + 2, y_offset + 2), desc, font=font_body, fill=(0, 0, 0, 255))
            draw.text((x_offset_desc, y_offset), desc, font=font_body, fill=text_color)
            
            # 키 파트 (가운데를 기준으로 일정 간격 띄워서 오른쪽 정렬 느낌)
            x_offset_key = 380
            draw.text((x_offset_key + 2, y_offset + 2), key, font=font_key, fill=(0, 0, 0, 255))
            draw.text((x_offset_key, y_offset), key, font=font_key, fill=key_color)
            
        y_offset += line_spacing

    # Draw Bottom Line (Gradient)
    draw_gradient_line(draw, height - 30, width, title_color, 200)

    output_path = r"c:\Users\ckswl\Desktop\GITHUB\PIP\Client\Client\Resource\UI\Controls_UI_New.png"
    img.save(output_path)
    print(f"Generated controls UI at: {output_path}")

if __name__ == "__main__":
    create_controls_image()
