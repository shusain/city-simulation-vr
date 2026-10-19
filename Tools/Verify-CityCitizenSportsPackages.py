"""D88 verify affected cooked assets and the signed APK's actual containers."""
from pathlib import Path
import csv,json,zipfile,io,hashlib
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D88'
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
names=[f'Animations/AS_Park_{n}.uasset' for n in ('BaseballPitch','BaseballSwing','SoccerKick','SoccerRun','GoalkeeperSave')]
names += [f'Meshes/{prefix}_Park_Field_{style}.uasset' for prefix in ('SK','SM') for style in ('Casual','Office','Worker')]
entries=[]
for platform in ('Windows','Quest'):
    rows=list(csv.DictReader((OUT/(platform+'-Container.csv')).open(newline='',encoding='utf-8-sig'),skipinitialspace=True))
    before=list(csv.DictReader((ROOT/'Artifacts/D82'/(platform+'-Container.csv')).open(newline='',encoding='utf-8-sig'),skipinitialspace=True))
    old={row['Filename'].strip():row for row in before}
    for name in names:
        path='../../../CitySimVR/Content/CitySim/Art/Parks/'+name
        row=next(r for r in rows if r['Filename'].strip()==path)
        assert int(row['Size'])>0
        assert row['Hash'].strip()!=old[path]['Hash'].strip(),(platform,name,'stale affected cooked asset')
        entries.append(dict(platform=platform,file=path,bytes=int(row['Size']),hash=row['Hash'].strip(),changed_from_D82=True))
apk=ROOT/'Artifacts/D88/QuestPackage/Android_ASTC/CitySimVR-arm64.apk'
staged=ROOT/'CitySimVR/Saved/StagedBuilds/Android_ASTC/CitySimVR/Content/Paks'
matches=[]
with zipfile.ZipFile(apk) as package:
    with zipfile.ZipFile(io.BytesIO(package.read('assets/main.obb.png'))) as obb:
        for name in ('CitySimVR-Android_ASTC.pak','CitySimVR-Android_ASTC.utoc','CitySimVR-Android_ASTC.ucas'):
            raw=obb.read('CitySimVR/Content/Paks/'+name);digest=hashlib.sha256(raw).hexdigest()
            assert digest==sha(staged/name),(name,'APK differs from verified staged payload')
            matches.append(dict(file=name,bytes=len(raw),sha256=digest))
exe=ROOT/'Artifacts/D88/ReviewPackage/Windows/CitySimVR/Binaries/Win64/CitySimVR.exe'
(OUT/'PackagedVerification.json').write_text(json.dumps(dict(task='D88',affected_assets=entries,quest_embedded_match=matches,
    windows_executable_sha256=sha(exe),quest_apk_sha256=sha(apk),pass_checks=True,device_installed=False),indent=2)+'\n')
print('CITYSIM_D88_PACKAGE_CONTENT_PASS 11 changed affected assets per platform;APK embedded containers match')
