"""Consolidate completed D58 traffic/alarm local gates; never infer physical acceptance."""
from pathlib import Path
import hashlib
import json
import re

ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'Artifacts/D58/TrafficAlarmRefinement'
read=lambda p:json.loads(p.read_text(encoding='utf-8-sig'))
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
candidate='integration-2026.10.12.2'
for name in ('Import.json','Graphs.json','DecodedVerification.json','Preservation.json','SteadyStemCheck.json',
             'RuntimePlayback/Runtime.json','EventPlayback/Events.json'):
    assert read(OUT/name)['status']=='Pass',name
audio=(OUT/'Audio.log').read_text(encoding='utf-8-sig')
full=(OUT/'Full.log').read_text(encoding='utf-8-sig')
assert len(re.findall(r'Test Completed\. Result=\{Success\}',audio))==6
assert len(re.findall(r'Test Completed\. Result=\{Success\}',full))==257
for log in (audio,full):
    assert not re.search(r'Test Completed\. Result=\{Fail|Assertion failed|Ensure condition failed|Handled ensure|Fatal error|Critical error',log)
assert 'Succeeded' in (OUT/'UBT-CrossfadeOrder.log').read_text(encoding='utf-8-sig')
assert 'passed for 295' in (OUT/'Formatting.log').read_text()
assert len(read(OUT/'RuntimePlayback/Runtime.json')['checks'])==14
assert all(read(OUT/'RuntimePlayback/Runtime.json')['checks'].values())
assert all(read(OUT/'EventPlayback/Events.json')['checks'].values())
assert len(read(OUT/'DecodedVerification.json')['records'])==8
cooked=[]
for platform in ('Windows','Android_ASTC'):
    folder=ROOT/f'CitySimVR/Saved/Cooked/{platform}/CitySimVR/Content/CitySim/Audio/Soundscape'
    paths=list(folder.rglob('*.uasset'));graphs=[p for p in paths if p.parent.name=='Graphs']
    assert len(paths)==108 and len(graphs)==54,(platform,len(paths),len(graphs))
    assert len([p for p in graphs if p.stem.startswith('SC_')])==4
    cooked.append(dict(platform=platform,waves=54,metaSounds=50,trafficSoundCues=4))
package=ROOT/'TesterDistribution/packages'/candidate
packages=[dict(path=p.relative_to(ROOT).as_posix(),bytes=p.stat().st_size,sha256=sha(p))
          for p in sorted(package.iterdir()) if p.suffix in ('.zip','.apk')]
assert len(packages)==2
for name in ('Windows-Package.log','Quest-Package.log'):
    assert 'BUILD SUCCESSFUL' in (OUT/name).read_text(encoding='utf-8-sig'),name
assert '26' in (OUT/'DesktopSmoke.log').read_text(encoding='utf-8-sig')
assert 'Quest APK verified:' in (OUT/'APKCheck.log').read_text(encoding='utf-8-sig')
assert 'Extracted launcher/button/render/save smoke passed; 20 captures' in (OUT/'BundleCheck.log').read_text(encoding='utf-8-sig')
sources={p.relative_to(ROOT).as_posix():sha(p) for p in (ROOT/'CitySimVR/Source/CitySimVR').rglob('*')
         if p.suffix in ('.cpp','.h') and 'Soundscape' in p.name}
report=dict(status='Pass',task='D58 traffic/alarm feedback',candidate=candidate,packages=packages,
            focused=6,renderedRegressions=257,nativeEventChecks=7,nativeLifecycleChecks=14,
            decodedTrafficRecordings=8,repeatedLoops=4,formattingFiles=295,cooked=cooked,
            preservation=read(OUT/'Preservation.json'),sources=sources,
            scope='Local source/native/runtime/Editor/Windows/Quest/launcher checks',
            userAcceptance='Prior rain and isolated sirens sound better in PCVR (2026-10-12)',
            pending=['New traffic tone/mix and physical PCVR/Quest motion/localization',
                     'Remaining physical lifecycle/comfort and representative dense-city/device audio cost'],
            installed=False,committed=False,pushed=False,published=False,
            repairedAttempts=['Native cue graph pins before any new cue was saved',
                              'Routing fixture world-associated audio handle and nearest-first crossfade expectation',
                              'Transient DDC IO startup crash; normal retry passed without cache deletion',
                              'Native verifier legacy-save GUID migration handling'])
(OUT/'Verification.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('D58_TRAFFIC_ALARM_VERIFICATION_PASS',candidate)
