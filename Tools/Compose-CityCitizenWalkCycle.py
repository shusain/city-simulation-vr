"""D121 GIFs/contact sheet and a local source-review page from actual renders."""
from pathlib import Path
import hashlib
import json
from PIL import Image, ImageDraw, ImageFont

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Artifacts/D121/V2/Preview'
doc=json.loads((OUT/'RenderManifest.json').read_text())
font=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',20)
small=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',16)
titles={'Outfits':'Casual / Office / Worker - relaxed walk', 'Side':'Side view - foot contact and opposing arm swing',
        'Front':'Front view - stride width and body sway', 'Travel':'Travelling preview - three cycles across fixed ground marks'}
media={}
for name,view in doc['views'].items():
    frames=[]
    for i,file in enumerate(view['files']):
        raw=Image.open(OUT/file).convert('RGB')
        panel=Image.new('RGB',(1000,700),(20,30,44));panel.paste(raw,(0,45))
        draw=ImageDraw.Draw(panel)
        draw.text((16,10),titles[name],font=font,fill=(235,243,252))
        draw.text((16,673),'D121 v2 | forearm-aligned hands / torso twist / forward lean | awaiting review',font=small,fill=(174,196,216))
        frames.append(panel)
    # GIF stores duration in 10ms increments. Alternate 80/90ms to retain 12fps.
    durations=[80 if i%3!=2 else 90 for i in range(len(frames))]
    frames[0].save(OUT/f'{name}.gif',save_all=True,append_images=frames[1:],duration=durations,loop=0)
    media[name]=dict(path=f'{name}.gif',sha256=hashlib.sha256((OUT/f'{name}.gif').read_bytes()).hexdigest(),frames=len(frames))
    if name=='Side':
        sheet=Image.new('RGB',(1500,700),(20,30,44));d=ImageDraw.Draw(sheet)
        for i,index in enumerate((0,2,4,7,9,11)):
            tile=frames[index].resize((500,350))
            sheet.paste(tile,((i%3)*500,(i//3)*350))
            d.text(((i%3)*500+8,(i//3)*350+38),f'{chr(65+i%3)}{1+i//3} | frame {1+index*2}',font=small,fill=(255,225,120))
        sheet.save(OUT/'Walk-ContactSheet.png')
html='''<!doctype html><meta charset="utf-8"><title>D121 citizen walk review</title>
<style>body{background:#141e2c;color:#ebf3fc;font:18px Segoe UI;margin:32px auto;max-width:1100px}img{max-width:100%;border-radius:8px}p{color:#adc4d8}section{margin:32px 0}a{color:#9bceff}</style>
<h1>Citizen walk cycle — D121 v2</h1><p>Revised Blender source preview, awaiting human review. Hands follow the forearms, shoulders twist against the hips and the torso leans slightly forward. The v1 stride is retained.</p>
<p>One 28-frame cycle at 24fps. Three outfits and both mesh detail levels. The travelling view adds a constant-speed carrier matching the planted foot speed; its end resets after three cycles. <a href="../../Preview/Review.html">Original v1 preview</a> remains available for comparison.</p>'''
for name,title in titles.items(): html+=f'<section><h2>{title}</h2><img src="{name}.gif" alt="{title}"></section>'
html+='<section><h2>Side contact poses</h2><img src="Walk-ContactSheet.png" alt="Six walk poses"></section>'
html+='<p>Review foot contact, silhouette, arm swing, cadence and style. This source review does not establish game playback or device appearance. Integration is planned separately after an exact source approval.</p>'
(OUT/'Review.html').write_text(html,encoding='utf-8')
(OUT/'Media.json').write_text(json.dumps(dict(task='D121',version=2,source_sha256=doc['source_sha256'],media=media),indent=2)+'\n')
print('CITYSIM_D121_MEDIA_PASS 4 GIFs; six-pose contact sheet; Review.html')
