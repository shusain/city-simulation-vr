"""Focused source/import, native images and maintained-package delivery receipt."""
from pathlib import Path
import ast,csv,hashlib,json,zipfile,io
from PIL import Image,ImageChops
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D104/CylinderShading'
def digest(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    protected=json.loads((OUT/'Protected.json').read_text());assert all(digest(ROOT/p)==h for p,h in protected.items())
    assert not (ROOT/'CitySimVR/Content/D104UnsavedNormalsBefore').exists()
    doc=json.loads((ROOT/'ArtSource/Manifests/city_low_density_buildings.json').read_text());assert digest(ROOT/doc['source'])==doc['source_sha256']
    for e in doc['assets']:assert digest(ROOT/e['export'])==e['export_sha256']
    source=json.loads((OUT/'Source.json').read_text());native=json.loads((OUT/'Import.json').read_text())
    assert source['source_sha256']==native['source_sha256']==doc['source_sha256']
    assert len(source['meshes'])==len(native['meshes'])==8 and sum(len(r['cylinders']) for r in source['meshes'])==16
    assert sum(r['smoothed_faces'] for r in source['meshes'])==336
    assert all(r['minimum_normal_dot']>.999 for r in native['meshes'])
    before=json.loads((OUT/'BeforeChimneysFinal/Report.json').read_text());after=json.loads((OUT/'AfterChimneysFinal/Report.json').read_text())
    assert len(before['views'])==len(after['views'])==8
    for a,b in zip(before['views'],after['views']):
        assert a['name']==b['name'] and a['camera_cm']==b['camera_cm'] and a['triangles']==b['triangles'] and a['materials']==b['materials']
        pa=OUT/'BeforeChimneysFinal'/a['image'];pb=OUT/'AfterChimneysFinal'/b['image']
        assert digest(pa)==a['sha256'] and digest(pb)==b['sha256']
        with Image.open(pa) as ia,Image.open(pb) as ib:assert ia.size==ib.size==(1000,1000) and ImageChops.difference(ia.convert('RGB'),ib.convert('RGB')).getbbox()
    previous=json.loads((OUT/'PreviousDefault.json').read_text());assert len(previous)==53
    assert all(digest(OUT/'PreviousDefaultWindows'/r['path'])==r['sha256'] for r in previous)
    inputs=sorted([p for folder in ('Source','Content/CitySim') for p in (ROOT/'CitySimVR'/folder).rglob('*') if p.is_file() and p.suffix in ('.h','.cpp','.cs','.uasset','.umap')],key=str)
    h=hashlib.sha256()
    for p in inputs:
        raw=p.read_bytes();h.update((p.relative_to(ROOT/'CitySimVR').as_posix()+'\0'+str(len(raw))+'\0').encode());h.update(raw)
    revision='dev-'+h.hexdigest()[:20];assert revision in (ROOT/'CitySimVR/Intermediate/CityGameRevision/CityGameRevisionBuild.h').read_text()
    game=ROOT/'Artifacts/D104/Integration/Desktop-Cylinders-User/Saved/Automation/LowDensity'
    fixture=json.loads((game/'Fixture.json').read_text());assert fixture['lastSavedGameRevision']==revision
    log=(ROOT/'Artifacts/D104/Integration/Desktop-Cylinders-Engine.log').read_text(errors='replace');assert 'CITYSIM_D104_DESKTOP_COMPLETE' in log and 'Result={Success}' in log and 'Result={Fail}' not in log
    assert len(list(game.glob('*.png')))==17
    cooked=[]
    for platform in ('Windows','Quest'):
        rows={r['Filename'].strip():r for r in csv.DictReader((OUT/(platform+'-Container.csv')).open(encoding='utf-8-sig'),skipinitialspace=True)}
        for e in doc['assets']:
            if e['category']=='Factory':
                path='../../../CitySimVR/Content/'+e['mesh_destination'].removeprefix('/Game/')+'.uasset';assert int(rows[path]['Size'])>0;cooked.append(dict(platform=platform,path=path,hash=rows[path]['Hash'].strip()))
        assert 'BUILD SUCCESSFUL' in (OUT/('WindowsPackage-Final.log' if platform=='Windows' else 'QuestPackage.log')).read_text(errors='replace')
    exe=ROOT/'Artifacts/PCVRSandbox/Windows/CitySimVR/Binaries/Win64/CitySimVR.exe';binary=exe.read_bytes();assert any(revision.encode(e) in binary for e in ('utf-8','utf-16le','utf-32le'))
    apk=OUT/'QuestPackage/Android_ASTC/CitySimVR-arm64.apk';embedded=[]
    with zipfile.ZipFile(apk) as z:
        binary=z.read('lib/arm64-v8a/libUnreal.so');assert any(revision.encode(e) in binary for e in ('utf-8','utf-16le','utf-32le'))
        with zipfile.ZipFile(io.BytesIO(z.read('assets/main.obb.png'))) as obb:
            for name in ('CitySimVR-Android_ASTC.pak','CitySimVR-Android_ASTC.utoc','CitySimVR-Android_ASTC.ucas'):
                raw=obb.read('CitySimVR/Content/Paks/'+name);sha=hashlib.sha256(raw).hexdigest();assert sha==digest(ROOT/'CitySimVR/Saved/StagedBuilds/Android_ASTC/CitySimVR/Content/Paks'/name);embedded.append(dict(name=name,sha256=sha))
    assert 'Quest APK verified:' in (OUT/'QuestValidation.log').read_text()
    for name in ['CityIndustrialCylinderNormals.py','Update-CityIndustrialCylinderNormals.py','Import-CityIndustrialCylinderNormals.py','Compose-CityIndustrialCylinderReview.py','Verify-CityIndustrialCylinderNormals.py']:ast.parse((ROOT/'Tools'/name).read_text())
    result=dict(task='D104',slice='Industrial cylinder smooth normals',status='Delivered',revision=revision,save_format=67,source_sha256=doc['source_sha256'],meshes=8,cylinders_including_damage=16,smoothed_side_faces=336,native_normal_readback=True,geometry_uv_rgba_materials_unchanged=True,protected_inputs=len(protected),previous_default_files=53,native_comparison_images=16,packaged_game_images=17,cooked=cooked,windows_sha256=digest(exe),quest_sha256=digest(apk),quest_embedded=embedded,reflection_policy='Specified environment cubemap retained; no runtime city recapture',user_accepts_prior_integrated_buildings=True,headset_rendering_confirmed=False,device_installed=False,published=False)
    (OUT/'Implementation.json').write_text(json.dumps(result,indent=2)+'\n');print('CITYSIM_D104_CYLINDER_DELIVERY_PASS '+revision)
if __name__=='__main__':main()
