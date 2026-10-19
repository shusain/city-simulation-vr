"""Before/after native chimney images with full-resolution review links."""
from pathlib import Path
import json,html
from PIL import Image,ImageDraw,ImageFont
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D104/CylinderShading'
font=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',22)
reports={label:json.loads((OUT/(label+'ChimneysFinal')/'Report.json').read_text()) for label in ('Before','After')}
maps={label:{r['name']:r for r in report['views']} for label,report in reports.items()}
assert all(len(r['views'])==8 for r in reports.values())
cards=[]
for damaged in (False,True):
    sheet=Image.new('RGB',(1000,2160),'#16202a');draw=ImageDraw.Draw(sheet)
    for row,letter in enumerate('ABCD'):
        name='SM_LowDensity_Factory_'+letter+('_Damaged' if damaged else '')
        links=[]
        for col,label in enumerate(('Before','After')):
            r=maps[label][name];p=OUT/(label+'ChimneysFinal')/r['image']
            with Image.open(p) as im:assert im.size==(1000,1000);sheet.paste(im.convert('RGB').resize((500,500)),(col*500,row*540+40))
            draw.text((col*500+12,row*540+5),f'Factory {letter} — {label}',font=font,fill='white')
            rel=p.relative_to(OUT).as_posix();links.append(f'<a href="{rel}"><img src="{rel}" alt="{name} {label}"></a>')
        cards.append(f'<section><h2>Factory {letter}{" — damaged" if damaged else ""}</h2><div class="pair">'+''.join(links)+'</div></section>')
    sheet.save(OUT/('DamagedComparison.png' if damaged else 'HealthyComparison.png'))
page='''<!doctype html><html><meta charset="utf-8"><title>D104 industrial cylinders</title><style>body{background:#16202a;color:#eef4fa;font:17px system-ui;margin:28px auto;max-width:1450px}h1,h2{font-weight:550}.pair{display:grid;grid-template-columns:1fr 1fr;gap:10px}.pair img{width:100%}a{color:#8bd4ff}section{margin:30px 0}</style><h1>D104 industrial cylinders — before / after</h1><p>Left: original flat chimney normals. Right: smooth curved-wall normals; hard caps/rims retained. Geometry, materials, roof flashing and building scale are unchanged. Click a view for the full image.</p>'''+''.join(cards)+'</html>'
(OUT/'Review.html').write_text(page,encoding='utf-8')
print('CITYSIM_D104_CYLINDER_REVIEW_PASS sixteen native comparison views')
