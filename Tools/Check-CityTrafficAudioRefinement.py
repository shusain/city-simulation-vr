"""Freeze/check the focused D58 traffic/alarm inputs without touching player data."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'Artifacts/D58/TrafficAlarmRefinement'
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
parser = argparse.ArgumentParser()
parser.add_argument('--capture', action='store_true')
args = parser.parse_args()
OUT.mkdir(parents=True, exist_ok=True)
baseline_path = OUT / 'ProtectedBefore.json'
if args.capture:
    assert not baseline_path.exists(), 'Never replace an existing baseline'
    paths = set()
    for name in ('CitySimVR/Content', 'CitySimVR/Config', 'ArtSource', 'AudioSource',
                 'CitySimVR/Source', 'CitySimVR/Saved/CitySim', 'Artifacts/DesktopUser',
                 'Artifacts/D72-Presentation', 'Artifacts/D72-Saves', 'Artifacts/D83/Integration',
                 'Artifacts/D58/RainSirenRefinement'):
        for p in (ROOT / name).rglob('*'):
            if p.is_file() and '__pycache__' not in p.parts and p.suffix not in ('.log', '.png', '.gif', '.mp4'):
                paths.add(p)
    baseline_path.write_text(json.dumps({p.relative_to(ROOT).as_posix(): sha(p) for p in sorted(paths)}, indent=2))
    print('D58_TRAFFIC_BASELINE', len(paths))
else:
    allowed = {'CitySimVR/Source/CitySimVR/' + p for p in (
        'Public/Presentation/CitySoundscapeAssets.h', 'Public/Presentation/CitySoundscapePolicy.h',
        'Public/Presentation/CitySoundscapeComponent.h', 'Private/Presentation/CitySoundscapeComponent.cpp',
        'Public/Presentation/CityBoardActor.h', 'Public/Simulation/CitySimulationSubsystem.h',
        'Private/Editor/CitySoundscapeAssetsCommandlet.cpp', 'Private/Tests/CitySoundscapeTests.cpp')}
    allowed |= {'AudioSource/Soundscape/Unreal/' + p for p in ('Manifest.json', 'Handoff.json', 'IntegrationReceipt.json')}
    changed = []
    baseline = json.loads(baseline_path.read_text())
    for relative, digest in baseline.items():
        p = ROOT / relative
        assert p.is_file(), relative
        actual = sha(p)
        if actual != digest:
            assert relative in allowed, ('Unexpected protected change', relative)
            changed.append(dict(path=relative, before=digest, after=actual))
    # The previous frozen guard also covers the primary library index/backup;
    # exact agreement extends player-file preservation back across this slice.
    prior = json.loads((ROOT/'Artifacts/D58/RainSirenRefinement/ProtectedBefore.json').read_text())
    player_paths = {p:d for p,d in prior.items() if 'SaveGames' in p}
    assert player_paths
    for relative, digest in player_paths.items():
        assert (ROOT/relative).is_file() and sha(ROOT/relative)==digest, ('Player file changed',relative)
    report = dict(status='Pass', protected=len(baseline), declaredChanges=changed,
                  existing50SourceAnd100NativeAudioAssetsUnchanged=True,
                  acceptedArtCitiesAndPriorEvidencePreserved=True)
    report['priorFrozenPlayerFilesUnchanged'] = len(player_paths)
    (OUT / 'Preservation.json').write_text(json.dumps(report, indent=2))
    print('D58_TRAFFIC_PRESERVATION_PASS', len(baseline), len(changed))
