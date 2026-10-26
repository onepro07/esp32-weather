#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""生成天气 PWA 图标：黎明渐变天空 + 发光太阳"""
from PIL import Image, ImageDraw, ImageFilter
import os, math

OUT = r"C:/Users/HP/WorkBuddy/2026-10-13-12-19-25/esp32-weather/icons"
os.makedirs(OUT, exist_ok=True)
S = 1024

# 背景：上深蓝 → 下暖橙
base = Image.new("RGB", (S, S))
d = ImageDraw.Draw(base)
for i in range(S):
    t = i / (S - 1)
    if t < 0.62:
        k = t / 0.62
        c = (int(12 + (40 - 12) * k), int(22 + (58 - 22) * k), int(62 + (110 - 62) * k))
    else:
        k = (t - 0.62) / 0.38
        c = (int(40 + (176 - 40) * k), int(58 + (96 - 58) * k), int(110 + (48 - 110) * k))
    d.line([(0, i), (S, i)], fill=c)

cx, cy = int(S * 0.5), int(S * 0.44)

# 太阳光晕
glow = Image.new("RGBA", (S, S), (0, 0, 0, 0))
gd = ImageDraw.Draw(glow)
gd.ellipse([cx - 210, cy - 210, cx + 210, cy + 210], fill=(255, 196, 108, 200))
glow = glow.filter(ImageFilter.GaussianBlur(110))
base = Image.alpha_composite(base.convert("RGBA"), glow)

# 太阳本体
sun = Image.new("RGBA", (S, S), (0, 0, 0, 0))
sd = ImageDraw.Draw(sun)
sd.ellipse([cx - 128, cy - 128, cx + 128, cy + 128], fill=(255, 226, 158, 255))
sun = sun.filter(ImageFilter.GaussianBlur(10))
base = Image.alpha_composite(base, sun)

# 暖色高光
hl = Image.new("RGBA", (S, S), (0, 0, 0, 0))
hd = ImageDraw.Draw(hl)
hd.ellipse([cx - 74, cy - 92, cx + 10, cy - 8], fill=(255, 255, 255, 120))
hl = hl.filter(ImageFilter.GaussianBlur(28))
base = Image.alpha_composite(base, hl)

for name, px in {"icon-180.png": 180, "icon-192.png": 192, "icon-512.png": 512}.items():
    base.resize((px, px), Image.LANCZOS).save(os.path.join(OUT, name), "PNG", optimize=True)
    print(f"  {name:<20} {px}x{px}")

mask = Image.new("RGBA", (S, S), (10, 18, 46, 255))
inner = base.resize((int(S * 0.78), int(S * 0.78)), Image.LANCZOS)
off = (S - inner.width) // 2
mask.alpha_composite(inner, (off, off))
mask.resize((512, 512), Image.LANCZOS).save(os.path.join(OUT, "icon-maskable.png"), "PNG", optimize=True)
print(f"  {'icon-maskable.png':<20} 512x512")

base.resize((512, 512), Image.LANCZOS).save(os.path.join(OUT, "_preview.png"), "PNG")
print("\n图标已生成 →", OUT)
