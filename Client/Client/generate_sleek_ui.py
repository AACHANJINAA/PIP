import os
from PIL import Image, ImageDraw, ImageFont, ImageFilter

def create_gradient_bg():
    w, h = 600, 100
    img = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    # Create a linear gradient from left (alpha 200) to right (alpha 0)
    for x in range(w):
        alpha = int(200 * (1.0 - (x / w)**1.5)) # Fade out to right
        for y in range(h):
            img.putpixel((x, y), (0, 0, 0, alpha))
    img.save(r"C:\Github\PIP\Client\Client\Resource\UI\Quest_BG.png")

def create_title_text():
    # 고해상도 렌더링
    text = "마을 주변 몬스터 제거"
    font_size = 40
    try:
        font = ImageFont.truetype("malgun.ttf", font_size)
    except:
        font = ImageFont.load_default()
        
    img = Image.new('RGBA', (500, 100), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # 드롭 섀도우
    draw.text((2, 2), text, font=font, fill=(0, 0, 0, 200))
    draw.text((0, 0), text, font=font, fill=(240, 240, 240, 255))
    
    img.save(r"C:\Github\PIP\Client\Client\Resource\UI\Quest_Title.png")

def create_numbers_texture():
    chars = ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '/']
    font_size = 40
    char_w = 40
    char_h = 60
    
    img_w = char_w * len(chars)
    img_h = char_h
    
    img = Image.new('RGBA', (img_w, img_h), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    try:
        font = ImageFont.truetype("malgun.ttf", font_size)
    except:
        font = ImageFont.load_default()
            
    for i, char in enumerate(chars):
        bbox = draw.textbbox((0, 0), char, font=font)
        tw = bbox[2] - bbox[0]
        th = bbox[3] - bbox[1]
        
        x = i * char_w + (char_w - tw) // 2
        y = (char_h - th) // 2 - bbox[1]
        
        # Shadow
        draw.text((x+2, y+2), char, font=font, fill=(0, 0, 0, 200))
        # Text
        draw.text((x, y), char, font=font, fill=(240, 240, 240, 255))
        
    img.save(r"C:\Github\PIP\Client\Client\Resource\UI\Quest_Numbers.png")

if __name__ == "__main__":
    create_gradient_bg()
    create_title_text()
    create_numbers_texture()
    print("New sleek UI assets generated.")
