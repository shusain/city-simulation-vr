"""Same-camera factory metal comparisons and actual packaged game gallery."""
from pathlib import Path
import json,hashlib
from PIL import Image,ImageDraw,ImageFont
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D104/IndustrialMetal'
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
reports={phase:json.loads((OUT/phase/'Report.json').read_text(encoding='utf-8')) for phase in ('Before','After')}
before={r['image']:r for r in reports['Before']['views']};after={r['image']:r for r in reports['After']['views']};assert before.keys()==after.keys() and len(after)==10
sheet=Image.new('RGB',(1200,4*645),'#16202a');draw=ImageDraw.Draw(sheet);font=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',22);cards=[]
for letter in 'ABCD':
 filename='SM_LowDensity_Factory_'+letter+'_Day_Detail.png';i='ABCD'.index(letter)
 for col,phase in enumerate(('Before','After')):
  r=reports[phase]['views'];p=OUT/phase/filename;record=(before if phase=='Before' else after)[filename]
  assert digest(p)==record['sha256']
  with Image.open(p) as im:assert im.size==(1000,1000);sheet.paste(im.convert('RGB').resize((600,600)),(col*600,i*645+40))
  draw.text((col*600+12,i*645+9),f'{letter} — {phase}',font=font,fill='white')
  cards.append(f'<figure><a href="{phase}/{filename}"><img src="{phase}/{filename}"></a><figcaption>{letter} — {phase}</figcaption></figure>')
sheet.save(OUT/'DoorComparison.png');full=[]
for filename,r in after.items():
 assert r['camera_cm']==before[filename]['camera_cm'] and r['triangles']==before[filename]['triangles']
 for phase,record in [('Before',before[filename]),('After',r)]:assert digest(OUT/phase/filename)==record['sha256']
 if not r['detail']:full.append(f'<figure><a href="After/{filename}"><img src="After/{filename}"></a><figcaption>{filename.removesuffix(".png").replace("_"," ")}</figcaption></figure>')
game=ROOT/'Artifacts/D104/Integration/Desktop-IndustrialMetal-User/Saved/Automation/LowDensity';gamecards=[]
if game.exists():
 for p in sorted(game.glob('*.png')):
  rel='../Integration/Desktop-IndustrialMetal-User/Saved/Automation/LowDensity/'+p.name
  gamecards.append(f'<figure><a href="{rel}"><img src="{rel}"></a><figcaption>{p.stem.replace("_"," ")}</figcaption></figure>')
page='''<!doctype html><html><meta charset="utf-8"><title>D104 industrial steel</title><style>body{background:#16202a;color:#eef4fa;font:17px system-ui;margin:28px auto;max-width:1350px}.grid{display:grid;grid-template-columns:repeat(2,1fr);gap:12px}figure{margin:0 0 20px}img{width:100%}a{color:#8bd4ff}</style><h1>Industrial steel doors and exposed hardware</h1><p>Garage/roller/workshop door panels, frames, thresholds and personnel-door metal now use metallic1 / roughness0.22. Exposed factory mechanical parts and bollards use metallic1 / roughness0.18. Glass, painted walls, geometry, scale, UVs and smooth cylinder normals are retained. Existing charred destroyed meshes stay rough and use the existing scripted damage recipe.</p><p>These native comparisons use the same camera and fixed studio reflection environment. The actual Windows game uses the accepted twice-per-second local scene capture; Android retains the static fallback. A uniform area of the environment can still give a broad, quiet reflection on a flat door.</p><h2>Same-camera door comparisons</h2><div class="grid">'''+''.join(cards)+'''</div><h2>Updated buildings and retained damage</h2><div class="grid">'''+''.join(full)+'''</div><h2>Actual packaged game views</h2><div class="grid">'''+''.join(gamecards)+'''</div><p><a href="Implementation.json">Delivery receipt</a> · <a href="SourceVerification.json">Exact source preservation</a> · <a href="../DynamicReflections/Review.html">Accepted dynamic reflection proof</a></p><p>Further Chaos destruction research is deferred. The prototype stays retained for possible later work with authored interiors/support structure; no Chaos runtime or baked replacement is integrated.</p></html>'''
(OUT/'Review.html').write_text(page,encoding='utf-8');print('CITYSIM_INDUSTRIAL_METAL_REVIEW_PASS',len(after)*2,'native views;',len(gamecards),'game views')
