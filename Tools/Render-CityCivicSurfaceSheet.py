"""Compose actual D91 civic asphalt/brick captures and portable review."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont,ImageStat
import json,html,hashlib
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D91/CivicSurfaces'
doc=json.loads((ROOT/'ArtSource/Manifests/city_civic_surface_study.json').read_text())
font=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',27);small=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',19)
def label(asset):
    name=asset.removeprefix('building.').removesuffix('.a').replace('power.','Power facility · ').replace('service.','Water treatment · ').replace('wastedepot','Waste depot').replace('recycling','Recycling').replace('coalpower','Coal power').replace('oilpower','Oil power').replace('nuclearpower','Nuclear power').replace('primaryschool','Primary school').replace('secondaryschool','Secondary school').replace('.',' · ')
    return name[0].upper()+name[1:]
files={(e['asset_id'],e['state'],e['lighting'],e['framing']):ROOT/e['file'] for e in doc['views']}
assert len(files)==39
verified=[]
for key,path in files.items():
    with Image.open(path) as im:assert im.size==(1200,900) and any(b-a>30 for a,b in ImageStat.Stat(im.convert('RGB')).extrema)
    verified.append(dict(file=path.relative_to(ROOT).as_posix(),size=[1200,900],sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
def sheet(name,title,rows,columns):
    canvas=Image.new('RGB',(620*len(columns),125+490*len(rows)),(24,30,40));draw=ImageDraw.Draw(canvas);draw.text((22,14),title,font=font,fill='white')
    for c,(state,light,framing) in enumerate(columns):draw.text((c*620+20,54),('Accepted windows / plain surfaces' if state=='Before' else 'Brick + asphalt')+' | '+light,font=small,fill=(205,221,235))
    for row,asset in enumerate(rows):
        y=86+row*490
        for c,(state,light,framing) in enumerate(columns):
            with Image.open(files[(asset,state,light,framing)]) as im:canvas.paste(im.convert('RGB').resize((600,450),Image.Resampling.LANCZOS),(c*620+10,y+24))
            draw.text((c*620+20,y),label(asset),font=small,fill='white')
    draw.text((22,canvas.height-30),'Blender candidates; matched before/after cameras. Geometry, accepted windows and original wall tints retained.',font=small,fill=(174,190,208));canvas.save(OUT/name)
columns=[('Before','Day','Whole'),('After','Day','Whole')]
sheet('Civic-Day-Comparison.png','D91 | Civic brick walls + asphalt roofs',['building.police.high.a','building.school.a','building.hospital.medium.a','building.recycling.medium.a'],columns)
sheet('Utility-Day-Comparison.png','D91 | Utility finishes at their current colors',['building.power.high.a','building.service.high.a','building.coalpower.a'],columns)
sheet('Surface-Details.png','D91 | Asphalt grain and physical brick scale',['building.police.high.a','building.power.high.a'],[('After','Day','Detail')])
sheet('Night-Review.png','D91 | Approved window glow with new finishes',['building.police.high.a','building.school.a','building.hospital.medium.a'],[('After','Day','Whole'),('After','Night','Whole')])
cards=[]
for asset in dict.fromkeys(e['asset_id'] for e in doc['views']):
    figures=[]
    for state,light,framing in (('After','Day','Whole'),('Before','Day','Whole'),('After','Night','Whole'),('After','Day','Detail')):
        path=files.get((asset,state,light,framing))
        if path:
            url=html.escape(path.relative_to(OUT).as_posix());figures.append(f'<figure><figcaption>{state} · {light} · {framing}</figcaption><a href="{url}"><img loading="lazy" src="{url}"></a></figure>')
    cards.append(f'<section><h2>{html.escape(label(asset))}</h2><div class="views">'+''.join(figures)+'</div></section>')
body='<!doctype html><html><meta charset="utf-8"><title>D91 Civic surfaces</title><style>body{background:#18202b;color:#e8edf4;font:16px system-ui;margin:24px}h1{font-size:26px}h2{font-size:20px}.views{display:flex;flex-wrap:wrap;gap:12px}figure{margin:0;flex:1;min-width:280px;max-width:640px}img{width:100%;height:auto}figcaption{margin:8px 0;color:#bdd2e2}section{border-top:1px solid #445060;margin-top:24px;padding-top:8px}p{max-width:940px;line-height:1.5}a{color:#a6d4fb}</style><h1>D91 · Civic and utility building finishes</h1><p>26 Blender candidates add grainy asphalt to flat roofs and brick to building enclosures, keeping their existing wall colors. Tanks, equipment, vehicles, signs, geometry and the approved windows remain intact. Brick uses the same physical scale as the zoned buildings. A real 1.43 m citizen supplies the ruler. Before/after pairs use matching cameras. These are appearance candidates; production assets remain unchanged.</p><p><a href="../Buildings/Review.html">Accepted window and damaged-home review</a></p>'+''.join(cards)+'</html>'
(OUT/'Review.html').write_text(body,encoding='utf-8');(OUT/'CaptureVerification.json').write_text(json.dumps(dict(task='D91',captures=verified,verified_dimensions=True,blender_only=True),indent=2)+'\n')
print('CITYSIM_D91_CIVIC_SURFACE_SHEETS_PASS 39 captures')
