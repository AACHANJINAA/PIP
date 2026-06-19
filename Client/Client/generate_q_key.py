# -*- coding: utf-8 -*-
import os
from PIL import Image, ImageDraw, ImageFont

def get_colors_from_image(img_path):
    # F_interaction_UI.png에서 색상 추출
    # 기본 폴백 색상
    bg_color = (3, 3, 3, 255)
    fg_color = (250, 247, 238, 255)
    
    if not os.path.exists(img_path):
        return bg_color, fg_color
        
    try:
        img = Image.open(img_path).convert('RGBA')
        w, h = img.size
        
        # 테두리 색상 추출: 외곽에서 5% 들어간 지점 샘플링
        # (테두리가 있는 영역을 지날 것이므로)
        sample_x = int(w * 0.05)
        sample_y = int(h * 0.05)
        # 만약 이 지점이 검은색이라면 조금 더 안쪽으로 들어감
        for offset in range(5, int(w * 0.15)):
            color = img.getpixel((offset, offset))
            if color[0] > 150 and color[1] > 150: # 밝은 색 발견 시 fg_color로 지정
                fg_color = color
                break
                
        # 배경색 추출: 정중앙은 글자가 있을 수 있으므로 구석 샘플링 (예: 2, 2)
        bg_color = img.getpixel((2, 2))
    except Exception as e:
        print(f"Error reading colors from source image: {e}")
        
    return bg_color, fg_color

def create_q_key_icon():
    base_dir = r"C:\Users\ckswl\Desktop\GITHUB\PIP\Client\Client\Resource\UI"
    f_ui_path = os.path.join(base_dir, "F_interaction_UI.png")
    
    # 1. 원본 이미지 크기 및 색상 로드
    width, height = 512, 512 # 기본 크기
    if os.path.exists(f_ui_path):
        try:
            with Image.open(f_ui_path) as img_f:
                width, height = img_f.size
        except:
            pass
            
    bg_color, fg_color = get_colors_from_image(f_ui_path)
    print(f"Detected dimensions: {width}x{height}")
    print(f"Detected colors - Background: {bg_color}, Foreground/Text: {fg_color}")
    
    # 2. 새로운 Q UI 이미지 생성
    img = Image.new('RGBA', (width, height), bg_color)
    draw = ImageDraw.Draw(img)
    
    # F_interaction_UI.png의 테두리 비율 계산
    # 대략 여백은 크기의 2.5%, 테두리 두께는 4.5% 정도임
    outer_margin = int(width * 0.025)
    border_width = int(width * 0.045)
    
    # 테두리 사각형 그리기 (Outline)
    # PIL의 rectangle은 width 옵션을 주면 안쪽으로 굵어짐
    rect_left = outer_margin + border_width // 2
    rect_top = outer_margin + border_width // 2
    rect_right = width - outer_margin - border_width // 2
    rect_bottom = height - outer_margin - border_width // 2
    
    draw.rectangle(
        [rect_left, rect_top, rect_right, rect_bottom],
        fill=None,
        outline=fg_color,
        width=border_width
    )
    
    # 3. 텍스트 'Q' 그리기
    # 텍스트 크기는 이미지 높이의 약 65% 수준
    font_size = int(height * 0.65)
    
    # 고풍스러운 세리프 폰트 사용 (Times New Roman 선호)
    fonts_to_try = ["times.ttf", "georgia.ttf", "arial.ttf"]
    font = None
    for f_name in fonts_to_try:
        try:
            font = ImageFont.truetype(f_name, font_size)
            break
        except:
            continue
    if font is None:
        font = ImageFont.load_default()
        
    # 텍스트 바운딩 박스를 통해 정확한 중앙 정렬 계산
    bbox = draw.textbbox((0, 0), "Q", font=font)
    tw = bbox[2] - bbox[0]
    th = bbox[3] - bbox[1]
    
    # Q 글자 특성상 꼬리가 아래로 쳐지므로 y 좌표 보정 필요
    tx = (width - tw) // 2 - bbox[0]
    # 약간 위로 오프셋을 줘서 꼬리가 박스 바깥으로 나가는 것을 막고 시각적 균형을 맞춤
    ty = (height - th) // 2 - bbox[1] - int(height * 0.03)
    
    # 텍스트 그리기 (그림자나 그라데이션 없이 플랫하게)
    draw.text((tx, ty), "Q", font=font, fill=fg_color)
    
    # 4. 저장
    os.makedirs(base_dir, exist_ok=True)
    
    # 두 가지 이름 모두로 저장하여 호환성 유지
    paths_to_save = [
        os.path.join(base_dir, "Q_interaction_UI.png"),
        os.path.join(base_dir, "Key_Q_Icon.png")
    ]
    
    for path in paths_to_save:
        img.save(path)
        print(f"Generated asset at: {path}")

if __name__ == "__main__":
    create_q_key_icon()
