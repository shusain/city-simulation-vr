"""D88 compose labeled source GIFs/filmstrips from the saved Blender renders."""
from pathlib import Path
import json
from PIL import Image,ImageDraw,ImageFont
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D88/Preview'
doc=json.loads((ROOT/'ArtSource/Manifests/city_citizen_sports_refinement.json').read_text())
def font(size):return ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',size)
jobs=doc['scenes']+[dict(clip='BaseballPair',period=72)]
titles={'BaseballPitch':'Pitch: gather / leg lift / plant / release / follow-through',
        'BaseballSwing':'Bat: load / two-handed swing / strike / follow-through',
        'SoccerKick':'Kick: backswing / planted support / contact / follow-through',
        'SoccerRun':'Run: opposing arms / stance / recovery / rolling ball',
        'GoalkeeperSave':'Keeper: ready crouch / reach / catch / gather',
        'BaseballPair':'Baseball: synchronized pitch and bat contact'}
for e in jobs:
    frames=[]
    for f in range(1,e['period']+1,2):
        raw=Image.open(OUT/f'{e["clip"]}-{f:03}.png').convert('RGB')
        panel=Image.new('RGB',(720,660),(20,30,44));panel.paste(raw,(0,50));d=ImageDraw.Draw(panel)
        d.text((15,12),titles[e['clip']],font=font(21),fill=(235,243,252))
        d.text((15,633),f'D88 Blender source | frame {f:02} / {e["period"]} | control-driven poses',font=font(17),fill=(174,196,216))
        frames.append(panel)
    frames[0].save(OUT/(e['clip']+'.gif'),save_all=True,append_images=frames[1:],duration=[80,90,80]*(len(frames)//3),loop=0)
    indices=sorted(set((0,len(frames)//4,len(frames)//2,3*len(frames)//4)))
    strip=Image.new('RGB',(1440,660),(20,30,44))
    for i,n in enumerate(indices):strip.paste(frames[n].resize((720,330)),((i%2)*720,(i//2)*330))
    strip.save(OUT/(e['clip']+'-Filmstrip.png'))
print('CITYSIM_D88_PREVIEW_MEDIA_PASS 6 source GIFs;6 filmstrips')
