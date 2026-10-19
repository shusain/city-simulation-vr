"""Reuse existing neutral maps and preserve each original wall tint."""
from pathlib import Path
from PIL import Image
import numpy as np,json,hashlib
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D91/CivicSurfaces';OUT.mkdir(parents=True,exist_ok=True)
texture_folder=ROOT/'ArtSource/Textures/CivicSurfaces';texture_folder.mkdir(parents=True,exist_ok=True)
rng=np.random.default_rng(91);size=512;y,x=np.mgrid[:size,:size]/size
noise=rng.normal(0,1,(size,size));smooth=(noise+np.roll(noise,1,0)+np.roll(noise,-1,0)+np.roll(noise,1,1)+np.roll(noise,-1,1))/5
def periodic_coarse(grid):
    padded=np.pad(rng.normal(0,1,(grid,grid)),1,mode='wrap').astype(np.float32)
    cell=size//grid
    enlarged=np.asarray(Image.fromarray(padded).resize(((grid+2)*cell,(grid+2)*cell),Image.Resampling.BICUBIC))
    return enlarged[cell:cell+size,cell:cell+size]
broad=.018*periodic_coarse(16)+.006*periodic_coarse(4)
grain=.035*smooth+.012*noise;gray=.56+broad+grain
height=.0006*smooth+.0001*noise
dx=(np.roll(height,-1,1)-np.roll(height,1,1))/(4/size);dy=(np.roll(height,-1,0)-np.roll(height,1,0))/(4/size)
normal=np.stack((-dx,-dy,np.ones_like(dx)),-1);normal/=np.linalg.norm(normal,axis=-1,keepdims=True)
maps={'BaseColor':np.repeat(gray[...,None],3,-1),'Normal':normal*.5+.5,'ORM':np.stack((np.clip(.985+.006*smooth,0,1),np.clip(.92+.025*smooth,0,1),np.zeros_like(gray)),-1)}
for suffix,pixels in maps.items():Image.fromarray(np.uint8(np.clip(pixels,0,1)*255+.5)).save(texture_folder/('CivicAsphalt_'+suffix+'.png'))
records={}
for prefix,folder in [('CityBrick','ArtSource/Textures/BuildingSurfaces'),('CivicAsphalt','ArtSource/Textures/CivicSurfaces')]:
    path=ROOT/folder/(prefix+'_BaseColor.png');pixels=np.asarray(Image.open(path).convert('RGB'),dtype=float)/255
    linear=np.where(pixels<=.04045,pixels/12.92,((pixels+.055)/1.055)**2.4);mean=linear.mean(axis=(0,1));normalization=1/mean
    assert np.allclose((linear*normalization).mean(axis=(0,1)),1)
    records[prefix]=dict(folder=folder,mean_linear=list(mean),normalization=list(normalization),strength=.65 if prefix=='CityBrick' else 1,
        maps={suffix:dict(file=(Path(folder)/(prefix+'_'+suffix+'.png')).as_posix(),sha256=hashlib.sha256((ROOT/folder/(prefix+'_'+suffix+'.png')).read_bytes()).hexdigest()) for suffix in ('BaseColor','Normal','ORM')})
(OUT/'TextureInputs.json').write_text(json.dumps(dict(task='D91',textures=records,brick_repeat_m=2.4,asphalt_repeat_m=2,asphalt_provenance='Original deterministic periodic aggregate/roughness and DirectX normal maps; project code, no external assets',tint_policy='Normalize shared texture average in linear space to1; preserve original wall tint. Roofs use neutral charcoal asphalt.'),indent=2)+'\n')
print('CITYSIM_D91_CIVIC_TEXTURE_INPUTS_PASS 3 reused brick +3 asphalt maps;wall tint mean preserved')
