"""Author a small, neutral tabletop-room specular reference in linear HDR.

No sun disc, emissive material, diffuse-light change or live scene capture.
The dark lower hemisphere and broad overhead gradient expose brushed-metal form.
"""
from pathlib import Path
import hashlib
import json
import math

root = Path(__file__).resolve().parents[1]
out = root / 'ArtSource/Textures/BuildingSurfaces/city_room_reflection.hdr'
width, height = 512, 256
data = bytearray(f'#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y {height} +X {width}\n'.encode())
for y in range(height):
    up = math.cos(math.pi * (y + .5) / height)
    blend = .5 + .5 * math.tanh(up * 3)
    channels = [bytearray() for _ in range(4)]
    for x in range(width):
        # A soft, broad overhead patch reads as room illumination at miniature scale.
        longitude = 2 * math.pi * (x + .5) / width
        patch = .08 * max(up, 0) * (.5 + .5 * math.cos(longitude))
        # Broad directional room-light features expose reflection changes on
        # polished/corrugated steel; a nearly uniform cube reads as flat gray.
        patch += .8 * math.exp(-((up-.58)/.23)**2) * (.5+.5*math.cos(longitude*3+.4))**10
        # Vertical metal walls viewed from above reflect the lower hemisphere.
        # Give it broad room/floor contrast as well, rather than uniform gray.
        patch += .42 * math.exp(-((up+.38)/.32)**2) * (.5+.5*math.cos(longitude*3-1.1))**8
        rgb = [low + blend * (high - low) + patch for low, high in
               zip((.08, .09, .10), (.40, .42, .44))]
        mantissa, exponent = math.frexp(max(rgb))
        scale = mantissa * 256 / max(rgb)
        for channel, value in zip(channels, (*[int(v * scale) for v in rgb], exponent + 128)):
            channel.append(value)
    data.extend((2, 2, width >> 8, width & 255))
    for channel in channels:
        for first in range(0, width, 128):
            packet = channel[first:first + 128]
            data.append(len(packet))
            data.extend(packet)
out.write_bytes(data)
out.with_suffix('.json').write_text(json.dumps(dict(
    purpose='Neutral room specular reflection with broad light features; diffuse lights unchanged',
    source_size=[width, height], native_cube_max_size=64, live_recapture=False,
    sha256=hashlib.sha256(data).hexdigest()), indent=2) + '\n')
print(out)
