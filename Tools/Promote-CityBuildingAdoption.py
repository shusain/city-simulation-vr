"""Select exactly the approved D91 exports, retaining prior authoring entries."""
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parent))
from Assets.Manifests.CityAssetCapabilities import set_capability
import json,copy,hashlib,shutil
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D91/Integration';OUT.mkdir(parents=True,exist_ok=True)
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
windows=json.loads((ROOT/'ArtSource/Manifests/city_building_window_study.json').read_text())
surfaces=json.loads((ROOT/'ArtSource/Manifests/city_civic_surface_study.json').read_text())
lods={e['asset_id']:e for e in json.loads((OUT/'LODs.json').read_text())};assert len(lods)==4
for lod in lods.values():assert lod['roundtrip_pass'] and digest(ROOT/lod['export'])==lod['sha256']
for doc in (windows,surfaces):
    assert digest(ROOT/doc['source'])==doc['source_sha256']
    for e in doc['assets']:assert e['roundtrip_pass'] and digest(ROOT/e['export'])==e['sha256']
combined={e['mesh_destination']:dict(e,manifest=e.get('manifest','city_starter_kit.json')) for e in windows['assets']}
combined.update({e['mesh_destination']:dict(e,manifest='city_starter_kit.json') for e in surfaces['assets']})
assert len(combined)==61
baseline=OUT/'Before/Content.json'
if not baseline.exists():
    baseline.parent.mkdir(parents=True,exist_ok=True)
    baseline.write_text(json.dumps({p.relative_to(ROOT).as_posix():digest(p) for p in (ROOT/'CitySimVR/Content').rglob('*') if p.is_file()},indent=2)+'\n')
records=[]
for filename in ('city_starter_kit.json','city_seasonal_kit.json'):
    path=ROOT/'ArtSource/Manifests'/filename;doc=json.loads(path.read_text())
    backup=OUT/'Before'/filename
    if not backup.exists():shutil.copy2(path,backup)
    for entry in doc['assets']:
        candidate=combined.get(entry['mesh_destination'])
        if not candidate:continue
        assert candidate['manifest']==filename
        if 'building_adoption_prior_entry' not in entry:entry['building_adoption_prior_entry']=copy.deepcopy(entry)
        previous=entry['building_adoption_prior_entry']
        entry.update({k:copy.deepcopy(candidate[k]) for k in ('source','export','material_slots','triangles','sections','dimensions_m')})
        entry.update(building_adoption_task='D91',export_sha256=candidate['sha256'],provenance='User-approved D91 opaque windows/home scale/detail and civic asphalt/tinted brick; prior authoring entry retained')
        set_capability(entry, 'reviewed_building')
        if 'bounds_min_m' in candidate:
            entry.update(bounds_min_m=candidate['bounds_min_m'],bounds_max_m=candidate['bounds_max_m'])
        if candidate.get('home_scale_adopted'):
            entry.update(pivot_policy='body ground center; entrance walk extends20cm below pivot to sit on lawn',body_xy_scale=.66,door_front_authored_m=[0,-4.72,.925],wreath_attachment_authored_m=[0,-4.78,.85],wreath_scale=.5)
        if filename=='city_seasonal_kit.json':entry['triangle_limit']=candidate['triangles']
        if entry['asset_id'] in lods:
            lod=lods[entry['asset_id']]
            entry.update(native_lod_export=lod['export'],native_lod_triangles=lod['triangles'],native_lod_export_sha256=lod['sha256'])
        native=ROOT/'CitySimVR/Content'/entry['mesh_destination'].removeprefix('/Game/');native=native.with_suffix('.uasset')
        native_backup=OUT/'Before/Content'/native.relative_to(ROOT/'CitySimVR/Content')
        if native.exists() and not native_backup.exists():native_backup.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(native,native_backup)
        records.append(copy.deepcopy(entry))
    doc['building_adoption_task']='D91';path.write_text(json.dumps(doc,indent=2)+'\n')
assert len(records)==61 and sum(e.get('damaged_variant',False) for e in records)==1
manifest=dict(task='D91',approved_date='2026-10-13',assets=records,textures=surfaces['textures'],glass_texture='ArtSource/Textures/HomeWindowStudy/HomeGlass.png',glass_sha256=digest(ROOT/'ArtSource/Textures/HomeWindowStudy/HomeGlass.png'),source_manifests=['city_building_window_study.json','city_civic_surface_study.json'],native_imported=False)
path=ROOT/'ArtSource/Manifests/city_building_adoption.json';path.write_text(json.dumps(manifest,indent=2)+'\n')
(OUT/'Promotion.json').write_text(json.dumps(dict(task='D91',entries=61,starter_entries=60,seasonal_entries=1,paired_lods=list(lods.values()),manifest_sha256=digest(path),prior_entries_retained=True),indent=2)+'\n')
print('CITYSIM_D91_BUILDING_PROMOTION_PASS 60 starter +1 seasonal;61 exact exports;paired pavilion LOD')
