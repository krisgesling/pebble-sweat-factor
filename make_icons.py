#!/usr/bin/env python3
"""
Generate Humid app icons from Twemoji 😅 (sweat smile, CC-BY 4.0).
Black background, emoji centred and scaled to fill ~85% of the canvas.
"""
import urllib.request
import os
from PIL import Image

# Twemoji 😅 = U+1F605
TWEMOJI_URL = 'https://cdn.jsdelivr.net/gh/twitter/twemoji@14.0.2/assets/72x72/1f605.png'
CACHE = '/tmp/twemoji_sweat.png'

if not os.path.exists(CACHE):
    urllib.request.urlretrieve(TWEMOJI_URL, CACHE)
    print(f'Downloaded Twemoji → {CACHE}')

emoji = Image.open(CACHE).convert('RGBA')

out_dir = os.path.join(os.path.dirname(__file__), 'resources')
os.makedirs(out_dir, exist_ok=True)

for size in [80, 144]:
    bg = Image.new('RGBA', (size, size), (0, 0, 0, 255))
    # Scale emoji to 88% of canvas with a small margin
    emoji_size = int(size * 0.88)
    scaled = emoji.resize((emoji_size, emoji_size), Image.Resampling.LANCZOS)
    offset = (size - emoji_size) // 2
    bg.paste(scaled, (offset, offset), scaled)
    path = os.path.join(out_dir, f'icon_{size}x{size}.png')
    bg.save(path)
    print(f'Humid {size}x{size} → {path}')

print('\nDone.')
