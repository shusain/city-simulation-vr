"""Review links for shipped live reflections, hollow flues and offline destruction."""
from pathlib import Path
import json,hashlib
from PIL import Image,ImageDraw,ImageFont
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D104/DynamicReflections'
delivery=json.loads((OUT/'Implementation.json').read_text());proof=OUT/'Proof-PackagedFinal-User/Saved/Automation/DynamicReflections';font=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',20)
sheet=Image.new('RGB',(1200,440),'#16202a');d=ImageDraw.Draw(sheet);cards=[]
for i,(name,label) in enumerate([('LiveRedGreen','Live reflected cue'),('RedRemoved_Stale','Removed object / old capture'),('RedRemoved_Updated','After automatic refresh')]):
    p=proof/(name+'.png')
    with Image.open(p) as im:sheet.paste(im.convert('RGB').resize((400,400)),(400*i,40))
    d.text((400*i+8,8),label,font=font,fill='white');rel=p.relative_to(OUT).as_posix();cards.append(f'<figure><a href="{rel}"><img src="{rel}"></a><figcaption>{label}</figcaption></figure>')
sheet.save(OUT/'ReflectionProof.png')
game=ROOT/'Artifacts/D104/Integration/Desktop-DynamicFinal-User/Saved/Automation/LowDensity';gamecards=[]
for p in sorted(game.glob('*.png')):
    rel='../Integration/Desktop-DynamicFinal-User/Saved/Automation/LowDensity/'+p.name;gamecards.append(f'<figure><a href="{rel}"><img src="{rel}"></a><figcaption>{p.stem.replace("_"," ")}</figcaption></figure>')
cost=delivery['bounded_desktop_cost'];gpuoff=cost['off_gpu'];gpuon=cost['on_gpu']
text=f'''<!doctype html><html><meta charset="utf-8"><title>D104 live reflections</title><style>body{{background:#16202a;color:#eef4fa;font:17px system-ui;margin:28px auto;max-width:1450px}}h1,h2{{font-weight:550}}.grid{{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}}figure{{margin:0 0 20px}}img{{width:100%}}a{{color:#8bd4ff}}</style><h1>D104 — Windows live reflections and hollow originals</h1><p>Delivered {delivery['revision']}. Windows desktop/PCVR refresh one64px scene cubemap twice per second on the GPU. Quest retains the static fallback. Original factory flues are hollow with matched damage and unchanged outside dimensions.</p><p>The actual factory metal reflects a colored object hidden from the review camera. Removing it retains the old reflection until capture refresh, then the reflected red pixels fall from {delivery['proof']['before_red_pixels']} to {delivery['proof']['after_red_pixels']}.</p><div class="grid">{''.join(cards)}</div><h2>Bounded desktop check</h2><p>Twelve actual buildings, paused city, six measured seconds per condition: GPU mean {gpuoff['mean_ms']:.3f} → {gpuon['mean_ms']:.3f}ms; p99 {gpuoff['p99_ms']:.3f} → {gpuon['p99_ms']:.3f}ms. This workstation observation does not establish headset cost. Captures omit shadow recomputation; main-view shadows and diffuse light remain. One shared environment cube has approximate spatial correspondence and mip filtering; it is not a per-surface mirror.</p><p><a href="../DestructionStudy/Review.html">Offline Chaos collapse and static bakes</a> · <a href="Implementation.json">Delivery receipt</a></p><h2>Actual packaged game views</h2><div class="grid">{''.join(gamecards)}</div></html>'''
(OUT/'Review.html').write_text(text,encoding='utf-8');print('CITYSIM_D104_DYNAMIC_REVIEW_PASS')
