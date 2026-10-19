"""Bake a small periodic grass/soil variation mask; no runtime noise or external assets."""
from pathlib import Path
import math
import random
from PIL import Image

root = Path(__file__).resolve().parent.parent
size = 128
rng = random.Random(7429)
waves = [(rng.randint(1, 5), rng.randint(-5, 5), rng.random() * math.tau, 0.026)
         for _ in range(12)]
waves += [(rng.randint(9, 35), rng.randint(-35, 35), rng.random() * math.tau, 0.006)
          for _ in range(24)]
pixels = []
for y in range(size):
    for x in range(size):
        value = 0.5 + sum(amplitude * math.sin(math.tau * (fx*x + fy*y) / size + phase)
                          for fx, fy, phase, amplitude in waves)
        gray = round(max(0.32, min(0.68, value)) * 255)
        pixels.append((gray, gray, gray))
image = Image.new('RGB', (size, size))
image.putdata(pixels)
path = root / 'ArtSource/Textures/CityTerrain.png'
path.parent.mkdir(parents=True, exist_ok=True)
image.save(path)
print(path)
