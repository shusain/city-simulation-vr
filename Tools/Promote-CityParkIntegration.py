"""Guarded metadata adoption after native D83 import; original source entries retained."""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from Assets.Manifests.CityAssetCapabilities import set_capability
import json,copy,hashlib,os
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Artifacts/D83/Integration'
doc=json.loads((ROOT/'ArtSource/Manifests/city_park_integration.json').read_text())
proof=json.loads((OUT/'Import.json').read_text())
assert not proof['errors'] and len(proof['assets'])==43
changes=[]
for name in ('city_starter_kit.json','city_seasonal_kit.json'):
    path=ROOT/'ArtSource/Manifests'/name;original=path.read_bytes()
    before=OUT/('Before-'+name)
    if not before.exists():before.write_bytes(original)
    data=json.loads(original)
    for entry in data['assets']:
        match=next((e for e in doc['statics'] if e['asset_id']==entry['asset_id']),None)
        if not match:continue
        if not entry.get('park_refinement_source_entry'):entry['park_refinement_source_entry']=copy.deepcopy(entry)
        lod=match['lods'][0];low=match['lods'][1]
        entry.update(source=match['source'],export=lod['export'],triangles=lod['triangles'],sections=len(match['materials']),
            dimensions_m=[round(b-a,4) for a,b in zip(*lod['bounds_m'])],material_slots=match['materials'],
            park_refinement_task='D83',building_refinement_task='D83',native_lod_export=low['export'],
            native_lod_triangles=low['triangles'],lod_policy='authored paired LOD; Desktop .025 threshold; Android minimum LOD1',
            provenance='Accepted D83 source derivative; Prepare-CityParkIntegration.py; prior source entry retained')
        set_capability(entry, 'paired_park_lods')
        set_capability(entry, 'building_refinement', False)
        entry.pop('native_export_source',None)
        changes.append(entry['asset_id'])
    pending=path.with_suffix('.D83-pending');pending.write_text(json.dumps(data,indent=2)+'\n',encoding='utf-8');os.replace(pending,path)
assert len(changes)==18
doc['native_selected']=True
(ROOT/'ArtSource/Manifests/city_park_integration.json').write_text(json.dumps(doc,indent=2)+'\n')
(OUT/'Promotion.json').write_text(json.dumps(dict(assets=changes,original_entries_retained=True,
    canonical_blends_exports_unchanged=True),indent=2)+'\n')
print('CITYSIM_D83_MANIFEST_PROMOTED 18')
