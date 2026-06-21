import os
from PIL import Image, ImageDraw, ImageFont

def create_title_text(text, filename):
    font_size = 40
    try:
        font = ImageFont.truetype("malgun.ttf", font_size)
    except:
        font = ImageFont.load_default()
        
    img = Image.new('RGBA', (500, 100), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    # Shadow
    draw.text((2, 2), text, font=font, fill=(0, 0, 0, 200))
    # Text
    draw.text((0, 0), text, font=font, fill=(240, 240, 240, 255))
    
    out_path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "Resource", "UI", filename)
    img.save(out_path)
    print(f"Generated {filename}: {text}")

if __name__ == "__main__":
    create_title_text("마을 주변 몬스터 제거", "Quest_Title_1.png")
    create_title_text("레버 내리기", "Quest_Title_2.png")
    create_title_text("우클릭으로 스킬 사용하기", "Skill_Unlock_UI.png")
