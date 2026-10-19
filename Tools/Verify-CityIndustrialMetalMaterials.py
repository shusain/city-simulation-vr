"""Verify focused industrial steel delivery and retained source/player baseline."""
from pathlib import Path
import json,hashlib,re,ast,csv,zipfile,io
from PIL import Image
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D104/IndustrialMetal'
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def main():
 protected=json.loads((OUT/'Protected.json').read_text(encoding='utf-8'));assert all(digest(ROOT/p)==h for p,h in protected.items())
 previous=json.loads((OUT/'PreviousDefault.json').read_text(encoding='utf-8'));assert len(previous)==53 and all(digest(OUT/'PreviousDefaultWindows'/r['path'])==r['sha256'] for r in previous)
 doc=json.loads((ROOT/'ArtSource/Manifests/city_low_density_buildings.json').read_text(encoding='utf-8'));assert digest(ROOT/doc['source'])==doc['source_sha256']
 for e in doc['assets']:assert digest(ROOT/e['export'])==e['export_sha256']
 source=json.loads((OUT/'SourceVerification.json').read_text(encoding='utf-8'));assert source['meshes']==24 and source['geometry_uv_rgba_custom_normals_exact']
 imports=json.loads((OUT/'Import.json').read_text(encoding='utf-8'));assert imports['source_sha256']==doc['source_sha256'] and len(imports['meshes'])==8 and len(imports['materials'])==2
 assert [r['roughness'] for r in imports['materials']]==[.22,.18] and all(r['metallic']==1 and r['hq_forward_reflections'] for r in imports['materials'])
 for r in imports['meshes']:
  assert r['sections']==6
  assert r['bindings']['IndustrialDoorMetal'].endswith('MI_CharredMechanical' if r['name'].endswith('_Damaged') else 'M_IndustrialDoorSteel')
 for phase in ('Before','After'):
  report=json.loads((OUT/phase/'Report.json').read_text(encoding='utf-8'));assert len(report['views'])==10
  for r in report['views']:
   p=OUT/phase/r['image'];assert digest(p)==r['sha256']
   with Image.open(p) as im:assert im.size==(1000,1000)
 native=(ROOT/'Artifacts/D104/Integration/Tests-IndustrialMetal-Engine.log').read_text(errors='replace');assert len(re.findall(r'Result=\{Success\}',native))==4 and 'Result={Fail}' not in native
 game=ROOT/'Artifacts/D104/Integration/Desktop-IndustrialMetal-User/Saved/Automation/LowDensity';assert len(list(game.glob('*.png')))==17
 log=(ROOT/'Artifacts/D104/Integration/Desktop-IndustrialMetal-Engine.log').read_text(errors='replace');assert 'Result={Success}' in log and 'Result={Fail}' not in log
 inputs=sorted([p for folder in ('Source','Content/CitySim') for p in (ROOT/'CitySimVR'/folder).rglob('*') if p.is_file() and p.suffix in ('.h','.cpp','.cs','.uasset','.umap')],key=str);h=hashlib.sha256()
 for p in inputs:
  raw=p.read_bytes();h.update((p.relative_to(ROOT/'CitySimVR').as_posix()+'\0'+str(len(raw))+'\0').encode());h.update(raw)
 revision='dev-'+h.hexdigest()[:20];assert revision in (ROOT/'CitySimVR/Intermediate/CityGameRevision/CityGameRevisionBuild.h').read_text()
 fixture=json.loads((game/'Fixture.json').read_text(encoding='utf-8'));assert fixture['lastSavedGameRevision']==revision
 cooked=[]
 for platform in ('Windows','Quest'):
  rows={r['Filename'].strip():r for r in csv.DictReader((OUT/(platform+'-Container.csv')).open(encoding='utf-8-sig'),skipinitialspace=True)}
  for folder,names in [('Meshes',[r['name'] for r in imports['meshes']]),('Materials',[r['native_material'].rsplit('/',1)[1] for r in imports['materials']])]:
   for name in names:
    path='../../../CitySimVR/Content/CitySim/Art/LowDensity/'+folder+'/'+name+'.uasset';assert int(rows[path]['Size'])>0;cooked.append(dict(platform=platform,path=path,hash=rows[path]['Hash'].strip()))
  assert not any('/CityStudies/D104Destruction/' in p for p in rows)
  assert 'BUILD SUCCESSFUL' in (OUT/(platform+'Package.log')).read_text(errors='replace')
 exe=ROOT/'Artifacts/PCVRSandbox/Windows/CitySimVR/Binaries/Win64/CitySimVR.exe';binary=exe.read_bytes();assert any(revision.encode(e) in binary for e in ('utf-8','utf-16le','utf-32le'))
 apk=OUT/'QuestPackage/Android_ASTC/CitySimVR-arm64.apk';embedded=[]
 with zipfile.ZipFile(apk) as z:
  binary=z.read('lib/arm64-v8a/libUnreal.so');assert any(revision.encode(e) in binary for e in ('utf-8','utf-16le','utf-32le'))
  with zipfile.ZipFile(io.BytesIO(z.read('assets/main.obb.png'))) as obb:
   for name in ('CitySimVR-Android_ASTC.pak','CitySimVR-Android_ASTC.utoc','CitySimVR-Android_ASTC.ucas'):
    raw=obb.read('CitySimVR/Content/Paks/'+name);sha=hashlib.sha256(raw).hexdigest();assert sha==digest(ROOT/'CitySimVR/Saved/StagedBuilds/Android_ASTC/CitySimVR/Content/Paks'/name);embedded.append(dict(name=name,sha256=sha))
 assert 'Quest APK verified:' in (OUT/'QuestValidation.log').read_text()
 for name in ['CityIndustrialMetalMaterials.py','Update-CityIndustrialMetalMaterials.py','Import-CityIndustrialMetalMaterials.py','Compose-CityIndustrialMetalReview.py','Verify-CityIndustrialMetalMaterials.py','Prepare-CityLowDensityIntegration.py','Import-CityLowDensityIntegration.py','Capture-CityLowDensityIntegration.py']:ast.parse((ROOT/'Tools'/name).read_text(encoding='utf-8'))
 result=dict(task='D104',slice='Industrial steel doors and hardware',status='Delivered',revision=revision,save_format=67,source_sha256=doc['source_sha256'],materials=imports['materials'],factory_meshes=8,geometry_uv_rgba_custom_normals_unchanged=True,native_views=20,focused_native_checks=4,packaged_game_views=17,protected_inputs=len(protected),previous_default_files=53,windows_sha256=digest(exe),quest_sha256=digest(apk),cooked=cooked,embedded=embedded,pc_dynamic_reflections_user_accepted=True,quest_static_fallback=True,headset_rendering_confirmed=False,chaos_research_deferred=True,current_scripted_damage_retained=True,device_installed=False,published=False)
 (OUT/'Implementation.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8');print('CITYSIM_INDUSTRIAL_METAL_DELIVERY_PASS',revision)
if __name__=='__main__':main()
