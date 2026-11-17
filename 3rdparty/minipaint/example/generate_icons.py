#!/usr/bin/env python3
"""
Generate authentic, high-quality MS Paint style full-color icons.
Uses 4x supersampling (128x128 -> 32x32) with Lanczos filtering for
crisp, antialiased, professional UI icons.
"""

import os
import math
from PIL import Image, ImageDraw

SCALE = 4
CANVAS_SIZE = 32 * SCALE  # 128x128

def create_base():
    return Image.new("RGBA", (CANVAS_SIZE, CANVAS_SIZE), (0, 0, 0, 0))

def finalize(img):
    return img.resize((32, 32), Image.Resampling.LANCZOS)

# 1. Pencil: Classic 3D yellow pencil
def make_pencil():
    img = create_base()
    draw = ImageDraw.Draw(img)
    # Shaft facets:
    draw.polygon([(36, 92), (88, 40), (96, 48), (44, 100)], fill=(220, 160, 0, 255))
    draw.polygon([(44, 84), (96, 32), (104, 40), (52, 92)], fill=(255, 230, 80, 255))
    draw.polygon([(44, 100), (96, 48), (104, 56), (52, 108)], fill=(245, 195, 20, 255))
    # Sharpened wood cone:
    draw.polygon([(16, 112), (36, 92), (52, 108)], fill=(245, 215, 175, 255))
    draw.polygon([(16, 112), (40, 88), (48, 104)], fill=(225, 190, 145, 255))
    # Graphite lead tip:
    draw.polygon([(12, 116), (24, 104), (28, 108)], fill=(40, 45, 55, 255))
    draw.polygon([(10, 118), (18, 110), (22, 114)], fill=(25, 25, 30, 255))
    # Silver ferrule band:
    draw.polygon([(88, 40), (104, 56), (110, 50), (94, 34)], fill=(190, 195, 205, 255))
    draw.polygon([(91, 37), (107, 53), (111, 49), (95, 33)], fill=(230, 235, 245, 255))
    # Pink eraser:
    draw.polygon([(98, 30), (114, 46), (122, 38), (106, 22)], fill=(255, 130, 155, 255))
    draw.polygon([(102, 26), (118, 42), (122, 38), (106, 22)], fill=(240, 110, 135, 255))
    return finalize(img)

# 2. Eraser: 3D pink and blue eraser
def make_eraser():
    img = create_base()
    draw = ImageDraw.Draw(img)
    # Isometric tilted block
    poly_pink_top = [(28, 56), (68, 28), (104, 48), (64, 76)]
    draw.polygon(poly_pink_top, fill=(255, 145, 170, 255))
    # Top highlight line
    draw.line([(28, 56), (68, 28), (104, 48)], fill=(255, 205, 220, 255), width=3)
    # Pink front/left face
    poly_pink_side = [(28, 56), (64, 76), (64, 104), (28, 84)]
    draw.polygon(poly_pink_side, fill=(230, 85, 120, 255))
    # Blue front/right face
    poly_blue_side = [(64, 76), (104, 48), (104, 76), (64, 104)]
    draw.polygon(poly_blue_side, fill=(58, 134, 255, 255))
    # Dark shadow line along bottom
    draw.line([(28, 84), (64, 104), (104, 76)], fill=(30, 40, 70, 255), width=3)
    draw.line([(64, 76), (64, 104)], fill=(40, 90, 180, 255), width=3)
    return finalize(img)

