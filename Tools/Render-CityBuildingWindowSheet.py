"""Compose Blender-first D91 review; all images are actual model renders."""
from pathlib import Path
from PIL import Image,ImageDraw,ImageFont,ImageStat
import json,hashlib,html
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D91/Buildings'
doc=json.loads((ROOT/'ArtSource/Manifests/city_building_window_study.json').read_text())
font=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',27);small=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',19)
labels={'building.residential.a':'Low-density home','building.residential.medium.a':'Residential medium','building.residential.high.a':'Residential high','building.shop.a':'Commercial low','building.shop.medium.a':'Commercial medium','building.shop.high.a':'Commercial high','building.factory.a':'Industrial low','building.factory.medium.a':'Industrial medium','building.factory.high.a':'Industrial high','building.school.a':'Public school','building.primaryschool.a':'Primary school','building.secondaryschool.a':'Secondary school','building.college.a':'College','building.hospital.a':'Hospital','building.hospital.medium.a':'Hospital upgraded','building.residential.a.damaged':'Damaged home - scale alignment'}
def label(asset):
    if asset in labels:return labels[asset]
    words=asset.removeprefix('building.').removesuffix('.a').split('.')
    return ' '.join(words).replace('wastedepot','Waste depot').replace('recycling','Recycling').capitalize()
files={(e['asset_id'],e['state'],e['lighting']):ROOT/e['file'] for e in doc['views']}
assert len(files)==44
verified=[]
for key,path in files.items():
    with Image.open(path) as im:
        assert im.size==(1200,900) and any(b-a>30 for a,b in ImageStat.Stat(im.convert('RGB')).extrema)
    verified.append(dict(file=path.relative_to(ROOT).as_posix(),size=[1200,900],sha256=hashlib.sha256(path.read_bytes()).hexdigest()))
def sheet(name,title,rows,columns):
    canvas=Image.new('RGB',(620*len(columns),125+490*len(rows)),(24,30,40));draw=ImageDraw.Draw(canvas)
    draw.text((22,14),title,font=font,fill='white')
    for c,(state,light) in enumerate(columns):draw.text((c*620+20,54),state+' | '+light,font=small,fill=(205,221,235))
    for row,asset in enumerate(rows):
        y=86+row*490
        for c,(state,light) in enumerate(columns):
            with Image.open(files[(asset,state,light)]) as im:canvas.paste(im.convert('RGB').resize((600,450),Image.Resampling.LANCZOS),(c*620+10,y+24))
            draw.text((c*620+20,y),label(asset),font=small,fill='white')
    draw.text((22,canvas.height-30),'Blender appearance candidates. Citizen 1.43 m; each building fits its camera. Unreal adoption awaits review.',font=small,fill=(174,190,208))
    canvas.save(OUT/name)
sheet('RCI-Day-Comparison.png','D91 | Opaque glass + framed windows | Blender', ['building.residential.high.a','building.shop.medium.a','building.factory.high.a'],[('Before','Day'),('After','Day')])
sheet('Civic-Day-Comparison.png','D91 | Civic windows | Blender',['building.police.high.a','building.school.a','building.hospital.medium.a','building.recycling.medium.a'],[('Before','Day'),('After','Day')])
sheet('Damaged-Scale-Comparison.png','D91 | Damaged house at the accepted home scale',['building.residential.a.damaged'],[('Before','Day'),('After','Day')])
night=[e['asset_id'] for e in doc['views'] if e['lighting']=='Night' and not e['asset_id'].endswith('.damaged')]
sheet('Night-Review.png','D91 | Window glow stays on glass panes | Blender',night,[('After','Day'),('After','Night')])
# A portable gallery provides every intact form, representative before/after
# views and the damage scale pair without requiring an Unreal startup.
cards=[]
for asset in dict.fromkeys(e['asset_id'] for e in doc['views']):
    images=[]
    for state,light in (('After','Day'),('Before','Day'),('After','Night')):
        path=files.get((asset,state,light))
        if path:images.append(f'<figure><figcaption>{state} · {light}</figcaption><a href="{html.escape(path.relative_to(OUT).as_posix())}"><img loading="lazy" src="{html.escape(path.relative_to(OUT).as_posix())}"></a></figure>')
    cards.append(f'<section><h2>{html.escape(label(asset))}</h2><div class="views">'+''.join(images)+'</div></section>')
body='<!doctype html><html><meta charset="utf-8"><title>D91 Building windows</title><style>body{background:#18202b;color:#e8edf4;font:16px system-ui;margin:24px}h1{font-size:26px}h2{font-size:20px}.views{display:flex;flex-wrap:wrap;gap:12px}figure{margin:0;flex:1;min-width:280px;max-width:640px}img{width:100%;height:auto}figcaption{margin:8px 0;color:#bdd2e2}section{border-top:1px solid #445060;margin-top:24px;padding-top:8px}p{max-width:900px;line-height:1.5}a{color:#a6d4fb}</style><h1>D91 · Opaque glass and framed building windows</h1><p>Blender review candidates for 50 intact assets, including 24 storefront variants. Each unique form is shown below; storefronts share its window treatment. A real street citizen appears at 1.43 m. Only the low-density house and its damaged partner change scale. Production assets and all other damaged models are preserved. Unreal work follows appearance approval.</p>'+''.join(cards)+'</html>'
(OUT/'Review.html').write_text(body,encoding='utf-8')
(OUT/'CaptureVerification.json').write_text(json.dumps(dict(task='D91',captures=verified,verified_dimensions=True,blender_only=True),indent=2)+'\n')
print('CITYSIM_D91_BUILDING_WINDOW_SHEETS_PASS',len(verified),'Blender captures')
