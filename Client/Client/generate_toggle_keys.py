# -*- coding: utf-8 -*-
import os
from PIL import Image, ImageDraw, ImageFont

def get_colors_from_image(img_path):
    bg_color = (3, 3, 3, 255)
    fg_color = (250, 247, 238, 255) # pale text
    
    if not os.path.exists(img_path):
        return bg_color, fg_color
        
    try:
        img = Image.open(img_path).convert('RGBA')
        w, h = img.size
        
        for offset in range(5, int(w * 0.15)):
            color = img.getpixel((offset, offset))
            if color[0] > 150 and color[1] > 150:
                fg_color = color
                break
                
        bg_color = img.getpixel((2, 2))
    except Exception as e:
        print(f"Error reading colors from source image: {e}")
        
    return bg_color, fg_color

def create_key_icon(key_char, is_on):
    base_dir = r"C:\Users\ckswl\Desktop\GITHUB\PIP\Client\Client\Resource\UI"
    f_ui_path = os.path.join(base_dir, "F_interaction_UI.png")
    
    width, height = 512, 512
    if os.path.exists(f_ui_path):
        try:
            with Image.open(f_ui_path) as img_f:
                width, height = img_f.size
        except:
            pass
            
    bg_color, fg_color_off = get_colors_from_image(f_ui_path)
    
    if is_on:
        # ON state: bright glowing gold outline and text
        fg_color = (255, 215, 0, 255) # Gold
        # Maybe slightly lighter background to show it's active
        bg_color = (40, 40, 30, 255)
    else:
        # OFF state: normal
        fg_color = fg_color_off
    
    img = Image.new('RGBA', (width, height), bg_color)
    draw = ImageDraw.Draw(img)
    
    outer_margin = int(width * 0.025)
    border_width = int(width * 0.045)
    if is_on:
        border_width = int(width * 0.06) # Thicker border when ON
    
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
    
    font_size = int(height * 0.65)
    
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
        
    bbox = draw.textbbox((0, 0), key_char, font=font)
    tw = bbox[2] - bbox[0]
    th = bbox[3] - bbox[1]
    
    tx = (width - tw) // 2 - bbox[0]
    ty = (height - th) // 2 - bbox[1] - int(height * 0.03)
    
    draw.text((tx, ty), key_char, font=font, fill=fg_color)
    
    os.makedirs(base_dir, exist_ok=True)
    
    state_str = "ON" if is_on else "OFF"
    path = os.path.join(base_dir, f"{key_char}_interaction_UI_{state_str}.png")
    
    img.save(path)
    print(f"Generated asset at: {path}")

if __name__ == "__main__":
    for key in ["Q", "E"]:
        for state in [True, False]:
            create_key_icon(key, state)
