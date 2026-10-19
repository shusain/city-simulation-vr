"""Compose captured native D83 evidence; no source/native asset edits."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont
import json
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Artifacts/D83/Integration'
font=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',22)
small=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',17)
def sheet(directory,names,destination,columns,title):
    width=480;height=400;rows=(len(names)+columns-1)//columns
    canvas=Image.new('RGB',(width*columns,height*rows+70),'#18202a');draw=ImageDraw.Draw(canvas)
    draw.text((16,12),title,font=font,fill='white')
    draw.text((16,40),'Unreal native renders | physical headset appearance remains pending',font=small,fill='#ccd8e4')
    for i,name in enumerate(names):
        x=(i%columns)*width;y=70+(i//columns)*height
        im=Image.open(directory/(name+'.png')).convert('RGB');im.thumbnail((460,355),Image.Resampling.LANCZOS)
        canvas.paste(im,(x+(width-im.width)//2,y));draw.text((x+12,y+360),name.replace('SM_Building_',''),font=small,fill='white')
    canvas.save(destination)
preview=OUT/'Preview'
names=['Basic-Day','Neighborhood-Day','City-Day','Baseball-Day','Soccer-Day','Skate-02']
sheet(preview,names,OUT/'Native-Parks.png',3,'D83 integrated park trees, ramps and citizen meshes')
frames=[]
for i in range(7):
    im=Image.open(preview/f'Skate-{i:02}.png').convert('RGB');im.thumbnail((1000,750),Image.Resampling.LANCZOS)
    d=ImageDraw.Draw(im);d.rectangle((0,0,im.width,36),fill='#18202a')
    d.text((12,7),f'Unreal D83 | forward kickflip / ground180 | source t={(.9,1.1,1.3,1.5,1.7,2.1,2.6)[i]:.1f}s',font=small,fill='white')
    frames.append(im)
cut=Image.new('RGB',frames[0].size,'#18202a');ImageDraw.Draw(cut).text((30,50),'Replay cut to approach (one-shot pass)',font=font,fill='white');frames.append(cut)
frames[0].save(OUT/'Native-Skate.gif',save_all=True,append_images=frames[1:],duration=[200,200,200,200,400,500,500,650],loop=0,disposal=2)
static=OUT/'StaticPreview'
if (static/'Report.json').exists():
    report=json.loads((static/'Report.json').read_text());assert not report['error']
    sheet(static,[v['view'] for v in report['views']],OUT/'Native-Variants.png',4,'D83 normal, damaged and December variants | desktop / Quest LODs')
print('CITYSIM_D83_NATIVE_MEDIA_PASS')
