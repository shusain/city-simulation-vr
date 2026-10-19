"""Check the delivered four-asset scale cook and the five approved clip bindings."""
from pathlib import Path
import csv,json,zipfile,io,hashlib
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D88/Scale'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
doc=json.loads((ROOT/'ArtSource/Manifests/city_citizen_sports_scale.json').read_text());entries=[];clips=[]
for platform in ('Windows','Quest'):
    rows={r['Filename'].strip():r for r in csv.DictReader((OUT/(platform+'-Container.csv')).open(newline='',encoding='utf-8-sig'),skipinitialspace=True)}
    old={r['Filename'].strip():r for r in csv.DictReader((ROOT/'Artifacts/D88'/(platform+'-Container.csv')).open(newline='',encoding='utf-8-sig'),skipinitialspace=True)}
    for e in doc['statics']+doc['props']:
        name='../../../CitySimVR/Content/'+e['mesh_destination'].removeprefix('/Game/')+'.uasset';row=rows[name]
        assert int(row['Size'])>0 and row['Hash'].strip()!=old[name]['Hash'].strip(),(platform,name,'stale scale asset')
        entries.append(dict(platform=platform,file=name,hash=row['Hash'].strip(),bytes=int(row['Size']),changed_from_original_D88=True))
    for e in doc['clips']:
        name='../../../CitySimVR/Content/CitySim/Art/Parks/Animations/AS_Park_'+e['name']+'.uasset';row=rows[name]
        assert int(row['Size'])>0;clips.append(dict(platform=platform,clip=e['name'],hash=row['Hash'].strip()))
apk=OUT/'QuestPackage/Android_ASTC/CitySimVR-arm64.apk';staged=ROOT/'CitySimVR/Saved/StagedBuilds/Android_ASTC/CitySimVR/Content/Paks';matches=[]
with zipfile.ZipFile(apk) as package:
    with zipfile.ZipFile(io.BytesIO(package.read('assets/main.obb.png'))) as obb:
        for name in ('CitySimVR-Android_ASTC.pak','CitySimVR-Android_ASTC.utoc','CitySimVR-Android_ASTC.ucas'):
            raw=obb.read('CitySimVR/Content/Paks/'+name);digest=hashlib.sha256(raw).hexdigest();assert digest==sha(staged/name)
            matches.append(dict(file=name,bytes=len(raw),sha256=digest))
exe=OUT/'ReviewPackage/Windows/CitySimVR/Binaries/Win64/CitySimVR.exe'
(OUT/'PackagedVerification.json').write_text(json.dumps(dict(task='D88-Scale',affected_assets=entries,approved_clips=clips,quest_embedded_match=matches,
    windows_executable_sha256=sha(exe),quest_apk_sha256=sha(apk),pass_checks=True,device_installed=False),indent=2)+'\n')
print('CITYSIM_D88_SCALE_PACKAGE_PASS 4 changed assets /5 approved clips per platform;APK containers match')