# 3. Bucket: Metallic bucket pouring blue paint
def make_bucket():
    img = create_base()
    draw = ImageDraw.Draw(img)
    # Bucket body
    draw.polygon([(52, 24), (80, 16), (104, 40), (76, 48)], fill=(180, 185, 195, 255))
    draw.polygon([(52, 24), (76, 48), (44, 80), (20, 56)], fill=(215, 220, 230, 255))
    draw.polygon([(76, 48), (104, 40), (72, 72), (44, 80)], fill=(160, 165, 175, 255))
    # Blue paint interior rim
    draw.polygon([(20, 56), (44, 80), (36, 88), (12, 64)], fill=(0, 120, 235, 255))
    # Flow pouring out
    draw.polygon([(16, 72), (36, 88), (28, 116), (12, 108)], fill=(0, 140, 255, 255))
    draw.ellipse([(10, 104), (30, 120)], fill=(0, 150, 255, 255))
    # Falling droplet
    draw.ellipse([(48, 96), (64, 114)], fill=(0, 160, 255, 255))
    draw.polygon([(56, 86), (64, 104), (48, 104)], fill=(0, 160, 255, 255))
    # Metal handle
    draw.line([(66, 20), (84, 10), (98, 26)], fill=(120, 125, 135, 255), width=4)
    return finalize(img)

# 4. Eyedropper: Glass pipette with red bulb
def make_eyedropper():
    img = create_base()
    draw = ImageDraw.Draw(img)
    # Red bulb
    draw.ellipse([(80, 12), (116, 48)], fill=(230, 40, 50, 255))
    draw.ellipse([(86, 16), (102, 32)], fill=(255, 110, 110, 255))
    # Silver collar
    draw.polygon([(72, 44), (84, 56), (92, 48), (80, 36)], fill=(180, 185, 195, 255))
    # Glass shaft
    draw.polygon([(40, 76), (72, 44), (84, 56), (52, 88)], fill=(225, 240, 255, 200))
    # Liquid inside glass
    draw.polygon([(36, 80), (56, 60), (66, 70), (46, 90)], fill=(0, 130, 240, 255))
    # Tapered tip
    draw.polygon([(20, 96), (40, 76), (52, 88), (32, 108)], fill=(210, 225, 245, 220))
    draw.polygon([(12, 116), (20, 96), (32, 108)], fill=(25, 30, 40, 255))
    # Tip drop
    draw.ellipse([(8, 112), (18, 122)], fill=(0, 140, 250, 255))
    return finalize(img)

# 5. Brush: Artist paint brush
def make_brush():
    img = create_base()
    draw = ImageDraw.Draw(img)
    # Wooden handle
    draw.polygon([(48, 76), (96, 28), (108, 40), (60, 88)], fill=(160, 95, 45, 255))
    draw.polygon([(52, 72), (100, 24), (108, 32), (60, 80)], fill=(205, 135, 75, 255))
    # Metal ferrule
    draw.polygon([(36, 88), (48, 76), (60, 88), (48, 100)], fill=(210, 215, 225, 255))
    draw.polygon([(38, 86), (50, 74), (54, 78), (42, 90)], fill=(245, 250, 255, 255))
    # Bristles (dark brown/black)
    draw.polygon([(20, 104), (36, 88), (48, 100), (32, 116)], fill=(60, 45, 35, 255))
    # Wet blue paint tip
    draw.polygon([(12, 116), (24, 104), (36, 116)], fill=(0, 130, 240, 255))
    draw.ellipse([(8, 112), (20, 124)], fill=(0, 150, 255, 255))
    return finalize(img)

# 6. Airbrush: Spray can with mist
def make_airbrush():
    img = create_base()
    draw = ImageDraw.Draw(img)
    # Cylinder spray can
    draw.rounded_rectangle([(64, 44), (108, 116)], radius=8, fill=(0, 140, 220, 255))
    # Can highlight
    draw.rounded_rectangle([(70, 48), (80, 112)], radius=4, fill=(80, 190, 255, 255))
    # Metal top
    draw.rounded_rectangle([(74, 32), (98, 44)], radius=4, fill=(200, 205, 215, 255))
    # Nozzle
    draw.polygon([(60, 24), (74, 28), (74, 36), (60, 36)], fill=(240, 50, 50, 255))
    # Spray mist droplets
    dots = [(44, 20), (32, 28), (20, 16), (24, 36), (12, 28), (36, 44), (16, 48), (28, 56)]
    for dx, dy in dots:
        draw.ellipse([(dx, dy), (dx + 8, dy + 8)], fill=(0, 160, 255, 220))
    return finalize(img)

