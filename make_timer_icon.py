#!/usr/bin/env python3
"""
Generates the Timer menu icon (timer.bin) as an LVGL v8 indexed 1-bit image,
in the same format as the existing menu icons (find.bin, level.bin, ...).

Usage:
  python make_timer_icon.py <spiffs_image folder>     -> writes theme1..theme9/menu/timer.bin
  python make_timer_icon.py <folder> --preview x.png  -> also writes a PNG preview

The icon colour is copied from each theme's own find.bin / level.bin, so every theme
gets a clock in its own accent colour. (If a theme's menu folder is missing, it is skipped.)
"""
import math, os, struct, sys
from PIL import Image, ImageDraw

W, H = 45, 49

def draw_icon():
    im = Image.new('1', (W, H), 0)
    d = ImageDraw.Draw(im)
    cx, cy = 22, 27
    R = 20

    def circle(r, fill=False):
        # midpoint circle, 1px wide, no antialiasing
        x, y, err = r, 0, 1 - r
        pts = set()
        while x >= y:
            for dx, dy in ((x, y), (y, x), (-y, x), (-x, y), (-x, -y), (-y, -x), (y, -x), (x, -y)):
                pts.add((cx + dx, cy + dy))
            y += 1
            if err < 0:
                err += 2 * y + 1
            else:
                x -= 1
                err += 2 * (y - x) + 1
        for p in pts:
            im.putpixel(p, 1)
        if fill:
            d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=1)

    # case: outer + inner ring (double line like the phone icons)
    circle(R)
    circle(R - 3)

    # crown on top: cap + stem
    d.rectangle((16, 0, 28, 2), outline=1)          # cap
    d.rectangle((19, 3, 25, 6), outline=1)          # stem
    d.line((20, 3, 24, 3), fill=0)                  # open the join to the cap
    d.line((20, 6, 24, 6), fill=0)                  # open the join to the case

    # side button, top right, 45 degrees
    a = math.radians(45)
    bx = cx + (R + 1) * math.sin(a)
    by = cy - (R + 1) * math.cos(a)
    ux, uy = math.sin(a), -math.cos(a)               # outward
    vx, vy = math.cos(a), math.sin(a)                # sideways
    def P(o, s):
        return (round(bx + ux * o + vx * s), round(by + uy * o + vy * s))
    d.polygon([P(0, -2), P(5, -2), P(5, 2), P(0, 2)], outline=1)
    d.line([P(0, -2), P(0, 2)], fill=0)

    # tick marks on the inner ring: 12 positions
    for i in range(12):
        ang = math.radians(i * 30)
        r_out = R - 5
        r_in = R - 9 if i % 3 == 0 else R - 7
        x1 = cx + r_in * math.sin(ang); y1 = cy - r_in * math.cos(ang)
        x2 = cx + r_out * math.sin(ang); y2 = cy - r_out * math.cos(ang)
        d.line((round(x1), round(y1), round(x2), round(y2)), fill=1)

    # hands: pointing at about 10:10 style, minute hand up to the right
    def hand(deg, length):
        ang = math.radians(deg)
        d.line((cx, cy, round(cx + length * math.sin(ang)), round(cy - length * math.cos(ang))), fill=1)
    hand(60, 10)     # minute hand
    hand(-60, 7)     # hour hand

    # filled hub, like the marker dot in find.bin
    d.ellipse((cx - 2, cy - 2, cx + 2, cy + 2), fill=1)

    return im

def encode(im, rgba):
    """LVGL v8 LV_IMG_CF_INDEXED_1BIT (=7): header, 2-entry BGRA palette, MSB-first rows."""
    w, h = im.size
    header = 7 | (w << 10) | (h << 21)
    r, g, b, a = rgba
    out = struct.pack('<I', header)
    out += bytes((0, 0, 0, 0))              # colour 0: transparent
    out += bytes((b, g, r, a))              # colour 1: accent
    rowb = (w + 7) // 8
    px = im.load()
    for y in range(h):
        row = bytearray(rowb)
        for x in range(w):
            if px[x, y]:
                row[x // 8] |= 0x80 >> (x % 8)
        out += bytes(row)
    return out

def read_accent(path):
    d = open(path, 'rb').read()
    h = struct.unpack('<I', d[:4])[0]
    cf = h & 0x1f
    n = {7: 2, 8: 4, 9: 16, 10: 256}[cf]
    for i in range(1, n):
        b, g, r, a = d[4 + i * 4: 8 + i * 4]
        if a == 255:
            return (r, g, b, a)
    return None

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(__doc__); sys.exit(1)
    root = sys.argv[1]
    icon = draw_icon()
    if '--preview' in sys.argv:
        icon.convert('RGB').resize((W * 6, H * 6), Image.NEAREST).save(sys.argv[sys.argv.index('--preview') + 1])
    for n in range(1, 10):
        menu = os.path.join(root, f'theme{n}', 'menu')
        if not os.path.isdir(menu):
            print(f'theme{n}: no menu folder, skipped'); continue
        color = None
        for ref in ('find.bin', 'level.bin', 'settings.bin', 'theremin.bin'):
            p = os.path.join(menu, ref)
            if os.path.isfile(p):
                color = read_accent(p)
                if color: break
        if not color:
            print(f'theme{n}: could not read an accent colour, skipped'); continue
        with open(os.path.join(menu, 'timer.bin'), 'wb') as f:
            f.write(encode(icon, color))
        print(f'theme{n}: wrote timer.bin, accent RGB{color[:3]}')
