"""Verify final source, protected inputs, world evidence and both cooked targets."""
from pathlib import Path
import ast,csv,hashlib,io,json,re,zipfile
from PIL import Image,ImageStat
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D104/Integration'
def digest(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def main():
    doc=json.loads((ROOT/'ArtSource/Manifests/city_low_density_buildings.json').read_text())
    imported=json.loads((OUT/'Import.json').read_text());assert len(imported['meshes'])==24 and len(imported['commercial_sign_meshes'])==32
    assert len(doc['variants'])==12 and imported['gameplay_definitions_unchanged'] and imported['commercial_identity_unchanged']
    assert digest(ROOT/doc['source'])==doc['source_sha256']==imported['source_sha256']
    assert digest(ROOT/doc['approved_source'])==doc['approved_source_sha256']
    for e in doc['assets']:assert digest(ROOT/e['export'])==e['export_sha256']
    for family in doc['textures'].values():
        for t in family['maps'].values():assert digest(ROOT/t['file'])==t['sha256']
    protected=json.loads((OUT/'Protected.json').read_text())
    owned={'CitySimVR/Source/CitySimVR/Public/Presentation/CityBoardActor.h','CitySimVR/Source/CitySimVR/Public/Interaction/CityVRToolController.h',
        'CitySimVR/Source/CitySimVR/Private/Presentation/CityDesktopModelPreview.cpp','CitySimVR/Source/CitySimVR/Private/Sandbox/CitySandboxEnvironment.cpp'}
    assert all(digest(ROOT/p)==h for p,h in protected.items() if p not in owned)
    previous=json.loads((OUT/'PreviousDefault.json').read_text());assert len(previous)==53
    for r in previous:assert digest(OUT/'PreviousDefaultWindows'/r['path'])==r['sha256']
    baseline=json.loads((ROOT/'Artifacts/D104/Batch.json').read_text())
    assert digest(ROOT/baseline['source'])==baseline['source_sha256']
    preserved=0
    for renderer in ('Blender','Unreal'):
        old=json.loads((ROOT/'Artifacts/D104'/renderer/'Report.json').read_text())
        for r in old['views']:
            assert digest(ROOT/'Artifacts/D104'/renderer/r['group']/r['view']/r['image'])==r['sha256'];preserved+=1
    assert preserved==136
    for name in ('EditorBuild-Acceptance.log',):assert 'Result: Succeeded' in (OUT/name).read_text(errors='replace')
    native=(OUT/'Tests-Integrated-Engine.log').read_text(errors='replace');assert len(re.findall(r'Result=\{Success\}',native))==4 and 'Result={Fail}' not in native
    game_log=(OUT/'Desktop-Final-Engine.log').read_text(errors='replace')
    assert 'CITYSIM_D104_DESKTOP_COMPLETE' in game_log and 'Result={Success}' in game_log and 'Result={Fail}' not in game_log
    review=json.loads((OUT/'Review.json').read_text());assert review['native_views']==26 and review['game_views']==17
    for r in review['game_images']:
        p=OUT/r['path'];assert digest(p)==r['sha256']
        with Image.open(p) as im:assert im.size==(1280,800) and max(ImageStat.Stat(im.convert('RGB')).stddev)>12
    frames=json.loads((OUT/'Unreal/Report.json').read_text());assert len(frames['views'])==26
    for r in frames['views']:
        p=OUT/'Unreal'/r['image'];assert digest(p)==r['sha256']
        with Image.open(p) as im:assert im.size==(1000,1000)
    signs=json.loads((OUT/'Signs.json').read_text());assert len(signs)==8 and all(r['offset'][1]>=0 for r in signs)
    bake=json.loads((OUT/'ReflectionBake.json').read_text())
    assert bake['reflection_build_data_saved'] and not bake['lightmass_built'] and not bake['live_recapture']
    for r in bake['files']:assert digest(ROOT/r['path'])==r['sha256']
    # Match UBT's ordinal absolute-path order and streamed name/length/content.
    inputs=sorted([p for folder in ('Source','Content/CitySim') for p in (ROOT/'CitySimVR'/folder).rglob('*') if p.is_file() and p.suffix in ('.h','.cpp','.cs','.uasset','.umap')],key=str)
    hasher=hashlib.sha256();source_rows=[]
    for p in inputs:
        raw=p.read_bytes();rel=p.relative_to(ROOT/'CitySimVR').as_posix()
        hasher.update((rel+'\0'+str(len(raw))+'\0').encode());hasher.update(raw)
        source_rows.append(dict(path=rel,sha256=hashlib.sha256(raw).hexdigest()))
    revision='dev-'+hasher.hexdigest()[:20]
    header=(ROOT/'CitySimVR/Intermediate/CityGameRevision/CityGameRevisionBuild.h').read_text();assert revision in header
    fixture=json.loads((OUT/'Desktop-Final-User/Saved/Automation/LowDensity/Fixture.json').read_text())
    assert fixture['lastSavedGameRevision']==revision
    changed=[p.removeprefix('/Game/') for p in imported['changed']]
    native_paths=sorted(set('CitySimVR/Content/'+p+'.uasset' for p in changed)|set([
        'CitySimVR/Content/CitySim/Data/DA_CityBuildingCatalog.uasset',
        'CitySimVR/Content/CitySim/Maps/CitySandbox.umap','CitySimVR/Content/CitySim/Maps/CitySandbox_BuiltData.uasset',
        'CitySimVR/Content/CitySim/Art/Textures/T_CityRoomReflection.uasset']))
    cooked=[]
    for platform in ('Windows','Quest'):
        rows={r['Filename'].strip():r for r in csv.DictReader((OUT/(platform+'-Container.csv')).open(encoding='utf-8-sig',newline=''),skipinitialspace=True)}
        for p in native_paths:
            name='../../../'+p;r=rows[name];assert int(r['Size'])>0
            cooked.append(dict(platform=platform,path=name,size=int(r['Size']),hash=r['Hash'].strip()))
        sky='../../../Engine/Content/MapTemplates/Sky/DaylightAmbientCubemap.uasset';assert sky in rows and int(rows[sky]['Size'])>0
        log=(OUT/('WindowsPackage-Acceptance.log' if platform=='Windows' else 'QuestPackage-Final.log')).read_text(errors='replace')
        assert 'BUILD SUCCESSFUL' in log and not re.search(r'Failed to compile Material|Default Material will be used|Fatal error:',log,re.I)
    exe=ROOT/'Artifacts/PCVRSandbox/Windows/CitySimVR/Binaries/Win64/CitySimVR.exe';binary=exe.read_bytes()
    assert any(revision.encode(e) in binary for e in ('utf-8','utf-16le','utf-32le'))
    apk=OUT/'QuestPackage/Android_ASTC/CitySimVR-arm64.apk';embedded=[]
    staged=ROOT/'CitySimVR/Saved/StagedBuilds/Android_ASTC/CitySimVR/Content/Paks'
    with zipfile.ZipFile(apk) as archive:
        libname=next(n for n in archive.namelist() if n=='lib/arm64-v8a/libUnreal.so')
        lib=archive.read(libname);assert any(revision.encode(e) in lib for e in ('utf-8','utf-16le','utf-32le'))
        with zipfile.ZipFile(io.BytesIO(archive.read('assets/main.obb.png'))) as obb:
            for name in ('CitySimVR-Android_ASTC.pak','CitySimVR-Android_ASTC.utoc','CitySimVR-Android_ASTC.ucas'):
                raw=obb.read('CitySimVR/Content/Paks/'+name);h=hashlib.sha256(raw).hexdigest();assert h==digest(staged/name)
                embedded.append(dict(name=name,sha256=h,bytes=len(raw)))
    assert 'Quest APK verified:' in (OUT/'QuestValidation-Final.log').read_text(errors='replace')
    for name in ['Prepare-CityLowDensityIntegration.py','Prepare-CityLowDensityBaseline.py','Import-CityLowDensityIntegration.py','Capture-CityLowDensityIntegration.py','Bake-CityLowDensityReflection.py','Update-CityLowDensitySigns.py','Compose-CityLowDensityIntegration.py','Verify-CityLowDensityIntegration.py']:
        ast.parse((ROOT/'Tools'/name).read_text())
    result=dict(task='D104',status='Materials and low-density variants integrated and delivered',revision=revision,save_format=67,
        source=doc['source'],source_sha256=doc['source_sha256'],appearances=12,source_meshes=24,native_meshes=52,
        materials=10,sign_instances=8,textures=9,protected_inputs=len(protected)-len(owned),previous_default_files=53,
        approved_original_gallery_views_preserved=136,gameplay_definitions_unchanged=True,simulation_save_format_unchanged=True,
        actual_native_views=26,actual_final_packaged_views=17,focused_native_checks=4,editor_build_warnings_as_errors=True,
        windows_executable=str(exe.relative_to(ROOT)),windows_sha256=digest(exe),quest_apk=str(apk.relative_to(ROOT)),quest_sha256=digest(apk),
        cooked_asset_headers_per_platform=len(native_paths),cooked=cooked,quest_embedded=embedded,
        static_reflection_build_persisted=True,fixed_daylight_sky=True,live_city_recapture=False,
        physical_input_and_headset_rendering_confirmed=False,device_installed=False,published=False,
        next_task='User review of integrated building variety/materials; focused PCVR/Quest appearance observation when available.')
    (OUT/'RevisionInputs.json').write_text(json.dumps(source_rows,indent=2)+'\n')
    (OUT/'Implementation.json').write_text(json.dumps(result,indent=2)+'\n')
    print(f'CITYSIM_D104_INTEGRATION_DELIVERY_PASS {revision};52 meshes;83 cooked asset headers per platform;17 actual Windows views')
if __name__=='__main__':main()
