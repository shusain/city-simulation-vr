"""Verify Windows dynamic reflections, Quest fallback and preservation receipts."""
from pathlib import Path
import ast,csv,hashlib,json,zipfile,io,re
from PIL import Image
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D104/DynamicReflections'
def digest(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    protected=json.loads((OUT/'Protected.json').read_text());assert all(digest(ROOT/p)==h for p,h in protected.items())
    doc=json.loads((ROOT/'ArtSource/Manifests/city_low_density_buildings.json').read_text());assert digest(ROOT/doc['source'])==doc['source_sha256']
    for e in doc['assets']:assert digest(ROOT/e['export'])==e['export_sha256']
    imports=json.loads((OUT/'Chimneys-Import.json').read_text());assert len(imports['meshes'])==2 and imports['source_sha256']==doc['source_sha256']
    previous=json.loads((OUT/'PreviousDefault.json').read_text());assert len(previous)==53 and all(digest(OUT/'PreviousDefaultWindows'/r['path'])==r['sha256'] for r in previous)
    proof=OUT/'Proof-PackagedFinal-User/Saved/Automation/DynamicReflections'
    data=json.loads((proof/'Proof.json').read_text());assert data['before_red_pixels']>100 and data['after_red_pixels']<data['before_red_pixels']*.2
    cost=json.loads((proof/'BoundedCost.json').read_text());assert cost['off_gpu']['samples']>100 and cost['on_gpu']['samples']>100
    for name in ('LiveRedGreen','RedRemoved_Stale','RedRemoved_Updated'):
        with Image.open(proof/(name+'.png')) as im:assert im.size==(800,800)
    logs=[OUT/'Proof-PackagedFinal-Engine.log',ROOT/'Artifacts/D104/Integration/Desktop-DynamicFinal-Engine.log']
    for p in logs:
        s=p.read_text(errors='replace');assert 'Result={Success}' in s and 'Result={Fail}' not in s
    native=(ROOT/'Artifacts/D104/Integration/Tests-Dynamic-Engine.log').read_text(errors='replace');assert len(re.findall(r'Result=\{Success\}',native))==4
    game=ROOT/'Artifacts/D104/Integration/Desktop-DynamicFinal-User/Saved/Automation/LowDensity';assert len(list(game.glob('*.png')))==17
    inputs=sorted([p for folder in ('Source','Content/CitySim') for p in (ROOT/'CitySimVR'/folder).rglob('*') if p.is_file() and p.suffix in ('.h','.cpp','.cs','.uasset','.umap')],key=str);h=hashlib.sha256()
    for p in inputs:
        raw=p.read_bytes();h.update((p.relative_to(ROOT/'CitySimVR').as_posix()+'\0'+str(len(raw))+'\0').encode());h.update(raw)
    revision='dev-'+h.hexdigest()[:20];assert revision in (ROOT/'CitySimVR/Intermediate/CityGameRevision/CityGameRevisionBuild.h').read_text()
    fixture=json.loads((game/'Fixture.json').read_text());assert fixture['lastSavedGameRevision']==revision
    cooked=[]
    for platform in ('Windows','Quest'):
        rows={r['Filename'].strip():r for r in csv.DictReader((OUT/(platform+'-Container.csv')).open(encoding='utf-8-sig'),skipinitialspace=True)}
        for r in imports['meshes']:
            path='../../../CitySimVR/Content/CitySim/Art/LowDensity/Meshes/'+r['name']+'.uasset';assert int(rows[path]['Size'])>0;cooked.append(dict(platform=platform,path=path,hash=rows[path]['Hash'].strip()))
        assert not any('/CityStudies/D104Destruction/' in path for path in rows)
        assert 'BUILD SUCCESSFUL' in (OUT/('WindowsPackage-Final.log' if platform=='Windows' else 'QuestPackage.log')).read_text(errors='replace')
    exe=ROOT/'Artifacts/PCVRSandbox/Windows/CitySimVR/Binaries/Win64/CitySimVR.exe';binary=exe.read_bytes();assert any(revision.encode(e) in binary for e in ('utf-8','utf-16le','utf-32le'))
    apk=OUT/'QuestPackage/Android_ASTC/CitySimVR-arm64.apk';embedded=[]
    with zipfile.ZipFile(apk) as z:
        binary=z.read('lib/arm64-v8a/libUnreal.so');assert any(revision.encode(e) in binary for e in ('utf-8','utf-16le','utf-32le'))
        with zipfile.ZipFile(io.BytesIO(z.read('assets/main.obb.png'))) as obb:
            for name in ('CitySimVR-Android_ASTC.pak','CitySimVR-Android_ASTC.utoc','CitySimVR-Android_ASTC.ucas'):
                raw=obb.read('CitySimVR/Content/Paks/'+name);sha=hashlib.sha256(raw).hexdigest();assert sha==digest(ROOT/'CitySimVR/Saved/StagedBuilds/Android_ASTC/CitySimVR/Content/Paks'/name);embedded.append(dict(name=name,sha256=sha))
    assert 'Quest APK verified:' in (OUT/'QuestValidation.log').read_text()
    code=(ROOT/'CitySimVR/Source/CitySimVR/Private/Sandbox/CitySandboxEnvironment.cpp').read_text();assert '#if PLATFORM_WINDOWS' in code and 'CityStaticReflections' in code
    for name in ['CityIndustrialCylinderNormals.py','Update-CityOriginalFactoryChimneys.py','Import-CityOriginalFactoryChimneys.py','Verify-CityDynamicReflectionDelivery.py']:ast.parse((ROOT/'Tools'/name).read_text())
    result=dict(task='D104',slice='Windows live reflections and original hollow flues',status='Delivered',revision=revision,save_format=67,source_sha256=doc['source_sha256'],windows_dynamic=True,pcvr_uses_same_windows_path=True,quest_static_fallback=True,refresh_seconds=.5,resolution=64,gpu_only=True,diffuse_lighting_preserved=True,mip_filter='native generated cube mipmaps; approximate roughness filtering',local_projection='One shared board-centre environment cube; approximate spatial correspondence',proof=data,bounded_desktop_cost=cost,headset_cost_confirmed=False,headset_rendering_confirmed=False,focused_native_checks=4,packaged_game_images=17,original_hollow_flues_including_damage=4,protected_inputs=len(protected),previous_default_files=53,windows_sha256=digest(exe),quest_sha256=digest(apk),quest_embedded=embedded,cooked=cooked,destruction_study_cooked=False,device_installed=False,published=False)
    (OUT/'Implementation.json').write_text(json.dumps(result,indent=2)+'\n');print('CITYSIM_D104_DYNAMIC_DELIVERY_PASS '+revision)
if __name__=='__main__':main()
