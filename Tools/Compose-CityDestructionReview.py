"""Verify recorded collapse/bakes and compose an offline art-study gallery."""
from pathlib import Path
import hashlib,json,ast
from PIL import Image,ImageDraw,ImageFont
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D104/DestructionStudy'
def digest(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    fragments=json.loads((OUT/'Fragments.json').read_text());sim=json.loads((OUT/'Simulation.json').read_text());baked=json.loads((OUT/'Baked.json').read_text());renders=json.loads((OUT/'ReviewRenders.json').read_text())
    assert len(fragments['fragments'])==84 and len(sim['snapshots'])==4 and len(baked['meshes'])==3 and len(renders['views'])==5
    assert len(sim['frames'])>50 and not any(r['awake'] for r in sim['snapshots'][-1]['transforms'])
    initial={r['name']:r for r in sim['snapshots'][0]['transforms']};final={r['name']:r for r in sim['snapshots'][-1]['transforms']}
    moved=sum(sum((a-b)**2 for a,b in zip(initial[k]['translation_cm'],final[k]['translation_cm']))>100 for k in initial);assert moved>20
    for e in baked['meshes']:assert digest(ROOT/e['export'])==e['sha256'] and e['triangles']==1560
    font=ImageFont.truetype('C:/Windows/Fonts/segoeui.ttf',20);sheet=Image.new('RGB',(1500,1080),'#16202a');draw=ImageDraw.Draw(sheet);cards=[]
    for i,r in enumerate(renders['views']):
        p=OUT/r['image'];assert digest(p)==r['sha256']
        with Image.open(p) as im:assert im.size==(1000,1000);sheet.paste(im.convert('RGB').resize((500,500)),(i%3*500,i//3*540+40))
        draw.text((i%3*500+10,i//3*540+8),r['name'].replace('SM_Study_Home_',''),font=font,fill='white');cards.append(f'<figure><a href="{r["image"]}"><img src="{r["image"]}"></a><figcaption>{r["name"].replace("SM_Study_Home_","")}</figcaption></figure>')
    sheet.save(OUT/'StaticBakeComparison.png')
    page='''<!doctype html><html><meta charset="utf-8"><title>D104 offline Chaos destruction</title><style>body{background:#16202a;color:#eef4fa;font:17px system-ui;margin:28px auto;max-width:1450px}.grid{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}figure{margin:0 0 24px}img{width:100%}a{color:#8bd4ff}</style><h1>Offline Chaos collapse → static destroyed meshes</h1><p>A representative house is rebuilt as84 closed masonry/roof pieces with authored windows/doors/trim, interior faces and dedicated structural collision. Unreal's Chaos rigid bodies simulate a mild inward support failure, gravity and contact. This is pre-fractured geometry, not a live Geometry Collection strain/cluster implementation.</p><p>The three ordinary static meshes bake actual recorded poses at1.5s,4s and20s. At the final pose all bodies are asleep. Each has1560 triangles and existing charred materials, compared below at the same camera/scale with the668-triangle current stand-in. Production damage assets are retained.</p><div class="grid">'''+''.join(cards)+'''</div><h2>Actual physics captures</h2><div class="grid">'''+''.join(f'<figure><a href="{n}.png"><img src="{n}.png"></a><figcaption>{n}</figcaption></figure>' for n in ('Assembly','EarlyCollapse','Collapsed','Settled'))+'''</div><p><a href="CityDestructionBaked.blend">Editable baked source</a> · <a href="Baked.json">Bake contracts</a> · <a href="Simulation.json">Recorded poses/animation frames</a> · <a href="../DynamicReflections/Review.html">Shipped live-reflection update</a></p><p>User decision2026-10-14: retain the existing stand-ins and scripted damage solutions. Further Chaos work is deferred until buildings have more detailed interiors and support structure; possible future destruction would be prebaked to limit runtime cost. This prototype remains reference evidence. No destruction animation or replacement is integrated.</p></html>'''
    (OUT/'Review.html').write_text(page,encoding='utf-8')
    for name in ['Create-CityDestructionStudy.py','Run-CityDestructionStudy.py','Bake-CityDestructionStudy.py','Capture-CityDestructionBakes.py','Compose-CityDestructionReview.py']:ast.parse((ROOT/'Tools'/name).read_text())
    result=dict(task='D104',study='Offline Unreal Chaos collapse and static bakes',status='Prototype retained; further research deferred',current_scripted_damage_retained=True,fragments=84,closed_wall_thickness_m=.24,roof_thickness_m=.14,collision='Dedicated structural UCX hulls; visual details do not inflate physics hulls',snapshots=4,retained_animation_frames=len(sim['frames']),moved_pieces=moved,final_awake_pieces=0,static_meshes=3,triangles_each=1560,native_static_review_views=5,actual_physics_views=4,geometry_collection_runtime=False,production_damage_replaced=False,headset_observed=False,source_sha256=digest(OUT/'CityDestructionStudy.blend'),baked_source_sha256=digest(OUT/'CityDestructionBaked.blend'),bakes=baked['meshes'])
    (OUT/'Implementation.json').write_text(json.dumps(result,indent=2)+'\n');print('CITYSIM_DESTRUCTION_STUDY_PASS actual Chaos simulation; three static bakes; production stand-ins retained')
if __name__=='__main__':main()
