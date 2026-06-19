import argparse
from PIL import Image, ImageDraw, ImageFont
import os

def create_text_image(text, output_path, width=1600, height=150, font_size=60):
    # Set up image size and font
    img = Image.new("RGBA", (width, height), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)
    
    try:
        # Try to load a system Korean font (Malgun Gothic)
        font = ImageFont.truetype("malgunbd.ttf", font_size)
    except IOError:
        try:
            font = ImageFont.truetype("malgun.ttf", font_size)
        except IOError:
            print("Failed to load Korean font. Using default.")
            font = ImageFont.load_default()

    # Calculate text bounding box to center it
    bbox = draw.textbbox((0, 0), text, font=font)
    text_w = bbox[2] - bbox[0]
    text_h = bbox[3] - bbox[1]
    
    x = (width - text_w) / 2
    y = (height - text_h) / 2

    # Draw thick outline (Black)
    outline_color = (0, 0, 0, 255)
    thickness = 3
    for dx in range(-thickness, thickness + 1):
        for dy in range(-thickness, thickness + 1):
            if dx == 0 and dy == 0: continue
            draw.text((x + dx, y + dy), text, font=font, fill=outline_color)

    # Draw main text (White)
    draw.text((x, y), text, font=font, fill=(255, 255, 255, 255))
    
    # Ensure directory exists
    os.makedirs(os.path.dirname(output_path), exist_ok=True)
    
    img.save(output_path)
    print(f"Saved to {output_path} with size {width}x{height}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Generate UI text PNG with thick outline.")
    parser.add_argument("--text", default="퀘스트 완료! 최대 체력 +50, 공격력 +10", help="Text to render")
    parser.add_argument("--output", default=r"..\Client\Client\Resource\UI\Quest_Reward.png", help="Output PNG path")
    parser.add_argument("--width", type=int, default=1600, help="Image width")
    parser.add_argument("--height", type=int, default=150, help="Image height")
    parser.add_argument("--fontsize", type=int, default=60, help="Font size")
    
    args = parser.parse_args()
    create_text_image(args.text, args.output, args.width, args.height, args.fontsize)