# 7. Text: Bold 3D serif 'A'
def make_text():
    img = create_base()
    draw = ImageDraw.Draw(img)
    # 3D shadow
    draw.polygon([(36, 112), (64, 24), (72, 24), (100, 112), (80, 112), (72, 88), (56, 88), (48, 112)], fill=(120, 10, 20, 255))
    # Main red 'A'
    draw.polygon([(32, 108), (60, 20), (68, 20), (96, 108), (76, 108), (68, 84), (52, 84), (44, 108)], fill=(230, 30, 45, 255))
    # Inner cutout
    draw.polygon([(56, 68), (64, 40), (72, 68)], fill=(0, 0, 0, 0))
    # Left serif highlight
    draw.line([(28, 108), (46, 108)], fill=(255, 120, 130, 255), width=4)
    draw.line([(82, 108), (100, 108)], fill=(255, 120, 130, 255), width=4)
    return finalize(img)

# 8. Line: Clean geometric diagonal line
def make_line():
    img = create_base()
    draw = ImageDraw.Draw(img)
    # Blue diagonal line with slight thickness and rounded caps
    draw.line([(20, 108), (108, 20)], fill=(0, 120, 235, 255), width=14)
    draw.line([(22, 106), (106, 22)], fill=(60, 170, 255, 255), width=6)
    draw.ellipse([(14, 102), (26, 114)], fill=(0, 100, 210, 255))
    draw.ellipse([(102, 14), (114, 26)], fill=(0, 100, 210, 255))
    return finalize(img)

# 9. Rectangle: Crisp blue bordered rectangle
def make_rectangle():
    img = create_base()
    draw = ImageDraw.Draw(img)
    draw.rounded_rectangle([(16, 24), (112, 104)], radius=8, fill=(210, 235, 255, 180), outline=(0, 110, 225, 255), width=10)
    return finalize(img)

# 10. Ellipse: Crisp orange bordered ellipse
def make_ellipse():
    img = create_base()
    draw = ImageDraw.Draw(img)
    draw.ellipse([(16, 24), (112, 104)], fill=(255, 240, 220, 180), outline=(245, 120, 30, 255), width=10)
    return finalize(img)

# 11. Save: Classic 3.5" Floppy Disk
def make_save():
    img = create_base()
    draw = ImageDraw.Draw(img)
    # Blue diskette body
    draw.rounded_rectangle([(20, 16), (108, 112)], radius=6, fill=(30, 100, 200, 255))
    # Shutter metal plate at top
    draw.rectangle([(36, 16), (92, 54)], fill=(220, 225, 235, 255))
    draw.rectangle([(50, 24), (66, 48)], fill=(40, 50, 70, 255))
    # White paper label at bottom
    draw.rounded_rectangle([(32, 62), (96, 112)], radius=4, fill=(250, 250, 250, 255))
    draw.line([(40, 74), (88, 74)], fill=(180, 190, 205, 255), width=4)
    draw.line([(40, 86), (88, 86)], fill=(180, 190, 205, 255), width=4)
    draw.line([(40, 98), (76, 98)], fill=(180, 190, 205, 255), width=4)
    return finalize(img)

