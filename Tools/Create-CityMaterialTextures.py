"""Original deterministic timber, clapboard and corrugated-metal PBR study maps."""
from pathlib import Path
import argparse
import hashlib
import json
import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]


def normal(height, tile_m=2.4):
    step = tile_m/height.shape[0]
    dx = (np.roll(height, -1, 1)-np.roll(height, 1, 1))/(2*step)
    dy = (np.roll(height, -1, 0)-np.roll(height, 1, 0))/(2*step)
    value = np.stack((-dx, -dy, np.ones_like(dx)), -1)
    value /= np.linalg.norm(value, axis=-1, keepdims=True)
    return value*.5+.5


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, default=ROOT/'ArtSource/Textures/MaterialStudy')
    args = parser.parse_args()
    out = args.output.resolve(); out.mkdir(parents=True, exist_ok=True)
    rng = np.random.default_rng(104)
    size = 512
    y, x = np.mgrid[:size, :size]/size
    noise = rng.normal(0, .007, (size, size))
    maps = {}

    def save_surface(name, base, height, ao, rough, metallic):
        maps[name] = dict(tile_m=[2.4, 2.4], maps={})
        for role, pixels in [('BaseColor', base), ('Normal', normal(height)),
                             ('ORM', np.stack((ao, rough, metallic), -1))]:
            path = out/f'{name}_{role}.png'
            Image.fromarray(np.uint8(np.clip(pixels, 0, 1)*255+.5)).save(path)
            actual = np.asarray(Image.open(path))/255
            if role == 'Normal':
                assert np.max(abs(np.linalg.norm(actual*2-1, axis=-1)-1)) < .008
            seam = max(np.mean(abs(actual[0]-actual[-1])), np.mean(abs(actual[:, 0]-actual[:, -1])))
            interior = max(np.mean(abs(np.diff(actual, axis=a))) for a in (0, 1))
            assert seam < max(.055, interior*4), (name, role, seam, interior)
            maps[name]['maps'][role] = dict(file=path.relative_to(ROOT).as_posix(),
                sha256=hashlib.sha256(path.read_bytes()).hexdigest(), size=[size, size],
                seam_mean=float(seam), interior_mean=float(interior))

    # Eight 300mm vertical timber boards; periodic meandering grain and knots.
    board = np.floor(x*8).astype(int)
    seam = np.minimum((x*8)%1, 1-(x*8)%1)
    bevel = np.clip((seam-.009)/.025, 0, 1)
    grain = .032*np.sin(2*np.pi*(x*92 + .25*np.sin(2*np.pi*y*3)))
    grain += .014*np.sin(2*np.pi*(x*173 + .17*np.sin(2*np.pi*y*5)))
    knots = np.zeros_like(x)
    for cx, cy in [(0.188, .3), (.68, .72)]:
        dx = (x-cx+.5)%1-.5; dy = (y-cy+.5)%1-.5
        radius = np.sqrt((dx/.06)**2+(dy/.15)**2)
        knots += .07*np.exp(-radius*radius)*np.cos(radius*14)
    variation = rng.uniform(-.035, .035, 8)[board]
    tone = .63+grain+noise+variation+knots
    base = np.stack((tone*1.05, tone*.78, tone*.52), -1)*(.62+.38*bevel[..., None])
    height = .0018*bevel + (grain+knots)*.0009
    save_surface('Timber', base, height, .78+.22*bevel,
                 np.clip(.69+noise-grain*.2, 0, 1), np.zeros_like(x))

    # Twelve horizontal 200mm painted clapboard courses, restrained grain.
    course = (y*12)%1
    edge = np.minimum(course, 1-course)
    bevel = np.clip((edge-.018)/.08, 0, 1)
    lap = .89+.11*bevel
    base = np.repeat((.78*lap+noise*.35)[..., None], 3, -1)
    height = .0018*bevel + .00025*course*bevel
    save_surface('Clapboard', base, height, .9+.1*bevel,
                 np.clip(.67+noise*.2, 0, 1), np.zeros_like(x))

    # Sixteen 150mm corrugations across the same physical tile.
    rib = .5+.5*np.cos(2*np.pi*x*16)
    fleck = .012*np.sin(2*np.pi*(x*127+y*79)) + noise
    base = np.repeat((.72+fleck)[..., None], 3, -1)
    save_surface('Corrugated', base, .005*rib+fleck*.0001,
                 .96+.04*rib, np.clip(.36+fleck, 0, 1), np.ones_like(x))
    catalog = dict(schema_version=1, task='D104', seed=104,
        provenance='Original project-generated seamless maps; no external assets or baked lighting',
        normal_convention='DirectX; green inverted in Blender', surfaces=maps,
        options={
            'Home': [('Timber', 'Cedar', [1, 1, 1], 0), ('Clapboard', 'Sage siding', [.42, .61, .48], 0), ('Corrugated', 'Cream metal panels', [.87, .82, .68], 0)],
            'Shop': [('Timber', 'Warm timber', [.88, .82, .72], 0), ('Clapboard', 'Blue siding', [.30, .49, .67], 0), ('Corrugated', 'Teal metal panels', [.22, .48, .48], 0)],
            'Factory': [('Timber', 'Weathered timber', [.65, .69, .67], 0), ('Clapboard', 'Gray siding', [.53, .59, .62], 0), ('Corrugated', 'Galvanized steel', [.78, .82, .85], 1)]})
    # Neutral full-sphere reflection source, matching the Blender world's
    # linear color. Unlike a sky-only cube it also lights downward metal rays.
    hdr = out/'PreviewEnvironment.hdr'
    hdr.write_bytes(b'#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 128 +X 256\n'+bytes([102, 128, 169, 127])*(128*256))
    catalog['preview_environment'] = dict(file=hdr.relative_to(ROOT).as_posix(),
        sha256=hashlib.sha256(hdr.read_bytes()).hexdigest(), native='/Game/CityPreview/Textures/T_StudioEnvironment',
        linear_color=[102/512, 128/512, 169/512], provenance='Original constant full-sphere studio reflection; no external HDRI')
    (out/'Materials.json').write_text(json.dumps(catalog, indent=2)+'\n')
    print('CITYSIM_D104_TEXTURES_PASS 9 seamless 512px PBR maps; DirectX normals verified')


if __name__ == '__main__':
    main()
