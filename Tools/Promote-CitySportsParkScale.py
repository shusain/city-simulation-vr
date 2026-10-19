"""Adopt verified cosmetic scale assets/prop frames in the main authoring manifests."""
from pathlib import Path
import json,copy,hashlib
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D88/Scale'
path=ROOT/'ArtSource/Manifests/city_citizen_sports_scale.json';doc=json.loads(path.read_text())
proof=json.loads((OUT/'Import.json').read_text())
assert not proof['errors'] and len(proof['assets'])==4 and proof['source_sha256']==doc['source_sha256']
for e in doc['statics']+doc['props']:
    assert hashlib.sha256((ROOT/e['export']).read_bytes()).hexdigest()==e['sha256']
    assert next(v['export_sha256'] for v in proof['assets'] if v['asset']==e['mesh_destination'])==e['sha256']
for filename in ('city_starter_kit.json','city_park_integration.json'):
    target=ROOT/'ArtSource/Manifests'/filename;data=json.loads(target.read_text())
    before=OUT/('Before-'+filename)
    if not before.exists():before.write_bytes(target.read_bytes())
    if filename=='city_starter_kit.json':
        for e in doc['statics']:
            entry=next(v for v in data['assets'] if v['asset_id']==e['asset_id'])
            if 'sports_scale_prior_entry' not in entry:entry['sports_scale_prior_entry']=copy.deepcopy(entry)
            entry.update(source=doc['source'],export=e['export'],dimensions_m=[round(b-a,4) for a,b in zip(*e['bounds_m'])],
                         triangles=e['triangles'],sports_scale_task='D88',provenance='Citizen-relative D88 scale derivative; prior source retained')
            entry.pop('native_export_source',None)
    else:
        for e in doc['props']:
            entry=next(v for v in data['props'] if v['kind']==e['kind'])
            if 'sports_scale_prior_entry' not in entry:entry['sports_scale_prior_entry']=copy.deepcopy(entry)
            entry.update({k:v for k,v in e.items() if k!='mesh_destination'})
        for e in doc['clips']:
            entry=next(v for v in data['clips'] if v['name']==e['name'])
            if 'sports_scale_prior_frames' not in entry:entry['sports_scale_prior_frames']=copy.deepcopy(entry['frames'])
            assert all(a['body']==b['body'] for a,b in zip(entry['frames'],e['frames'])),'Approved body carrier changed'
            entry['frames']=e['frames'];entry['prop_scale_source']=doc['source'];entry['prop_scale_task']='D88'
        data['sports_scale_statics']=doc['statics'];data['sports_scale_source']=doc['source']
    target.write_text(json.dumps(data,indent=2)+'\n')
doc['native_selected']=True;path.write_text(json.dumps(doc,indent=2)+'\n')
(OUT/'Promotion.json').write_text(json.dumps(dict(task='D88-Scale',statics=2,props=2,clip_prop_rows=5,approved_clip_assets_unchanged=True,prior_entries_retained=True),indent=2)+'\n')
print('CITYSIM_D88_SCALE_PROMOTION_PASS 2 main fields;2 balls;5 synchronized prop clips')