# 12. Load: Open folder with document
def make_load():
    img = create_base()
    draw = ImageDraw.Draw(img)
    # Back folder tab
    draw.rounded_rectangle([(16, 32), (64, 56)], radius=6, fill=(230, 150, 0, 255))
    draw.rounded_rectangle([(16, 40), (112, 108)], radius=6, fill=(230, 150, 0, 255))
    # White sheet inside
    draw.rectangle([(32, 24), (96, 76)], fill=(255, 255, 255, 255), outline=(200, 205, 215, 255), width=2)
    draw.line([(40, 36), (88, 36)], fill=(150, 160, 180, 255), width=4)
    draw.line([(40, 48), (88, 48)], fill=(150, 160, 180, 255), width=4)
    # Front angled folder flap
    draw.polygon([(12, 108), (28, 56), (116, 56), (104, 108)], fill=(255, 185, 30, 255))
    return finalize(img)

# 13. FilePlus: Document with green '+'
def make_file_plus():
    img = create_base()
    draw = ImageDraw.Draw(img)
    # Document sheet
    draw.polygon([(24, 16), (80, 16), (104, 40), (104, 112), (24, 112)], fill=(255, 255, 255, 255), outline=(190, 195, 205, 255), width=4)
    # Dog-ear corner
    draw.polygon([(80, 16), (104, 40), (80, 40)], fill=(210, 215, 225, 255))
    # Green plus badge
    draw.ellipse([(64, 64), (116, 116)], fill=(34, 177, 76, 255))
    draw.line([(90, 74), (90, 106)], fill=(255, 255, 255, 255), width=8)
    draw.line([(74, 90), (106, 90)], fill=(255, 255, 255, 255), width=8)
    return finalize(img)

# 14. Undo: Curved blue arrow pointing left
def make_undo():
    img = create_base()
    draw = ImageDraw.Draw(img)
    # Arrow head
    draw.polygon([(16, 64), (48, 32), (48, 52), (76, 52), (88, 64), (88, 92), (72, 92), (72, 72), (48, 72), (48, 96)], fill=(0, 125, 240, 255))
    draw.polygon([(16, 64), (48, 32), (48, 52)], fill=(40, 160, 255, 255))
    return finalize(img)

# 15. Redo: Curved blue arrow pointing right
def make_redo():
    img = create_base()
    draw = ImageDraw.Draw(img)
    # Arrow head
    draw.polygon([(112, 64), (80, 32), (80, 52), (52, 52), (40, 64), (40, 92), (56, 92), (56, 72), (80, 72), (80, 96)], fill=(0, 125, 240, 255))
    draw.polygon([(112, 64), (80, 32), (80, 52)], fill=(40, 160, 255, 255))
    return finalize(img)

# 16. Palette: Wooden painter palette
def make_palette_logo():
    img = create_base()
    draw = ImageDraw.Draw(img)
    # Oval wooden board
    draw.ellipse([(16, 16), (112, 112)], fill=(225, 175, 115, 255), outline=(175, 125, 70, 255), width=4)
    # Thumb hole
    draw.ellipse([(36, 72), (56, 96)], fill=(0, 0, 0, 0), outline=(175, 125, 70, 255), width=4)
    # Paint dollops
    draw.ellipse([(36, 32), (56, 52)], fill=(235, 30, 45, 255))   # Red
    draw.ellipse([(68, 24), (88, 44)], fill=(255, 200, 10, 255))  # Yellow
    draw.ellipse([(92, 52), (110, 70)], fill=(35, 180, 75, 255))  # Green
    draw.ellipse([(76, 80), (96, 100)], fill=(0, 130, 240, 255))  # Blue
    return finalize(img)

# 17. ColorEdit: Chromatic wheel
def make_color_edit():
    img = create_base()
    # Draw radial hue wheel
    center = CANVAS_SIZE // 2
    radius = int(CANVAS_SIZE * 0.42)
    for y in range(CANVAS_SIZE):
        for x in range(CANVAS_SIZE):
            dx = x - center
            dy = y - center
            dist = math.hypot(dx, dy)
            if dist <= radius:
                angle = (math.atan2(dy, dx) / (2 * math.pi)) % 1.0
                sat = dist / radius
                # HSV to RGB
                h = angle * 6.0
                c = sat
                x_val = c * (1.0 - abs(h % 2.0 - 1.0))
                m = 1.0 - c
                if h < 1: r, g, b = c, x_val, 0
                elif h < 2: r, g, b = x_val, c, 0
                elif h < 3: r, g, b = 0, c, x_val
                elif h < 4: r, g, b = 0, x_val, c
                elif h < 5: r, g, b = x_val, 0, c
                else: r, g, b = c, 0, x_val
                # Add brightness
                r = int((r + m) * 255)
                g = int((g + m) * 255)
                b = int((b + m) * 255)
                img.putpixel((x, y), (r, g, b, 255))
    return finalize(img)

