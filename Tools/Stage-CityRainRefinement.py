"""Guarded four-wave source revision; preserve46 accepted siren/other library WAVs."""
from pathlib import Path
import json,hashlib,shutil,importlib.util
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D58/RainSirenRefinement'
STAGE=ROOT/'AudioSource/Soundscape/Unreal';PACK=ROOT/'Artifacts/D58/Library-20261012-v8'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
spec=importlib.util.spec_from_file_location('stage',ROOT/'Tools/Stage-CityAudioSources.py')
stage=importlib.util.module_from_spec(spec);spec.loader.exec_module(stage)
manifest=stage.validate_stage()
source=json.loads((PACK/'Manifest.json').read_text())
assert json.loads((PACK/'IndependentVerification.json').read_text())['status']=='Pass'
selected={'Rain-Light','Rain-Heavy','Rain-Moderate','Rain-Downpour'};changes=[]
for entry in manifest['assets']:
    if entry['id'] not in selected:continue
    old=entry['sha256'];record=source['assets'][entry['id']];path=PACK/record['runtime']
    assert sha(path)==record['runtimeSha256'] and record['frames']==entry['frames']
    entry.update(sha256=record['runtimeSha256'],sourcePack=PACK.relative_to(ROOT).as_posix(),
        sourceRuntimeFile=record['runtime'],integratedLUFS=record['integratedLUFS'],truePeakDBTP=record['truePeakDBTP'])
    changes.append(dict(id=entry['id'],package=entry['package'],old_sha256=old,new_sha256=entry['sha256']))
    shutil.copy2(path,STAGE/entry['file'])
manifest['rainRevision']=dict(date='2026-10-12',changes=changes,preservedWaveCount=46,
    libraryManifestSha256=sha(PACK/'Manifest.json'),libraryIndependentSha256=sha(PACK/'IndependentVerification.json'))
for name in ('selections.json','library-presets.json'):
    path=STAGE/'Acceptance'/name;shutil.copy2(ROOT/'AudioSource/Soundscape'/name,path)
    manifest['acceptanceSnapshotSha256'][path.relative_to(STAGE).as_posix()]=sha(path)
(STAGE/'Manifest.json').write_text(json.dumps(manifest,indent=2)+'\n',encoding='utf-8')
stage.validate_stage()
handoff_path=STAGE/'Handoff.json';handoff=json.loads(handoff_path.read_text(encoding='utf-8'))
handoff['rainRefinement20261012']=dict(scope='Four continuous/rounded rain beds replace the reviewed crackly runtime texture; other46 WAVs preserved',
    library=PACK.relative_to(ROOT).as_posix(),assetManifestSha256=sha(STAGE/'Manifest.json'),acceptedSelectionsSha256=sha(ROOT/'AudioSource/Soundscape/selections.json'),
    physicalListening='Pending',sourceChanges=changes)
handoff_path.write_text(json.dumps(handoff,indent=2)+'\n',encoding='utf-8')
(OUT/'RainStage.json').write_text(json.dumps(dict(changes=changes,other46_sources_unchanged=True),indent=2),encoding='utf-8')
print('D58_RAIN_SOURCE_STAGE_PASS 4 revised,46 preserved')
