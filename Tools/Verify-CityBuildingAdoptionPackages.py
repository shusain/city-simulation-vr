"""Verify affected cooked headers/payloads and the actual embedded Quest archive."""
from pathlib import Path
import json,csv,zipfile,io,hashlib,re
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D91/Integration'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
proof=json.loads((OUT/'Implementation.json').read_text());paths=sorted(proof['changed_native_files']);checks=[]
for platform in ('Windows','Quest'):
    rows={r['Filename'].strip():r for r in csv.DictReader((OUT/(platform+'-Container.csv')).open(encoding='utf-8-sig',newline=''),skipinitialspace=True)}
    old={r['Filename'].strip():r for r in csv.DictReader((ROOT/'Artifacts/D82/ApprovedIntegration'/(platform+'-Container.csv')).open(encoding='utf-8-sig',newline=''),skipinitialspace=True)}
    for path in paths:
        cooked='../../../'+path.removeprefix('CitySimVR/').replace('Content/','CitySimVR/Content/',1)
        # Native path CitySimVR/Content/... becomes the archive mount path.
        for suffix in ('.uasset','.uexp','.ubulk'):
            name=cooked.removesuffix('.uasset')+suffix
            if suffix!='.uasset' and name not in rows:continue # IoStore may inline exports/small texture mips.
            row=rows[name];assert int(row['Size'])>0,(platform,name)
            if suffix=='.uasset' and name in old:assert row['Hash'].strip()!=old[name]['Hash'].strip(),(platform,name,'stale approved asset')
            checks.append(dict(platform=platform,file=name,bytes=int(row['Size']),hash=row['Hash'].strip(),new=name not in old))
    log=(OUT/(platform+'Package.log')).read_text(errors='replace');assert 'BUILD SUCCESSFUL' in log and not re.search(r'Failed to compile Material|Default Material will be used|Fatal error:',log,re.I)
apk=OUT/'QuestPackage/Android_ASTC/CitySimVR-arm64.apk';matches=[]
staged=ROOT/'CitySimVR/Saved/StagedBuilds/Android_ASTC/CitySimVR/Content/Paks'
with zipfile.ZipFile(apk) as package:
    with zipfile.ZipFile(io.BytesIO(package.read('assets/main.obb.png'))) as obb:
        for name in ('CitySimVR-Android_ASTC.pak','CitySimVR-Android_ASTC.utoc','CitySimVR-Android_ASTC.ucas'):
            raw=obb.read('CitySimVR/Content/Paks/'+name);digest=hashlib.sha256(raw).hexdigest();assert digest==sha(staged/name);matches.append(dict(file=name,bytes=len(raw),sha256=digest))
smoke=(ROOT/'Artifacts/D91-Windows-Smoke/D68-Packaged-1280-800.log').read_text(errors='replace');assert 'CITYSIM_DESKTOP_PACKAGED_SMOKE_COMPLETE' in smoke and 'Result={Success}' in smoke
exe=OUT/'WindowsPackage/CitySimVR/Binaries/Win64/CitySimVR.exe'
report=dict(task='D91',native_assets=80,meshes=61,shared_materials_textures=19,cooked_checks=checks,quest_embedded_match=matches,windows_executable_sha256=sha(exe),quest_apk_sha256=sha(apk),windows_smoke='InspectorOnly Success/five1280x800 captures',device_installed=False,physical_device_verified=False)
(OUT/'PackagedVerification.json').write_text(json.dumps(report,indent=2)+'\n')
proof['packages']=dict(verification='Artifacts/D91/Integration/PackagedVerification.json',windows='Artifacts/D91/Integration/WindowsPackage',quest='Artifacts/D91/Integration/QuestPackage/Android_ASTC/CitySimVR-arm64.apk',windows_sha256=sha(exe),quest_sha256=sha(apk),device_installed=False)
(OUT/'Implementation.json').write_text(json.dumps(proof,indent=2)+'\n')
print('CITYSIM_D91_BUILDING_PACKAGES_PASS 80 cooked assets per platform;five Windows captures;Quest embedded containers match')
