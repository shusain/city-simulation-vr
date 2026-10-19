"""Verify protected D58 follow-up inputs; permit only scoped rain/audio edits."""
from pathlib import Path
import json,hashlib
ROOT=Path(__file__).resolve().parents[1];OUT=ROOT/'Artifacts/D58/RainSirenRefinement'
baseline=json.loads((OUT/'ProtectedBefore.json').read_text())
allowed={'CitySimVR/Source/CitySimVR/'+p for p in (
    'Private/Presentation/CitySoundscapeComponent.cpp','Public/Presentation/CitySoundscapeComponent.h',
    'Public/Presentation/CitySoundscapePolicy.h','Private/Tests/CitySoundscapeTests.cpp')}
allowed|={'AudioSource/Soundscape/'+p for p in ('SampleLibrary.scd','library-presets.json','selections.json',
    'Unreal/Manifest.json','Unreal/Handoff.json','Unreal/IntegrationReceipt.json',
    'Unreal/Acceptance/library-presets.json','Unreal/Acceptance/selections.json')}
manifest=json.loads((ROOT/'AudioSource/Soundscape/Unreal/Manifest.json').read_text())
for e in manifest['assets']:
    if e['id'].startswith('Rain-'):
        allowed.add('AudioSource/Soundscape/Unreal/'+e['file'])
        allowed.add('CitySimVR/Content/'+e['package'].removeprefix('/Game/')+'.uasset')
changed=[]
for relative,digest in baseline.items():
    p=ROOT/relative;assert p.exists(),relative
    actual=hashlib.sha256(p.read_bytes()).hexdigest()
    if actual!=digest:
        assert relative in allowed,('Unexpected protected change',relative)
        changed.append(dict(path=relative,before=digest,after=actual))
result=dict(status='Pass',protected=len(baseline),declared_changes=changed,
    accepted_art_and_siren_sources_preserved=True,authoritative_city_files_unchanged=True)
(OUT/'Preservation.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
print('D58_RAIN_SIREN_PRESERVATION_PASS',len(baseline),len(changed))
