"""Bake a small seamless mask: red = subtle grain, green = slab joints."""
from pathlib import Path
from PIL import Image
import math
root = Path(__file__).resolve().parent.parent
out = root / 'ArtSource' / 'Textures'
out.mkdir(exist_ok=True)
im = Image.new('RGB', (64, 64))
for y in range(64):
    for x in range(64):
        grain = .5 + .025*math.sin(2*math.pi*(3*x+5*y)/64) + .02*math.cos(2*math.pi*(7*x-2*y)/64)
        im.putpixel((x,y), (round(grain*255), 180 if x<2 or y<2 else 255, 255))
im.save(out / 'CityPavement.png')
