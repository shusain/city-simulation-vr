"""Append four verified driving/idle sources without replacing any accepted D58 WAV."""
from pathlib import Path
import hashlib
import importlib.util
import json
import shutil

ROOT = Path(__file__).resolve().parents[1]
STAGE = ROOT / 'AudioSource/Soundscape/Unreal'
PACK = ROOT / 'Artifacts/D58/Traffic-20261012-v1'
OUT = ROOT / 'Artifacts/D58/TrafficAlarmRefinement'
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
spec = importlib.util.spec_from_file_location('stage',ROOT/'Tools/Stage-CityAudioSources.py')
stage = importlib.util.module_from_spec(spec); spec.loader.exec_module(stage)
manifest = json.loads((STAGE/'Manifest.json').read_text())
assert len(manifest['assets']) == 50, 'Run once; preserve existing stage'
for e in manifest['assets']:
    assert sha(STAGE/e['file']) == e['sha256']
assert json.loads((PACK/'IndependentVerification.json').read_text())['status'] == 'Pass'
source = json.loads((PACK/'Manifest.json').read_text())
for identity, record in source['assets'].items():
    path = PACK/record['runtime']; assert sha(path) == record['runtimeSha256']
    stage.pcm_header(path, record['frames'])
    relative = 'Waves/Ambience/City/'+path.name
    assert not (STAGE/relative).exists()
    shutil.copy2(path,STAGE/relative)
    manifest['assets'].append(dict(id=identity,bank='Traffic',file=relative,sourcePack=PACK.relative_to(ROOT).as_posix(),
        sourceRuntimeFile=record['runtime'],sha256=record['runtimeSha256'],bytes=path.stat().st_size,
        assetName=path.stem,package='/Game/CitySim/Audio/Soundscape/Ambience/City/'+path.stem,
        sampleRate=48000,channels=1,bits=16,frames=record['frames'],seconds=20,loop=True,
        integratedLUFS=record['integratedLUFS'],truePeakDBTP=record['truePeakDBTP'],assetVolume=1.0,assetPitch=1.0,
        loopStartFrame=0,loopEndFrameExclusive=record['frames']))
manifest['sourceWAVBytes'] = sum(e['bytes'] for e in manifest['assets'])
manifest['trafficRevision'] = dict(date='2026-10-12',preservedWaveCount=50,newSteadyLoops=4,
    retiredRuntimeCues=['Car-Pass','Truck-Pass'],originalPassingSourcesRetained=True,
    manifestSha256=sha(PACK/'Manifest.json'),independentVerificationSha256=sha(PACK/'IndependentVerification.json'))
snapshot=STAGE/'Acceptance/traffic-presets.json';shutil.copy2(ROOT/'AudioSource/Soundscape/traffic-presets.json',snapshot)
manifest['acceptanceSnapshotSha256'][snapshot.relative_to(STAGE).as_posix()] = sha(snapshot)
(STAGE/'Manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
stage.validate_stage()
p=STAGE/'Handoff.json';h=json.loads(p.read_text());h['trafficAlarmRefinement20261012']=dict(
    sourcePack=PACK.relative_to(ROOT).as_posix(),sourceCount=54,preservedSources=50,
    newSteadyLoops=list(source['assets']),physicalListening='Pending',
    policy='Two actual vehicle emitters; Unreal smoothed Doppler and spatialization; no prerecorded pass')
p.write_text(json.dumps(h,indent=2)+'\n')
(OUT/'TrafficStage.json').write_text(json.dumps(dict(status='Pass',newSources=list(source['assets']),preservedSources=50),indent=2))
print('D58_TRAFFIC_STAGE_PASS 4 new,50 preserved')
