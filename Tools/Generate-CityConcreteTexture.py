"""#125 reproducible seamless 1m concrete aggregate; no external image inputs."""
from pathlib import Path
import hashlib, json
import numpy as np
from PIL import Image
root = Path(__file__).resolve().parents[1]
rng = np.random.default_rng(125)
size=512
# Periodic low-frequency mottling plus fine aggregate. No painted slab border:
# foundation geometry supplies the edges and the same tile works on the walk.
y,x=np.mgrid[:size,:size]/size
mottle=sum(np.sin(2*np.pi*(a*x+b*y)+p)*w for a,b,p,w in
    [(2,3,.3,5),(5,-2,1.7,3),(11,7,2.3,1.8),(23,-17,.7,1)])
grain=rng.normal(0,3,(size,size))
pores=rng.random((size,size))<.012
base=np.clip(151+mottle+grain-pores*18,0,255)
rgb=np.stack([base*1.015,base,base*.965],axis=-1).astype('uint8')
out=root/'ArtSource/Textures/HomeGroundPilot/Concrete_BaseColor.png'
out.parent.mkdir(parents=True,exist_ok=True);Image.fromarray(rgb).save(out)
(out.parent/'Concrete.json').write_text(json.dumps(dict(issue=125,recipe='Generate-CityConcreteTexture.py',
    seed=125,size_px=size,tile_metres=1,srgb=True,roughness=.9,metallic=0,
    sha256=hashlib.sha256(out.read_bytes()).hexdigest()),indent=2)+'\n')
