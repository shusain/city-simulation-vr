"""Deterministic, original tiling PBR maps for the D79 surface prototype.

Run with bundled Python (NumPy/Pillow). Height gradients produce DirectX normal
maps for Unreal; Blender flips their green channel in its preview shader.
"""
from pathlib import Path
import hashlib
import json
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'ArtSource/Textures/BuildingSurfaces'
OUT.mkdir(parents=True, exist_ok=True)
rng = np.random.default_rng(79)
records = []

def save(name, pixels, role):
    path = OUT / (name + '.png')
    Image.fromarray(np.uint8(np.clip(pixels, 0, 1) * 255 + .5)).save(path)
    records.append(dict(file=path.relative_to(ROOT).as_posix(), role=role,
                        width=pixels.shape[1], height=pixels.shape[0],
                        sha256=hashlib.sha256(path.read_bytes()).hexdigest()))

def normal(height, width_m, height_m):
    # PNG rows increase downward. OpenGL tangent green is +dHeight/dRow;
    # DirectX uses the opposite sign. Keep physical pixel spacing in metres.
    dx = (np.roll(height, -1, 1) - np.roll(height, 1, 1)) / (2 * width_m / height.shape[1])
    dy = (np.roll(height, -1, 0) - np.roll(height, 1, 0)) / (2 * height_m / height.shape[0])
    n = np.stack((-dx, -dy, np.ones_like(dx)), -1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    return n * .5 + .5

# Six 200mm bricks across, sixteen 75mm courses, in a 1.2m square tile.
size = 1024
y, x = np.mgrid[:size, :size] / size
row = np.floor(y * 16).astype(int)
u = (x * 6 + (row % 2) * .5) % 1
v = (y * 16) % 1
edge = np.minimum(np.minimum(u, 1-u) * .2, np.minimum(v, 1-v) * .075)
bevel = np.clip((edge - .0025) / .0025, 0, 1)
bevel = bevel * bevel * (3 - 2 * bevel)
brick = np.floor(x * 6 + (row % 2) * .5).astype(int) % 6
variation = rng.uniform(-.045, .045, (16, 6))[row, brick]
grain = rng.normal(0, .011, (size, size))
# The tile is neutral masonry, tintable to any paint color by the material.
gray = .69 + variation + grain
base = np.repeat((.40 * (1-bevel) + gray * bevel)[..., None], 3, -1)
base *= np.array([1.00, .985, .955])
height = .0025 * bevel + grain * .00065 * bevel
ao = .84 + .16 * bevel
rough = np.clip(.94 * (1-bevel) + (.84 + grain) * bevel, 0, 1)
save('CityBrick_BaseColor', base, 'sRGB base color; tintable neutral masonry')
save('CityBrick_Normal', normal(height, 1.2, 1.2), 'linear DirectX tangent normal')
save('CityBrick_ORM', np.stack((ao, rough, np.zeros_like(ao)), -1), 'linear R=AO G=roughness B=metallic(0)')

size = 512
y, x = np.mgrid[:size, :size] / size
# Periodic brushed/grain detail; no visible baked highlights or lighting.
grain = rng.normal(0, .006, (size, size))
brush = .008*np.sin(x * np.pi * 2 * 96) + .004*np.sin(x * np.pi * 2 * 177)
gray = .70 + grain + brush
save('CityMechanical_BaseColor', np.repeat(gray[..., None], 3, -1), 'sRGB neutral painted/exposed steel')
save('CityMechanical_Normal', normal((grain + brush) * .00018, .5, .5), 'linear DirectX tangent normal')
save('CityMechanical_ORM', np.stack((np.ones_like(gray), .42 + grain + brush, np.ones_like(gray)), -1), 'linear R=AO G=bare-metal roughness B=metallic(1); vertex R selects paint/metal')

# Validate wrap slopes and normal lengths, rather than forcing equal opposite
# texels (the sample at 1.0 is the repeat of 0.0, not the final texel).
for record in records:
    data = np.asarray(Image.open(ROOT / record['file'])).astype(float) / 255
    if 'Normal' in record['file']:
        lengths = np.linalg.norm(data * 2 - 1, axis=-1)
        assert np.max(abs(lengths-1)) < .008
    mean_step = max(np.mean(abs(np.diff(data, axis=a))) for a in (0, 1))
    seam_step = max(np.mean(abs(data[0]-data[-1])), np.mean(abs(data[:, 0]-data[:, -1])))
    record['wrap_mean_step'] = round(float(seam_step), 6)
    record['interior_mean_step'] = round(float(mean_step), 6)
    assert seam_step < max(.05, mean_step * 4), record

metadata = dict(task='D79', provenance='Original deterministic project-generated textures; no external assets',
                brick_tile_m=[1.2, 1.2], mechanical_tile_m=[.5, .5],
                normal_convention='DirectX (+green in Unreal); preview flips green for Blender', textures=records)
(OUT / 'building_surfaces.json').write_text(json.dumps(metadata, indent=2)+'\n', encoding='utf-8')
print('CITYSIM_BUILDING_SURFACE_TEXTURES_PASS', len(records))