# 18. Fit: Magnifying glass
def make_fit():
    img = create_base()
    draw = ImageDraw.Draw(img)
    # Glass lens
    draw.ellipse([(20, 20), (84, 84)], fill=(220, 245, 255, 180), outline=(160, 170, 185, 255), width=8)
    draw.arc([(28, 28), (76, 76)], start=190, end=290, fill=(255, 255, 255, 255), width=6)
    # Handle
    draw.line([(76, 76), (112, 112)], fill=(120, 125, 135, 255), width=16)
    draw.line([(80, 80), (108, 108)], fill=(60, 65, 75, 255), width=8)
    return finalize(img)

# 19. Plus: Green plus badge
def make_plus():
    img = create_base()
    draw = ImageDraw.Draw(img)
    draw.ellipse([(16, 16), (112, 112)], fill=(34, 177, 76, 255))
    draw.line([(64, 32), (64, 96)], fill=(255, 255, 255, 255), width=12)
    draw.line([(32, 64), (96, 64)], fill=(255, 255, 255, 255), width=12)
    return finalize(img)

# 20. Minus: Red minus badge
def make_minus():
    img = create_base()
    draw = ImageDraw.Draw(img)
    draw.ellipse([(16, 16), (112, 112)], fill=(235, 45, 55, 255))
    draw.line([(32, 64), (96, 64)], fill=(255, 255, 255, 255), width=12)
    return finalize(img)

# 21. Layers: Three stacked sheets
def make_layers():
    img = create_base()
    draw = ImageDraw.Draw(img)
    # Bottom layer
    draw.polygon([(64, 80), (112, 96), (64, 112), (16, 96)], fill=(50, 120, 200, 255))
    # Mid layer
    draw.polygon([(64, 52), (112, 68), (64, 84), (16, 68)], fill=(0, 160, 240, 255))
    # Top layer
    draw.polygon([(64, 24), (112, 40), (64, 56), (16, 40)], fill=(120, 210, 255, 255))
    return finalize(img)

# 22. Grid: Clean 3x3 grid
def make_grid():
    img = create_base()
    draw = ImageDraw.Draw(img)
    draw.rounded_rectangle([(20, 20), (108, 108)], radius=6, outline=(0, 120, 220, 255), width=8)
    draw.line([(49, 20), (49, 108)], fill=(0, 120, 220, 255), width=6)
    draw.line([(79, 20), (79, 108)], fill=(0, 120, 220, 255), width=6)
    draw.line([(20, 49), (108, 49)], fill=(0, 120, 220, 255), width=6)
    draw.line([(20, 79), (108, 79)], fill=(0, 120, 220, 255), width=6)
    return finalize(img)

# 23. Clear: Document with red 'X'
def make_clear():
    img = create_base()
    draw = ImageDraw.Draw(img)
    draw.rounded_rectangle([(24, 20), (104, 108)], radius=6, fill=(255, 255, 255, 255), outline=(190, 195, 205, 255), width=4)
    draw.line([(44, 44), (84, 84)], fill=(235, 45, 55, 255), width=10)
    draw.line([(84, 44), (44, 84)], fill=(235, 45, 55, 255), width=10)
    return finalize(img)

