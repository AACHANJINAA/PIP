import os
from PIL import Image, ImageDraw, ImageFont

def create_numbers_texture():
    # 문자 배열 (0~9, 그리고 퀘스트 표기용 '/')
    chars = ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '/']
    
    # 텍스처 한 글자당 크기
    char_w = 64
    char_h = 64
    
    # 전체 이미지 크기
    img_w = char_w * len(chars)
    img_h = char_h
    
    # 투명 배경 이미지 생성
    img = Image.new('RGBA', (img_w, img_h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # 폰트 로드 (윈도우 기본 폰트인 맑은 고딕 또는 바탕체 시도)
    try:
        # 바탕체로 판타지 느낌을 줌
        font = ImageFont.truetype("batang.ttc", 48)
    except:
        try:
            font = ImageFont.truetype("arial.ttf", 48)
        except:
            font = ImageFont.load_default()
            
    # 각 글자 그리기
    for i, char in enumerate(chars):
        # 텍스트 크기 계산을 위해 getbbox 사용
        bbox = draw.textbbox((0, 0), char, font=font)
        tw = bbox[2] - bbox[0]
        th = bbox[3] - bbox[1]
        
        # 글자를 가운데 정렬하기 위한 오프셋
        x = i * char_w + (char_w - tw) // 2
        y = (char_h - th) // 2 - bbox[1] # 베이스라인 보정
        
        # 흰색으로 그리기 (이후 UIRenderComponent에서 색상 틴트 가능)
        draw.text((x, y), char, font=font, fill=(255, 255, 255, 255))
        
    # 저장
    out_path = r"C:\Github\PIP\Client\Client\Resource\UI\Quest_Numbers.png"
    img.save(out_path)
    print("Saved texture to:", out_path)

if __name__ == "__main__":
    create_numbers_texture()
