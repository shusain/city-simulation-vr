"""Label actual Blender/Unreal captures and verify their delivered dimensions."""
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont, ImageStat
import json,hashlib,sys
ROOT=Path(__file__).resolve().parents[1]
scale='--home-scale' in sys.argv
details=scale or '--home-details' in sys.argv
OUT=ROOT/('Artifacts/D91/Scale' if scale else 'Artifacts/D91/Details' if details else 'Artifacts/D91')
font=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',28)
small=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',20)
names=('Before','Refined') if scale else ('WindowsOnly','Refined') if details else ('Before','Opaque','Translucent')
titles=('Previous: street0.90m / park1.40m','Refined: street1.43m / park1.40m') if scale else ('Accepted opaque windows','Door + chimney details') if details else ('Original dark squares','Frames + opaque glass','Frames +35% opacity')
framings=('Whole','Entrance','Chimney') if scale else ('Whole','Door','Chimney') if details else ('Whole','Window')
prefix='SM_HomeScale_' if scale else 'SM_HomeDetails_' if details else 'SM_HomeWindows_'
verified=[]
for engine in ('Blender','Unreal'):
    lights=('Day','Night') if engine=='Unreal' else ('Day',)
    for lighting in lights:
        height=150+470*len(framings)
        sheet=Image.new('RGB',(620*len(names),height),(24,30,40));draw=ImageDraw.Draw(sheet)
        label='citizen scale + matching brick' if scale else 'details' if details else 'windows'
        draw.text((24,15),f'D91 | Low-density home {label} | {engine} | {lighting}',font=font,fill='white')
        for column,(name,title) in enumerate(zip(names,titles)):
            x=column*620+10;draw.text((x+10,62),title,font=small,fill=(205,221,235))
            for row,framing in enumerate(framings):
                file=OUT/engine/(prefix+name+('-'+lighting if engine=='Unreal' else '')+'-'+framing+'.png')
                with Image.open(file) as im:
                    assert im.size==(1200,900), (file,im.size)
                    extrema=ImageStat.Stat(im.convert('RGB')).extrema
                    assert any(high-low>30 for low,high in extrema), 'Blank capture'
                    tile=im.convert('RGB').resize((600,450),Image.Resampling.LANCZOS)
                    # Native scene-capture PNGs can have inverted/zero background
                    # alpha; comparisons preserve RGB rather than masking pixels.
                    sheet.paste(tile,(x,100+row*470))
                verified.append(dict(file=file.relative_to(ROOT).as_posix(),size=[1200,900],sha256=hashlib.sha256(file.read_bytes()).hexdigest()))
        draw.text((24,height-40),'Matched cameras within each renderer; separate source/material test, no gameplay or headset acceptance.',font=small,fill=(174,190,208))
        sheet.save(OUT/(engine+'-'+lighting+'-Comparison.png'))
(OUT/'CaptureVerification.json').write_text(json.dumps(dict(task='D91',captures=verified,verified_dimensions=True),indent=2)+'\n')
print('CITYSIM_D91_SHEETS_PASS',len(verified),'captures')