# 24. Rotate: Circular rotation arrow
def make_rotate():
    img = create_base()
    draw = ImageDraw.Draw(img)
    draw.arc([(24, 24), (104, 104)], start=30, end=300, fill=(0, 125, 240, 255), width=10)
    # Arrow head at end of arc
    draw.polygon([(88, 20), (112, 40), (84, 52)], fill=(0, 125, 240, 255))
    return finalize(img)

# 25. FlipH: Horizontal flip
def make_flip_h():
    img = create_base()
    draw = ImageDraw.Draw(img)
    draw.line([(64, 16), (64, 112)], fill=(150, 160, 175, 255), width=6)
    draw.polygon([(52, 36), (52, 92), (20, 64)], fill=(0, 130, 245, 255))
    draw.polygon([(76, 36), (76, 92), (108, 64)], fill=(90, 190, 255, 255))
    return finalize(img)

# 26. FlipV: Vertical flip
def make_flip_v():
    img = create_base()
    draw = ImageDraw.Draw(img)
    draw.line([(16, 64), (112, 64)], fill=(150, 160, 175, 255), width=6)
    draw.polygon([(36, 52), (92, 52), (64, 20)], fill=(0, 130, 245, 255))
    draw.polygon([(36, 76), (92, 76), (64, 108)], fill=(90, 190, 255, 255))
    return finalize(img)

# 27. Resize: Window with resize arrow
def make_resize():
    img = create_base()
    draw = ImageDraw.Draw(img)
    draw.rounded_rectangle([(16, 16), (88, 88)], radius=6, outline=(140, 150, 165, 255), width=6)
    # Double-headed diagonal arrow
    draw.line([(48, 48), (108, 108)], fill=(0, 120, 235, 255), width=8)
    draw.polygon([(116, 100), (116, 116), (100, 116)], fill=(0, 120, 235, 255))
    draw.polygon([(40, 56), (40, 40), (56, 40)], fill=(0, 120, 235, 255))
    return finalize(img)

# 28. Eye: Open eye
def make_eye():
    img = create_base()
    draw = ImageDraw.Draw(img)
    draw.ellipse([(16, 36), (112, 92)], fill=(255, 255, 255, 255), outline=(60, 70, 85, 255), width=6)
    draw.ellipse([(44, 40), (84, 88)], fill=(0, 140, 240, 255))
    draw.ellipse([(54, 52), (74, 76)], fill=(20, 25, 35, 255))
    draw.ellipse([(64, 56), (72, 64)], fill=(255, 255, 255, 255))
    return finalize(img)

# 29. EyeOff: Closed / slashed eye
def make_eye_off():
    img = make_eye()
    draw = ImageDraw.Draw(img)
    # Red strike-through
    draw.line([(4, 4), (28, 28)], fill=(235, 40, 50, 255), width=3)
    return img

# 30. Lock: Golden padlock
def make_lock():
    img = create_base()
    draw = ImageDraw.Draw(img)
    # Steel shackle
    draw.arc([(40, 20), (88, 76)], start=180, end=0, fill=(160, 170, 185, 255), width=10)
    # Golden body
    draw.rounded_rectangle([(32, 56), (96, 112)], radius=8, fill=(255, 195, 30, 255), outline=(210, 150, 10, 255), width=4)
    # Keyhole
    draw.ellipse([(58, 72), (70, 84)], fill=(70, 50, 10, 255))
    draw.polygon([(60, 80), (68, 80), (70, 96), (58, 96)], fill=(70, 50, 10, 255))
    return finalize(img)

# 31. Unlock: Open golden padlock
def make_unlock():
    img = create_base()
    draw = ImageDraw.Draw(img)
    # Open steel shackle (shifted up and rotated)
    draw.arc([(40, 8), (88, 64)], start=180, end=0, fill=(160, 170, 185, 255), width=10)
    draw.line([(40, 36), (40, 60)], fill=(160, 170, 185, 255), width=10)
    # Golden body
    draw.rounded_rectangle([(32, 56), (96, 112)], radius=8, fill=(255, 195, 30, 255), outline=(210, 150, 10, 255), width=4)
    draw.ellipse([(58, 72), (70, 84)], fill=(70, 50, 10, 255))
    draw.polygon([(60, 80), (68, 80), (70, 96), (58, 96)], fill=(70, 50, 10, 255))
    return finalize(img)

