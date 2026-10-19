"""Verify all approved music headers and the APK's actual embedded containers."""
import argparse
import csv
import hashlib
import io
import json
from pathlib import Path
import zipfile

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/"Artifacts/D82/ApprovedIntegration"

def sha(path):
    digest=hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda:stream.read(1024*1024),b""):digest.update(chunk)
    return digest.hexdigest()

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--windows",type=Path,required=True)
    parser.add_argument("--quest",type=Path,required=True)
    args=parser.parse_args()
    manifest=json.loads((ROOT/"AudioSource/Music/CitySoundtrack/UnrealApproved/Manifest.json").read_text())
    imported=json.loads((OUT/"ImportReport.json").read_text())
    assert len(imported["assets"])==len(manifest["assets"])==9 and imported["status"]=="Pass"
    assets=[]
    for platform in ("Windows","Quest"):
        with (OUT/(platform+"-Container.csv")).open(newline="",encoding="utf-8-sig") as stream:
            rows=list(csv.DictReader(stream,skipinitialspace=True))
        for entry in manifest["assets"]:
            filename='../../../CitySimVR/Content/'+entry["package"].removeprefix('/Game/')+'.uasset'
            matching=[r for r in rows if r['Filename'].strip().replace('\\','/')==filename]
            assert len(matching)==1 and int(matching[0]['Size'])>0,(platform,entry['id'])
            native=ROOT/'CitySimVR/Content'/(entry['package'].removeprefix('/Game/')+'.uasset')
            receipt=next(r for r in imported['assets'] if r['id']==entry['id'])
            assert sha(native)==receipt['nativeSha256'] and entry['sha256']==receipt['sourceSha256']
            assets.append({'platform':platform,'id':entry['id'],'filename':filename,'bytes':int(matching[0]['Size']),'cookedHash':matching[0]['Hash'].strip(),'nativeSha256':receipt['nativeSha256'],'approvedPreviewSha256':entry['approvedPreviewSha256']})
    exe=args.windows/'CitySimVR/Binaries/Win64/CitySimVR.exe'
    if not exe.exists():exe=args.windows/'Windows/CitySimVR/Binaries/Win64/CitySimVR.exe'
    apk=args.quest/'Android_ASTC/CitySimVR-arm64.apk'
    staged=ROOT/'CitySimVR/Saved/StagedBuilds/Android_ASTC/CitySimVR/Content/Paks'
    embedded=[]
    with zipfile.ZipFile(apk) as package:
        with zipfile.ZipFile(io.BytesIO(package.read('assets/main.obb.png'))) as obb:
            for filename in ('CitySimVR-Android_ASTC.pak','CitySimVR-Android_ASTC.utoc','CitySimVR-Android_ASTC.ucas','global.utoc','global.ucas'):
                with obb.open('CitySimVR/Content/Paks/'+filename) as stream:
                    digest=hashlib.sha256()
                    for chunk in iter(lambda:stream.read(1024*1024),b''):digest.update(chunk)
                assert digest.hexdigest()==sha(staged/filename)
                embedded.append({'file':filename,'sha256':digest.hexdigest(),'bytes':(staged/filename).stat().st_size})
    windows_paks=exe.parents[2]/'Content/Paks'
    containers=[{'path':str(p),'sha256':sha(p),'bytes':p.stat().st_size} for p in sorted(windows_paks.glob('*')) if p.suffix in ('.pak','.utoc','.ucas')]
    assert {Path(e['path']).name for e in containers}=={'CitySimVR-Windows.pak','CitySimVR-Windows.utoc','CitySimVR-Windows.ucas','global.utoc','global.ucas'}
    result={'task':'D82','date':'2026-10-13','status':'Pass','approvedAssets':assets,'windowsExecutable':str(exe),'windowsExecutableSha256':sha(exe),'windowsContainers':containers,'questApk':str(apk),'questApkSha256':sha(apk),'questEmbeddedContainers':embedded,'scope':'Nine approved native sources referenced/cooked on both platforms; actual APK payload equals staged containers; physical headset listening pending','deviceInstalled':False}
    (OUT/'PackagedVerification.json').write_text(json.dumps(result,indent=2))
    print('D82_APPROVED_PACKAGE_PASS nine Windows/nine Quest headers; exact embedded payload')

if __name__=='__main__':main()
