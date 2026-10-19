"""Current production asset/import/render scope proof; historical study evidence is retained."""
from pathlib import Path
import json,hashlib,html
from PIL import Image
import numpy as np
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D91/Integration'
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
doc=json.loads((ROOT/'ArtSource/Manifests/city_building_adoption.json').read_text())
proof=json.loads((OUT/'Import.json').read_text());captures=json.loads((OUT/'Unreal/Report.json').read_text())
assert len(doc['assets'])==len(proof['meshes'])==61 and len(captures['views'])==30
native={e['destination']:e for e in proof['meshes']};allowed=set()
for e in doc['assets']:
    assert digest(ROOT/e['export'])==e['export_sha256'];p=ROOT/'CitySimVR/Content'/e['mesh_destination'].removeprefix('/Game/');p=p.with_suffix('.uasset');assert p.is_file();allowed.add(p.relative_to(ROOT).as_posix())
    v=native[e['mesh_destination']];assert v['export_sha256']==e['export_sha256'] and v['triangles']==e['triangles'] and v['sections']==e['sections'] and set(v['materials'])==set(e['material_slots'].values()) and v['vertex_color_policy']=='REPLACE'
    production=json.loads((ROOT/'ArtSource/Manifests'/('city_seasonal_kit.json' if e.get('name')=='SM_Building_Residential_SeasonalSpace_A' else 'city_starter_kit.json')).read_text())
    selected=next(v for v in production['assets'] if v['mesh_destination']==e['mesh_destination']);assert selected==e
for p in proof['materials']['changed']:
    allowed.add(('CitySimVR/Content/'+p.removeprefix('/Game/')+'.uasset'))
before=json.loads((OUT/'Before/Content.json').read_text());unexpected=[p for p,h in before.items() if p not in allowed and (not (ROOT/p).exists() or digest(ROOT/p)!=h)]
assert not unexpected,unexpected
new={p.relative_to(ROOT).as_posix() for p in (ROOT/'CitySimVR/Content').rglob('*') if p.is_file()}-set(before)
assert new.issubset(allowed),new-allowed
build=(OUT/'EditorBuild.log').read_text(errors='replace');assert 'Result: Succeeded' in build
test=(OUT/'Tests-Engine.log').read_text(errors='replace');assert test.count('Result={Success}')==11 and 'Result={Fail}' not in test
frames=[];cards=[]
for v in captures['views']:
    p=OUT/'Unreal'/(v['name']+'.png');im=Image.open(p).convert('RGB');a=np.array(im)
    assert im.size==(1200,900) and np.std(a)>8 and len(np.unique(a.reshape(-1,3),axis=0))>1000,p
    assert abs(v['citizen_height_cm']-143.175)<.2 and abs(v['citizen_feet_z_cm']+20)<.2
    frames.append(dict(file=p.relative_to(ROOT).as_posix(),sha256=digest(p),size=list(im.size),view=v['name']))
    cards.append('<figure><a href="Unreal/'+v['name']+'.png"><img loading="lazy" src="Unreal/'+v['name']+'.png"></a><figcaption>'+html.escape(v['name'])+'</figcaption></figure>')
for group,selection in [('Day-Review',[v for v in captures['views'] if v['lighting']=='Day' and v['framing']=='Whole' and v['lod']==0]),('Night-Review',[v for v in captures['views'] if v['lighting']=='Night']),('Home-Details',[v for v in captures['views'] if v['framing']!='Whole'])]:
    width=1500;tw=500;th=400;sheet=Image.new('RGB',(width,((len(selection)+2)//3)*th),(24,30,36))
    from PIL import ImageDraw
    draw=ImageDraw.Draw(sheet)
    for i,v in enumerate(selection):
        im=Image.open(OUT/'Unreal'/(v['name']+'.png')).convert('RGB');im.thumbnail((tw,375));x=(i%3)*tw;y=(i//3)*th;sheet.paste(im,(x,y));draw.text((x+8,y+378),v['name'].replace('SM_Building_','')[:65],fill='white')
    sheet.save(OUT/(group+'.png'))
(OUT/'Review.html').write_text('<!doctype html><meta charset="utf-8"><title>D91 Unreal building adoption</title><style>body{background:#182029;color:#eef2f5;font:16px system-ui;margin:24px}main{display:grid;grid-template-columns:repeat(auto-fit,minmax(420px,1fr));gap:16px}figure{margin:0}img{width:100%}figcaption{padding:8px;overflow-wrap:anywhere}</style><h1>D91 — approved buildings in Unreal</h1><p>61 production meshes, shared opaque glass and civic asphalt/tinted brick. Actual Windows D3D11 forward captures; fixed lighting and exposure. Mobile pavilion LOD and seasonal wreath included. Physical headset appearance remains unconfirmed.</p><main>'+''.join(cards)+'</main>')
result=dict(task='D91',date='2026-10-13',status='Production adoption verified',meshes=61,starter_meshes=60,seasonal_meshes=1,native_import=proof,focused_native_tests=11,editor_build='WarningsAsErrors succeeded',captures=frames,old_native_files=len(before),protected_old_native_files=len(before)-len(allowed&set(before)),unrelated_native_preserved=True,new_native_files=sorted(new),changed_native_files={p:digest(ROOT/p) for p in sorted(allowed)},physical_device_verified=False,packages='Recorded separately; no device installation/publication',adjacent_staffing_failure=dict(test='CitySim.Health.HospitalStaffingUtilitiesUpgradeAndSave',observed=[6,28,1],expected=[10,48,2],evidence='Tests-First.log',owner_notified='D89',simulation_behavior_changed=False),vertex_reimport_repair='Persisted legacy FBX vertex policy explicitly replaced; first rejected captures retained')
(OUT/'Implementation.json').write_text(json.dumps(result,indent=2)+'\n')
print('CITYSIM_D91_BUILDING_ADOPTION_PASS 61 assets;30 actual native captures;11 focused tests;unrelated content preserved;adjacent staffing failure recorded')