ICONS = {
    "pencil": make_pencil,
    "eraser": make_eraser,
    "bucket": make_bucket,
    "eyedropper": make_eyedropper,
    "brush": make_brush,
    "airbrush": make_airbrush,
    "text": make_text,
    "line": make_line,
    "rectangle": make_rectangle,
    "ellipse": make_ellipse,
    "save": make_save,
    "load": make_load,
    "filePlus": make_file_plus,
    "resize": make_resize,
    "rotate": make_rotate,
    "flipH": make_flip_h,
    "flipV": make_flip_v,
    "clear": make_clear,
    "palette": make_palette_logo,
    "layers": make_layers,
    "grid": make_grid,
    "fit": make_fit,
    "plus": make_plus,
    "minus": make_minus,
    "eye": make_eye,
    "eyeOff": make_eye_off,
    "lock": make_lock,
    "unlock": make_unlock,
    "colorEdit": make_color_edit,
    "undo": make_undo,
    "redo": make_redo,
}

def main():
    base_dir = "/home/theking1x/Projetos IA/teste/minipaint/example"
    icons_dir = os.path.join(base_dir, "icons")
    os.makedirs(icons_dir, exist_ok=True)
    header_path = os.path.join(base_dir, "EmbeddedIcons.hpp")
    
    png_bytes_map = {}
    for name, func in ICONS.items():
        img = func()
        png_path = os.path.join(icons_dir, f"{name}.png")
        img.save(png_path, format="PNG")
        with open(png_path, "rb") as f:
            png_bytes_map[name] = f.read()
            
    with open(header_path, "w", encoding="utf-8") as out:
        out.write("#pragma once\n\n")
        out.write("// Authentic MS Paint Style Full-Color Icons\n")
        out.write("// Embedded RGBA PNG assets for standalone MiniPaint\n\n")
        out.write("#include <cstddef>\n#include <cstdint>\n#include <string_view>\n\n")
        out.write("namespace minipaint::example\n{\n")
        out.write("struct EmbeddedIcon { const std::uint8_t* data; std::size_t size; };\n\n")
        
        for name, data in png_bytes_map.items():
            var_name = f"kIcon_{name}"
            out.write(f"static const std::uint8_t {var_name}[] = \n{{\n")
            lines = []
            for i in range(0, len(data), 16):
                chunk = data[i:i+16]
                hex_str = ", ".join(f"0x{b:02x}" for b in chunk)
                lines.append(f"    {hex_str},")
            out.write("\n".join(lines))
            out.write("\n};\n\n")
            
        out.write("struct IconEntry { std::string_view name; const std::uint8_t* data; std::size_t size; };\n\n")
        out.write("static const IconEntry kEmbeddedIcons[] = \n{\n")
        for name, data in png_bytes_map.items():
            out.write(f'    {{"{name}", kIcon_{name}, sizeof(kIcon_{name})}},\n')
        out.write("};\n\n")
        out.write("[[nodiscard]] inline EmbeddedIcon findEmbeddedIcon(std::string_view name)\n{\n")
        out.write("    for (const IconEntry& entry : kEmbeddedIcons)\n    {\n")
        out.write("        if (entry.name == name) return {entry.data, entry.size};\n    }\n")
        out.write("    return {nullptr, 0};\n}\n\n")
        out.write("} // namespace minipaint::example\n")
        
    print(f"Generated {len(ICONS)} icons in {icons_dir} and updated {header_path}")

if __name__ == "__main__":
    main()
